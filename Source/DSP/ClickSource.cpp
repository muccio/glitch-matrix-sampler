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
        v.active = false;
        v.samplesRemaining = 0;
        v.amplitude = 1.0f;
        v.noteNumber = -1;
    }
    activityHoldCounter.store(0, std::memory_order_relaxed);
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
        voiceIdx = 0;
    }

    auto& v = voices[voiceIdx];
    v.noteNumber = noteNumber;
    v.active = true;
    v.samplesRemaining = 1; // Pure 1-sample unit impulse
    v.amplitude = velocity > 0.0f ? velocity : 1.0f;

    activityHoldCounter.store(2, std::memory_order_relaxed);
}

void ClickSource::noteOff(int /*noteNumber*/, float /*velocity*/)
{
    // Clicks are single-sample impulses; noteOff does not choke or modify the impulse
}

void ClickSource::choke()
{
    for (auto& v : voices)
    {
        v.active = false;
        v.samplesRemaining = 0;
    }
    activityHoldCounter.store(0, std::memory_order_relaxed);
}

bool ClickSource::isPlaying() const noexcept
{
    if (activityHoldCounter.load(std::memory_order_relaxed) > 0)
        return true;

    for (const auto& v : voices)
    {
        if (v.active)
            return true;
    }
    return false;
}

void ClickSource::processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, double hostBpm, double /*hostPpq*/)
{
    if (getMuted())
        return;

    int pol = getPolarity();
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
                if (v.samplesRemaining > 0)
                {
                    float impulse = v.amplitude;
                    if (pol == 1) // Negative
                        impulse = -impulse;
                    else if (pol == 2) // Bipolar alternating
                        impulse = (v.noteNumber % 2 == 0) ? impulse : -impulse;

                    sampleSum += impulse;
                    v.samplesRemaining--;
                }

                if (v.samplesRemaining <= 0)
                {
                    v.active = false;
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

    int hold = activityHoldCounter.load(std::memory_order_relaxed);
    if (hold > 0)
        activityHoldCounter.store(hold - 1, std::memory_order_relaxed);
}

std::shared_ptr<SoundSource> ClickSource::clone(int newId) const
{
    auto cloned = std::make_shared<ClickSource>(newId, name + " (Clone)");
    cloned->setAssignedNote(getAssignedNote());
    cloned->setChokeGroup(getChokeGroup());
    cloned->setOutputBus(getOutputBus());
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
    obj->setProperty("outputBus", getOutputBus());
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
    if (obj->hasProperty("outputBus")) setOutputBus(static_cast<int>(obj->getProperty("outputBus")));
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
