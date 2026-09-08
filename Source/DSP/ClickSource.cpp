#include "ClickSource.h"

namespace GlitchDSP
{

ClickSource::ClickSource(int id, const std::string& sourceName)
    : SoundSource(id, sourceName, SourceType::Click)
{
}

void ClickSource::prepare(double sampleRate, int /*maxBlockSize*/)
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
        v.sampleIndex = 0;
        v.phase = 0.0;
        v.noteNumber = -1;
    }
}

void ClickSource::setAttackMs(float ms) noexcept
{
    envAttackMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setAttackMs(ms);
}

void ClickSource::setHoldMs(float ms) noexcept
{
    envHoldMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setHoldMs(ms);
}

void ClickSource::setDecayMs(float ms) noexcept
{
    envDecayMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setDecayMs(ms);
}

void ClickSource::setSustainLevel(float lvl) noexcept
{
    envSustain.store(lvl, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setSustainLevel(lvl);
}

void ClickSource::setReleaseMs(float ms) noexcept
{
    envReleaseMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setReleaseMs(ms);
}

void ClickSource::setCurveShape(float shape) noexcept
{
    envCurve.store(shape, std::memory_order_relaxed);
    for (auto& v : voices)
    {
        v.envelope.setAttackCurve(shape);
        v.envelope.setDecayCurve(shape);
        v.envelope.setReleaseCurve(shape);
    }
}

void ClickSource::noteOn(int noteNumber, float velocity)
{
    int voiceIdx = -1;

    for (int i = 0; i < MAX_VOICES; ++i)
    {
        if (!voices[i].active)
        {
            voiceIdx = i;
            break;
        }
    }

    if (voiceIdx == -1)
    {
        float minLevel = 100.0f;
        for (int i = 0; i < MAX_VOICES; ++i)
        {
            float lvl = voices[i].envelope.getCurrentLevel();
            if (lvl < minLevel)
            {
                minLevel = lvl;
                voiceIdx = i;
            }
        }
    }

    if (voiceIdx >= 0)
    {
        auto& v = voices[voiceIdx];
        v.noteNumber = noteNumber;
        v.active = true;
        v.sampleIndex = 0;
        v.phase = 0.0;

        float totalSemitones = static_cast<float>(noteNumber - 69) + pitchSemi.load(std::memory_order_relaxed)
                               + (pitchFine.load(std::memory_order_relaxed) * 0.01f);
        v.noteFreq = static_cast<float>(440.0 * std::pow(2.0, totalSemitones / 12.0));

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

void ClickSource::noteOff(float /*velocity*/)
{
    for (auto& v : voices)
    {
        if (v.active)
        {
            v.envelope.noteOff();
        }
    }
}

void ClickSource::choke()
{
    for (auto& v : voices)
    {
        if (v.active)
        {
            v.envelope.choke();
        }
    }
}

bool ClickSource::isPlaying() const noexcept
{
    for (const auto& v : voices)
    {
        if (v.active)
            return true;
    }
    return false;
}

float ClickSource::generateClickSample(Voice& v, ClickType ct, int pw, float baseFreq, float damp, bool trackPitch, int pol) noexcept
{
    float raw = 0.0f;
    float freq = trackPitch ? v.noteFreq : baseFreq;
    if (freq < 10.0f) freq = 10.0f;

    switch (ct)
    {
        case ClickType::Dirac:
        {
            if (v.sampleIndex < pw)
            {
                if (pol == 0) // Positive
                    raw = 1.0f;
                else if (pol == 1) // Negative
                    raw = -1.0f;
                else // Bipolar alternating
                    raw = (v.sampleIndex % 2 == 0) ? 1.0f : -1.0f;
            }
            else
            {
                raw = 0.0f;
            }
            break;
        }

        case ClickType::Resonant:
        {
            // Damped sinusoidal pop: sin(2pi * f * t) * exp(-t / tau)
            double t = static_cast<double>(v.sampleIndex) / currentSampleRate;
            double tau = 0.001 + (1.0 - static_cast<double>(damp)) * 0.09;
            double decay = std::exp(-t / tau);
            if (decay > 1e-5)
            {
                double angle = t * 2.0 * juce::MathConstants<double>::pi * static_cast<double>(freq);
                raw = static_cast<float>(std::sin(angle) * decay);
                if (pol == 1) raw = -raw;
            }
            break;
        }

        case ClickType::Chirp:
        {
            // Exponential frequency drop from 4*freq to freq over 8ms
            double t = static_cast<double>(v.sampleIndex) / currentSampleRate;
            double chirpDuration = 0.008 * (1.0 + (1.0 - static_cast<double>(damp)));
            double decay = std::exp(-t / (chirpDuration * 0.8));
            if (decay > 1e-5)
            {
                double sweepRate = std::exp(-t / (chirpDuration * 0.25));
                double curFreq = static_cast<double>(freq) * (1.0 + 3.0 * sweepRate);
                v.phase += curFreq / currentSampleRate;
                if (v.phase >= 1.0) v.phase -= std::floor(v.phase);

                raw = static_cast<float>(std::sin(v.phase * 2.0 * juce::MathConstants<double>::pi) * decay);
                if (pol == 1) raw = -raw;
            }
            break;
        }

        case ClickType::BitFlip:
        {
            if (v.sampleIndex < pw * 4)
            {
                uint32_t seed = static_cast<uint32_t>(v.sampleIndex * 1664525u + 1013904223u);
                seed ^= seed << 13;
                seed ^= seed >> 17;
                seed ^= seed << 5;
                raw = (seed & 1) ? 1.0f : -1.0f;
            }
            else
            {
                raw = 0.0f;
            }
            break;
        }
    }

    v.sampleIndex++;
    return raw;
}

void ClickSource::processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, double hostBpm, double /*hostPpq*/)
{
    if (getMuted())
        return;

    auto ct = getClickType();
    int pw = getPulseWidthSamples();
    float baseFreq = getClickFrequency();
    float damp = getClickDamping();
    bool pt = getPitchTrack();
    int pol = getPolarity();

    float currentGain = getGain();
    float currentPan = getPan();

    float leftGain = currentGain * std::cos((currentPan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
    float rightGain = currentGain * std::sin((currentPan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);

    auto* leftOut = buffer.getWritePointer(0, startSample);
    auto* rightOut = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, startSample) : leftOut;

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
                    sampleSum += generateClickSample(v, ct, pw, baseFreq, damp, pt, pol) * env;
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

std::shared_ptr<SoundSource> ClickSource::clone(int newId) const
{
    auto cloned = std::make_shared<ClickSource>(newId, name + " (Clone)");
    cloned->setAssignedNote(getAssignedNote());
    cloned->setChokeGroup(getChokeGroup());
    cloned->setGain(getGain());
    cloned->setPan(getPan());
    cloned->setPitchSemi(getPitchSemi());
    cloned->setPitchFine(getPitchFine());

    cloned->setClickType(getClickType());
    cloned->setPulseWidthSamples(getPulseWidthSamples());
    cloned->setClickFrequency(getClickFrequency());
    cloned->setClickDamping(getClickDamping());
    cloned->setPitchTrack(getPitchTrack());
    cloned->setPolarity(getPolarity());

    cloned->setAttackMs(getAttackMs());
    cloned->setHoldMs(getHoldMs());
    cloned->setDecayMs(getDecayMs());
    cloned->setSustainLevel(getSustainLevel());
    cloned->setReleaseMs(getReleaseMs());
    cloned->setCurveShape(getCurveShape());

    cloned->getGlitchFX().setBitDepth(glitchFx.getBitDepth());
    cloned->getGlitchFX().setBitcrushMix(glitchFx.getBitcrushMix());
    cloned->getGlitchFX().setDownsampleHz(glitchFx.getDownsampleHz());
    cloned->getGlitchFX().setDownsampleMix(glitchFx.getDownsampleMix());
    cloned->getGlitchFX().setStutterHz(glitchFx.getStutterHz());
    cloned->getGlitchFX().setStutterDuty(glitchFx.getStutterDuty());
    cloned->getGlitchFX().setStutterMix(glitchFx.getStutterMix());
    cloned->getGlitchFX().setStutterSync(glitchFx.getStutterSync());
    cloned->getGlitchFX().setStutterDivision(glitchFx.getStutterDivision());

    return cloned;
}

juce::var ClickSource::toVar() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("id", getId());
    obj->setProperty("name", juce::String(getName()));
    obj->setProperty("type", "Click");
    obj->setProperty("assignedNote", getAssignedNote());
    obj->setProperty("chokeGroup", getChokeGroup());
    obj->setProperty("muted", getMuted());
    obj->setProperty("soloed", getSoloed());
    obj->setProperty("gain", getGain());
    obj->setProperty("pan", getPan());
    obj->setProperty("pitchSemi", getPitchSemi());
    obj->setProperty("pitchFine", getPitchFine());

    obj->setProperty("clickType", static_cast<int>(getClickType()));
    obj->setProperty("clickWidthSamples", getPulseWidthSamples());
    obj->setProperty("clickFrequency", getClickFrequency());
    obj->setProperty("clickDamping", getClickDamping());
    obj->setProperty("clickPitchTrack", getPitchTrack());
    obj->setProperty("clickPolarity", getPolarity());

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

void ClickSource::fromVar(const juce::var& v)
{
    auto* obj = v.getDynamicObject();
    if (obj == nullptr)
        return;

    if (obj->hasProperty("name")) setName(obj->getProperty("name").toString().toStdString());
    if (obj->hasProperty("assignedNote")) setAssignedNote(static_cast<int>(obj->getProperty("assignedNote")));
    if (obj->hasProperty("chokeGroup")) setChokeGroup(static_cast<int>(obj->getProperty("chokeGroup")));
    if (obj->hasProperty("muted")) setMuted(static_cast<bool>(obj->getProperty("muted")));
    if (obj->hasProperty("soloed")) setSoloed(static_cast<bool>(obj->getProperty("soloed")));
    if (obj->hasProperty("gain")) setGain(static_cast<float>(obj->getProperty("gain")));
    if (obj->hasProperty("pan")) setPan(static_cast<float>(obj->getProperty("pan")));
    if (obj->hasProperty("pitchSemi")) setPitchSemi(static_cast<float>(obj->getProperty("pitchSemi")));
    if (obj->hasProperty("pitchFine")) setPitchFine(static_cast<float>(obj->getProperty("pitchFine")));

    if (obj->hasProperty("clickType")) setClickType(static_cast<ClickType>(static_cast<int>(obj->getProperty("clickType"))));
    if (obj->hasProperty("clickWidthSamples")) setPulseWidthSamples(static_cast<int>(obj->getProperty("clickWidthSamples")));
    if (obj->hasProperty("clickFrequency")) setClickFrequency(static_cast<float>(obj->getProperty("clickFrequency")));
    if (obj->hasProperty("clickDamping")) setClickDamping(static_cast<float>(obj->getProperty("clickDamping")));
    if (obj->hasProperty("clickPitchTrack")) setPitchTrack(static_cast<bool>(obj->getProperty("clickPitchTrack")));
    if (obj->hasProperty("clickPolarity")) setPolarity(static_cast<int>(obj->getProperty("clickPolarity")));

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
