#include "OscillatorSource.h"

namespace GlitchDSP
{

OscillatorSource::OscillatorSource(int id, const std::string& sourceName)
    : SoundSource(id, sourceName, SourceType::Oscillator)
{
}

void OscillatorSource::prepare(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    glitchFx.prepare(currentSampleRate);

    for (auto& v : voices)
    {
        v.envelope.prepare(currentSampleRate);
        v.envelope.setAttackMs(envAttackMs.load(std::memory_order_relaxed));
        v.envelope.setHoldMs(envHoldMs.load(std::memory_order_relaxed));
        v.envelope.setDecayMs(envDecayMs.load(std::memory_order_relaxed));
        v.envelope.setSustainLevel(envSustain.load(std::memory_order_relaxed));
        v.envelope.setReleaseMs(envReleaseMs.load(std::memory_order_relaxed));
        v.envelope.setAttackCurve(envCurve.load(std::memory_order_relaxed));
        v.envelope.setDecayCurve(envCurve.load(std::memory_order_relaxed));
        v.envelope.setReleaseCurve(envCurve.load(std::memory_order_relaxed));
        v.active = false;
        v.phase = 0.0;
        v.phaseInc = 0.0;
        v.noteNumber = -1;
    }
}

void OscillatorSource::setAttackMs(float ms) noexcept
{
    envAttackMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setAttackMs(ms);
}

void OscillatorSource::setHoldMs(float ms) noexcept
{
    envHoldMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setHoldMs(ms);
}

void OscillatorSource::setDecayMs(float ms) noexcept
{
    envDecayMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setDecayMs(ms);
}

void OscillatorSource::setSustainLevel(float lvl) noexcept
{
    envSustain.store(lvl, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setSustainLevel(lvl);
}

void OscillatorSource::setReleaseMs(float ms) noexcept
{
    envReleaseMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setReleaseMs(ms);
}

void OscillatorSource::setCurveShape(float shape) noexcept
{
    envCurve.store(shape, std::memory_order_relaxed);
    for (auto& v : voices)
    {
        v.envelope.setAttackCurve(shape);
        v.envelope.setDecayCurve(shape);
        v.envelope.setReleaseCurve(shape);
    }
}

void OscillatorSource::setFrequency(float f) noexcept
{
    float clamped = std::clamp(f, 10.0f, 20000.0f);
    frequency.store(clamped, std::memory_order_relaxed);

    bool pt = pitchTrack.load(std::memory_order_relaxed);
    float semi = pitchSemi.load(std::memory_order_relaxed) + (pitchFine.load(std::memory_order_relaxed) * 0.01f);

    for (auto& v : voices)
    {
        if (v.active)
        {
            float totalSemitones = semi;
            if (pt && v.noteNumber >= 0)
                totalSemitones += static_cast<float>(v.noteNumber - 69);

            double targetFreq = static_cast<double>(clamped) * std::pow(2.0, totalSemitones / 12.0);
            v.phaseInc = targetFreq / currentSampleRate;
        }
    }
}

void OscillatorSource::setPitchTrack(bool pt) noexcept
{
    pitchTrack.store(pt, std::memory_order_relaxed);

    float f = frequency.load(std::memory_order_relaxed);
    float semi = pitchSemi.load(std::memory_order_relaxed) + (pitchFine.load(std::memory_order_relaxed) * 0.01f);

    for (auto& v : voices)
    {
        if (v.active)
        {
            float totalSemitones = semi;
            if (pt && v.noteNumber >= 0)
                totalSemitones += static_cast<float>(v.noteNumber - 69);

            double targetFreq = static_cast<double>(f) * std::pow(2.0, totalSemitones / 12.0);
            v.phaseInc = targetFreq / currentSampleRate;
        }
    }
}

void OscillatorSource::noteOn(int noteNumber, float velocity)
{
    int voiceIdx = -1;

    // 0. Check if this exact note is already actively playing on a voice (retrigger)
    for (int i = 0; i < MAX_VOICES; ++i)
    {
        if (voices[i].active && voices[i].noteNumber == noteNumber)
        {
            voiceIdx = i;
            break;
        }
    }

    // 1. Find inactive voice
    if (voiceIdx == -1)
    {
        for (int i = 0; i < MAX_VOICES; ++i)
        {
            if (!voices[i].active || !voices[i].envelope.isActive())
            {
                voiceIdx = i;
                break;
            }
        }
    }

    // 2. If all busy, steal released voice with lowest envelope level
    if (voiceIdx == -1)
    {
        float minLevel = 1e9f;
        for (int i = 0; i < MAX_VOICES; ++i)
        {
            if (voices[i].envelope.getStage() == FastEnvelope::Stage::Release ||
                voices[i].envelope.getStage() == FastEnvelope::Stage::ChokeRelease)
            {
                float lvl = voices[i].envelope.getCurrentLevel();
                if (lvl < minLevel)
                {
                    minLevel = lvl;
                    voiceIdx = i;
                }
            }
        }
    }

    // 3. If all voices are sustaining (held notes), steal oldest voice (LRU)
    if (voiceIdx == -1)
    {
        uint32_t oldestAge = std::numeric_limits<uint32_t>::max();
        for (int i = 0; i < MAX_VOICES; ++i)
        {
            if (voices[i].age < oldestAge)
            {
                oldestAge = voices[i].age;
                voiceIdx = i;
            }
        }
    }

    if (voiceIdx >= 0)
    {
        auto& v = voices[voiceIdx];
        bool wasActive = v.active && v.envelope.isActive();
        v.noteNumber = noteNumber;
        v.age = nextVoiceAge++;
        v.active = true;

        float baseFreq = frequency.load(std::memory_order_relaxed);
        bool pt = pitchTrack.load(std::memory_order_relaxed);

        float totalSemitones = pitchSemi.load(std::memory_order_relaxed)
                               + (pitchFine.load(std::memory_order_relaxed) * 0.01f);
        if (pt)
        {
            totalSemitones += static_cast<float>(noteNumber - 69);
        }

        double freq = static_cast<double>(baseFreq) * std::pow(2.0, totalSemitones / 12.0);
        v.phaseInc = freq / currentSampleRate;

        // Zero-crossing phase initialization for click-free attack transients
        if (!wasActive || v.envelope.getCurrentLevel() < 0.001f)
        {
            auto wf = getWaveform();
            switch (wf)
            {
                case OscWaveform::Sine:            v.phase = 0.0; break;
                case OscWaveform::Saw:             v.phase = 0.5; break;
                case OscWaveform::Triangle:        v.phase = 0.25; break;
                case OscWaveform::GlitchWavetable: v.phase = 0.0; break;
                case OscWaveform::Square:
                default:                           v.phase = 0.0; break;
            }
        }

        // Apply updated envelope parameters
        v.envelope.setAttackMs(envAttackMs.load(std::memory_order_relaxed));
        v.envelope.setHoldMs(envHoldMs.load(std::memory_order_relaxed));
        v.envelope.setDecayMs(envDecayMs.load(std::memory_order_relaxed));
        v.envelope.setSustainLevel(envSustain.load(std::memory_order_relaxed));
        v.envelope.setReleaseMs(envReleaseMs.load(std::memory_order_relaxed));
        v.envelope.setAttackCurve(envCurve.load(std::memory_order_relaxed));
        v.envelope.setDecayCurve(envCurve.load(std::memory_order_relaxed));
        v.envelope.setReleaseCurve(envCurve.load(std::memory_order_relaxed));

        v.envelope.noteOn(velocity);
    }
}

void OscillatorSource::noteOff(int noteNumber, float /*velocity*/)
{
    for (auto& v : voices)
    {
        if (v.active && (noteNumber < 0 || v.noteNumber == noteNumber))
        {
            v.envelope.noteOff();
        }
    }
}

void OscillatorSource::choke()
{
    for (auto& v : voices)
    {
        if (v.active)
        {
            v.envelope.choke();
        }
    }
}

bool OscillatorSource::isPlaying() const noexcept
{
    for (const auto& v : voices)
    {
        if (v.active)
            return true;
    }
    return false;
}

float OscillatorSource::generateSample(Voice& v, OscWaveform wf, float pw, float morph) noexcept
{
    double t = v.phase;
    double dt = v.phaseInc;
    float raw = 0.0f;

    switch (wf)
    {
        case OscWaveform::Sine:
        {
            raw = static_cast<float>(std::sin(t * 2.0 * juce::MathConstants<double>::pi));
            break;
        }

        case OscWaveform::Saw:
        {
            // PolyBLEP anti-aliased saw
            raw = static_cast<float>((2.0 * t) - 1.0);
            raw -= static_cast<float>(polyBlep(t, dt));
            break;
        }

        case OscWaveform::Square:
        {
            // PolyBLEP pulse with PWM
            raw = (t < pw) ? 1.0f : -1.0f;
            raw += static_cast<float>(polyBlep(t, dt));
            raw -= static_cast<float>(polyBlep(std::fmod(t + (1.0 - pw), 1.0), dt));
            break;
        }

        case OscWaveform::Triangle:
        {
            // Integrated square or direct triangle
            raw = static_cast<float>((t < 0.5) ? (4.0 * t - 1.0) : (3.0 - 4.0 * t));
            break;
        }

        case OscWaveform::GlitchWavetable:
        {
            // Morphable glitch wave: non-linear Chebyshev fold + bit-shuffling
            double baseSine = std::sin(t * 2.0 * juce::MathConstants<double>::pi);
            // Chebyshev T3(x) = 4x^3 - 3x fold
            double fold = 4.0 * (baseSine * baseSine * baseSine) - 3.0 * baseSine;
            // Stepped quantization based on morph
            float steps = 4.0f + (1.0f - morph) * 28.0f;
            double stepped = std::round(fold * steps) / steps;
            raw = static_cast<float>((1.0f - morph) * baseSine + morph * stepped);
            break;
        }
    }

    // Advance phase
    v.phase += dt;
    if (v.phase >= 1.0)
        v.phase -= std::floor(v.phase);

    return raw;
}

void OscillatorSource::processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, double hostBpm, double /*hostPpq*/)
{
    if (getMuted())
        return;

    auto currentWf = getWaveform();
    float pw = getPulseWidth();
    float morph = getGlitchMorph();
    float currentGain = getGain();
    float currentPan = getPan();

    float leftGain = currentGain * std::cos((currentPan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
    float rightGain = currentGain * std::sin((currentPan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);

    int bus = getOutputBus();
    int numChannels = buffer.getNumChannels();
    int chL = bus * 2;
    int chR = bus * 2 + 1;
    if (chR >= numChannels)
    {
        chL = 0;
        chR = std::min(1, numChannels - 1);
    }

    auto* leftOut = buffer.getWritePointer(chL, startSample);
    auto* rightOut = buffer.getWritePointer(chR, startSample);

    for (int s = 0; s < numSamples; ++s)
    {
        float sampleSum = 0.0f;

        for (auto& v : voices)
        {
            if (v.active)
            {
                float env = v.envelope.getNextSample();
                if (!v.envelope.isActive())
                {
                    v.active = false;
                }
                else
                {
                    sampleSum += generateSample(v, currentWf, pw, morph) * env;
                }
            }
        }

        if (sampleSum != 0.0f)
        {
            float sL = sampleSum * leftGain;
            float sR = sampleSum * rightGain;

            glitchFx.processSample(sL, sR, hostBpm);

            leftOut[s] += sL;
            if (leftOut != rightOut)
                rightOut[s] += sR;
        }
    }
}

std::shared_ptr<SoundSource> OscillatorSource::clone(int newId) const
{
    auto cloned = std::make_shared<OscillatorSource>(newId, name + " (Clone)");
    cloned->setAssignedNote(getAssignedNote());
    cloned->setChokeGroup(getChokeGroup());
    cloned->setOutputBus(getOutputBus());
    cloned->setMuted(getMuted());
    cloned->setSoloed(getSoloed());
    cloned->setGain(getGain());
    cloned->setPan(getPan());
    cloned->setPitchSemi(getPitchSemi());
    cloned->setPitchFine(getPitchFine());

    cloned->setWaveform(getWaveform());
    cloned->setPulseWidth(getPulseWidth());
    cloned->setGlitchMorph(getGlitchMorph());
    cloned->setFrequency(getFrequency());
    cloned->setPitchTrack(getPitchTrack());

    cloned->setAttackMs(getAttackMs());
    cloned->setHoldMs(getHoldMs());
    cloned->setDecayMs(getDecayMs());
    cloned->setSustainLevel(getSustainLevel());
    cloned->setReleaseMs(getReleaseMs());
    cloned->setCurveShape(getCurveShape());

    cloned->glitchFx.setBitDepth(glitchFx.getBitDepth());
    cloned->glitchFx.setBitcrushMix(glitchFx.getBitcrushMix());
    cloned->glitchFx.setDownsampleHz(glitchFx.getDownsampleHz());
    cloned->glitchFx.setDownsampleMix(glitchFx.getDownsampleMix());
    cloned->glitchFx.setStutterHz(glitchFx.getStutterHz());
    cloned->glitchFx.setStutterDuty(glitchFx.getStutterDuty());
    cloned->glitchFx.setStutterMix(glitchFx.getStutterMix());
    cloned->glitchFx.setStutterSync(glitchFx.getStutterSync());
    cloned->glitchFx.setStutterDivision(glitchFx.getStutterDivision());

    return cloned;
}

juce::var OscillatorSource::toVar() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("id", sourceId);
    obj->setProperty("name", juce::String(name));
    obj->setProperty("type", "Oscillator");
    obj->setProperty("assignedNote", getAssignedNote());
    obj->setProperty("chokeGroup", getChokeGroup());
    obj->setProperty("outputBus", getOutputBus());
    obj->setProperty("muted", getMuted());
    obj->setProperty("soloed", getSoloed());
    obj->setProperty("gain", getGain());
    obj->setProperty("pan", getPan());
    obj->setProperty("pitchSemi", getPitchSemi());
    obj->setProperty("pitchFine", getPitchFine());

    obj->setProperty("waveform", static_cast<int>(getWaveform()));
    obj->setProperty("pulseWidth", getPulseWidth());
    obj->setProperty("glitchMorph", getGlitchMorph());
    obj->setProperty("frequency", getFrequency());
    obj->setProperty("pitchTrack", getPitchTrack());

    obj->setProperty("attackMs", getAttackMs());
    obj->setProperty("holdMs", getHoldMs());
    obj->setProperty("decayMs", getDecayMs());
    obj->setProperty("sustain", getSustainLevel());
    obj->setProperty("releaseMs", getReleaseMs());
    obj->setProperty("curve", getCurveShape());

    obj->setProperty("bitDepth", glitchFx.getBitDepth());
    obj->setProperty("bitcrushMix", glitchFx.getBitcrushMix());
    obj->setProperty("downsampleHz", glitchFx.getDownsampleHz());
    obj->setProperty("downsampleMix", glitchFx.getDownsampleMix());
    obj->setProperty("stutterHz", glitchFx.getStutterHz());
    obj->setProperty("stutterDuty", glitchFx.getStutterDuty());
    obj->setProperty("stutterMix", glitchFx.getStutterMix());
    obj->setProperty("stutterSync", glitchFx.getStutterSync());
    obj->setProperty("stutterDivision", glitchFx.getStutterDivision());

    return juce::var(obj);
}

void OscillatorSource::fromVar(const juce::var& v)
{
    if (!v.isObject())
        return;

    auto* obj = v.getDynamicObject();
    if (obj == nullptr)
        return;

    if (obj->hasProperty("name")) name = obj->getProperty("name").toString().toStdString();
    if (obj->hasProperty("assignedNote")) setAssignedNote(static_cast<int>(obj->getProperty("assignedNote")));
    if (obj->hasProperty("chokeGroup")) setChokeGroup(static_cast<int>(obj->getProperty("chokeGroup")));
    if (obj->hasProperty("outputBus")) setOutputBus(static_cast<int>(obj->getProperty("outputBus")));
    if (obj->hasProperty("muted")) setMuted(static_cast<bool>(obj->getProperty("muted")));
    if (obj->hasProperty("soloed")) setSoloed(static_cast<bool>(obj->getProperty("soloed")));
    if (obj->hasProperty("gain")) setGain(static_cast<float>(obj->getProperty("gain")));
    if (obj->hasProperty("pan")) setPan(static_cast<float>(obj->getProperty("pan")));
    if (obj->hasProperty("pitchSemi")) setPitchSemi(static_cast<float>(obj->getProperty("pitchSemi")));
    if (obj->hasProperty("pitchFine")) setPitchFine(static_cast<float>(obj->getProperty("pitchFine")));

    if (obj->hasProperty("waveform")) setWaveform(static_cast<OscWaveform>(static_cast<int>(obj->getProperty("waveform"))));
    if (obj->hasProperty("pulseWidth")) setPulseWidth(static_cast<float>(obj->getProperty("pulseWidth")));
    if (obj->hasProperty("glitchMorph")) setGlitchMorph(static_cast<float>(obj->getProperty("glitchMorph")));
    if (obj->hasProperty("frequency")) setFrequency(static_cast<float>(obj->getProperty("frequency")));
    if (obj->hasProperty("pitchTrack")) setPitchTrack(static_cast<bool>(obj->getProperty("pitchTrack")));

    if (obj->hasProperty("attackMs")) setAttackMs(static_cast<float>(obj->getProperty("attackMs")));
    if (obj->hasProperty("holdMs")) setHoldMs(static_cast<float>(obj->getProperty("holdMs")));
    if (obj->hasProperty("decayMs")) setDecayMs(static_cast<float>(obj->getProperty("decayMs")));
    if (obj->hasProperty("sustain")) setSustainLevel(static_cast<float>(obj->getProperty("sustain")));
    if (obj->hasProperty("releaseMs")) setReleaseMs(static_cast<float>(obj->getProperty("releaseMs")));
    if (obj->hasProperty("curve")) setCurveShape(static_cast<float>(obj->getProperty("curve")));

    if (obj->hasProperty("bitDepth")) glitchFx.setBitDepth(static_cast<float>(obj->getProperty("bitDepth")));
    if (obj->hasProperty("bitcrushMix")) glitchFx.setBitcrushMix(static_cast<float>(obj->getProperty("bitcrushMix")));
    if (obj->hasProperty("downsampleHz")) glitchFx.setDownsampleHz(static_cast<float>(obj->getProperty("downsampleHz")));
    if (obj->hasProperty("downsampleMix")) glitchFx.setDownsampleMix(static_cast<float>(obj->getProperty("downsampleMix")));
    if (obj->hasProperty("stutterHz")) glitchFx.setStutterHz(static_cast<float>(obj->getProperty("stutterHz")));
    if (obj->hasProperty("stutterDuty")) glitchFx.setStutterDuty(static_cast<float>(obj->getProperty("stutterDuty")));
    if (obj->hasProperty("stutterMix")) glitchFx.setStutterMix(static_cast<float>(obj->getProperty("stutterMix")));
    if (obj->hasProperty("stutterSync")) glitchFx.setStutterSync(static_cast<bool>(obj->getProperty("stutterSync")));
    if (obj->hasProperty("stutterDivision")) glitchFx.setStutterDivision(static_cast<int>(obj->getProperty("stutterDivision")));
}

} // namespace GlitchDSP
