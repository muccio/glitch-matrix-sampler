#include "NoiseSource.h"

namespace GlitchDSP
{

NoiseSource::NoiseSource(int id, const std::string& sourceName)
    : SoundSource(id, sourceName, SourceType::Noise)
{
}

void NoiseSource::prepare(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    glitchFx.prepare(currentSampleRate);

    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
    hashPhase = 0.0;
    currentHashSample = 0.0f;
    rngState = 123456789 + static_cast<uint32_t>(sourceId * 10007);

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
        v.noteNumber = -1;
        v.pitchTracking = 1.0f;
    }
}

void NoiseSource::setAttackMs(float ms) noexcept
{
    envAttackMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setAttackMs(ms);
}

void NoiseSource::setHoldMs(float ms) noexcept
{
    envHoldMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setHoldMs(ms);
}

void NoiseSource::setDecayMs(float ms) noexcept
{
    envDecayMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setDecayMs(ms);
}

void NoiseSource::setSustainLevel(float lvl) noexcept
{
    envSustain.store(lvl, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setSustainLevel(lvl);
}

void NoiseSource::setReleaseMs(float ms) noexcept
{
    envReleaseMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setReleaseMs(ms);
}

void NoiseSource::setCurveShape(float shape) noexcept
{
    envCurve.store(shape, std::memory_order_relaxed);
    for (auto& v : voices)
    {
        v.envelope.setAttackCurve(shape);
        v.envelope.setDecayCurve(shape);
        v.envelope.setReleaseCurve(shape);
    }
}

void NoiseSource::noteOn(int noteNumber, float velocity)
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

        float totalSemitones = static_cast<float>(noteNumber - 69) + pitchSemi.load(std::memory_order_relaxed)
                               + (pitchFine.load(std::memory_order_relaxed) * 0.01f);
        v.pitchTracking = static_cast<float>(std::pow(2.0, totalSemitones / 12.0));

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

void NoiseSource::noteOff(float /*velocity*/)
{
    for (auto& v : voices)
    {
        if (v.active)
        {
            v.envelope.noteOff();
        }
    }
}

void NoiseSource::choke()
{
    for (auto& v : voices)
    {
        if (v.active)
        {
            v.envelope.choke();
        }
    }
}

bool NoiseSource::isPlaying() const noexcept
{
    for (const auto& v : voices)
    {
        if (v.active)
            return true;
    }
    return false;
}

float NoiseSource::generateNoise(NoiseType type, float pitchMult) noexcept
{
    float white = (static_cast<float>(xorshift32()) / 2147483648.0f) - 1.0f;

    switch (type)
    {
        case NoiseType::White:
            return white;

        case NoiseType::Pink:
        {
            // Voss-McCartney / Paul Kellet pinking filter
            b0 = 0.99886f * b0 + white * 0.0555179f;
            b1 = 0.99332f * b1 + white * 0.0750759f;
            b2 = 0.96900f * b2 + white * 0.1538520f;
            b3 = 0.86650f * b3 + white * 0.3104856f;
            b4 = 0.55000f * b4 + white * 0.5329522f;
            b5 = -0.7616f * b5 - white * 0.0168980f;
            float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
            b6 = white * 0.115926f;
            return pink * 0.11f;
        }

        case NoiseType::Crackle:
        {
            // Poisson digital crackle / random impulse density
            float density = crackleDensity.load(std::memory_order_relaxed) * pitchMult;
            float prob = std::clamp(density / static_cast<float>(currentSampleRate), 0.00001f, 0.9f);
            float u = static_cast<float>(xorshift32()) / 4294967295.0f;
            if (u < prob)
            {
                // Instantaneous random impulse with sharp crackle
                return (white > 0.0f) ? 1.0f : -1.0f;
            }
            return 0.0f;
        }

        case NoiseType::BitFlipHash:
        {
            // Bit-Flip / Hash noise clocked at decimated rate
            float hr = hashRate.load(std::memory_order_relaxed) * pitchMult;
            double phaseInc = std::clamp(static_cast<double>(hr) / currentSampleRate, 0.001, 1.0);
            hashPhase += phaseInc;
            if (hashPhase >= 1.0)
            {
                hashPhase -= std::floor(hashPhase);
                uint32_t h = xorshift32();
                // XOR fold into 8 bits and normalize
                uint32_t folded = (h ^ (h >> 8) ^ (h >> 16) ^ (h >> 24)) & 0xFF;
                currentHashSample = (static_cast<float>(folded) / 127.5f) - 1.0f;
            }
            return currentHashSample;
        }
    }
    return 0.0f;
}

void NoiseSource::processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, double hostBpm, double /*hostPpq*/)
{
    if (getMuted())
        return;

    auto currentType = getNoiseType();
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
                    sampleSum += generateNoise(currentType, v.pitchTracking) * env;
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

std::shared_ptr<SoundSource> NoiseSource::clone(int newId) const
{
    auto cloned = std::make_shared<NoiseSource>(newId, name + " (Clone)");
    cloned->setAssignedNote(getAssignedNote());
    cloned->setChokeGroup(getChokeGroup());
    cloned->setMuted(getMuted());
    cloned->setSoloed(getSoloed());
    cloned->setGain(getGain());
    cloned->setPan(getPan());
    cloned->setPitchSemi(getPitchSemi());
    cloned->setPitchFine(getPitchFine());

    cloned->setNoiseType(getNoiseType());
    cloned->setCrackleDensity(getCrackleDensity());
    cloned->setHashRate(getHashRate());

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

juce::var NoiseSource::toVar() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("id", sourceId);
    obj->setProperty("name", juce::String(name));
    obj->setProperty("type", "Noise");
    obj->setProperty("assignedNote", getAssignedNote());
    obj->setProperty("chokeGroup", getChokeGroup());
    obj->setProperty("muted", getMuted());
    obj->setProperty("soloed", getSoloed());
    obj->setProperty("gain", getGain());
    obj->setProperty("pan", getPan());
    obj->setProperty("pitchSemi", getPitchSemi());
    obj->setProperty("pitchFine", getPitchFine());

    obj->setProperty("noiseType", static_cast<int>(getNoiseType()));
    obj->setProperty("crackleDensity", getCrackleDensity());
    obj->setProperty("hashRate", getHashRate());

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

void NoiseSource::fromVar(const juce::var& v)
{
    if (!v.isObject())
        return;

    auto* obj = v.getDynamicObject();
    if (obj == nullptr)
        return;

    if (obj->hasProperty("name")) name = obj->getProperty("name").toString().toStdString();
    if (obj->hasProperty("assignedNote")) setAssignedNote(static_cast<int>(obj->getProperty("assignedNote")));
    if (obj->hasProperty("chokeGroup")) setChokeGroup(static_cast<int>(obj->getProperty("chokeGroup")));
    if (obj->hasProperty("muted")) setMuted(static_cast<bool>(obj->getProperty("muted")));
    if (obj->hasProperty("soloed")) setSoloed(static_cast<bool>(obj->getProperty("soloed")));
    if (obj->hasProperty("gain")) setGain(static_cast<float>(obj->getProperty("gain")));
    if (obj->hasProperty("pan")) setPan(static_cast<float>(obj->getProperty("pan")));
    if (obj->hasProperty("pitchSemi")) setPitchSemi(static_cast<float>(obj->getProperty("pitchSemi")));
    if (obj->hasProperty("pitchFine")) setPitchFine(static_cast<float>(obj->getProperty("pitchFine")));

    if (obj->hasProperty("noiseType")) setNoiseType(static_cast<NoiseType>(static_cast<int>(obj->getProperty("noiseType"))));
    if (obj->hasProperty("crackleDensity")) setCrackleDensity(static_cast<float>(obj->getProperty("crackleDensity")));
    if (obj->hasProperty("hashRate")) setHashRate(static_cast<float>(obj->getProperty("hashRate")));

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
