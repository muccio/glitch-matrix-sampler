#include "SampleSource.h"

namespace GlitchDSP
{

SampleSource::SampleSource(int id, const std::string& sourceName)
    : SoundSource(id, sourceName, SourceType::Sample)
{
}

void SampleSource::prepare(double sampleRate, int /*maxBlockSize*/)
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
        v.playhead = 0.0;
        v.noteNumber = -1;
    }
}

void SampleSource::setAttackMs(float ms) noexcept
{
    envAttackMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setAttackMs(ms);
}

void SampleSource::setHoldMs(float ms) noexcept
{
    envHoldMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setHoldMs(ms);
}

void SampleSource::setDecayMs(float ms) noexcept
{
    envDecayMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setDecayMs(ms);
}

void SampleSource::setSustainLevel(float lvl) noexcept
{
    envSustain.store(lvl, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setSustainLevel(lvl);
}

void SampleSource::setReleaseMs(float ms) noexcept
{
    envReleaseMs.store(ms, std::memory_order_relaxed);
    for (auto& v : voices) v.envelope.setReleaseMs(ms);
}

void SampleSource::setCurveShape(float shape) noexcept
{
    envCurve.store(shape, std::memory_order_relaxed);
    for (auto& v : voices)
    {
        v.envelope.setAttackCurve(shape);
        v.envelope.setDecayCurve(shape);
        v.envelope.setReleaseCurve(shape);
    }
}

bool SampleSource::loadFile(const juce::File& file, juce::AudioFormatManager& formatManager)
{
    if (!file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
        return false;

    auto newBuffer = std::make_shared<juce::AudioBuffer<float>>(reader->numChannels, static_cast<int>(reader->lengthInSamples));
    reader->read(newBuffer.get(), 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

    setAudioBuffer(newBuffer, reader->sampleRate, file.getFullPathName().toStdString());
    return true;
}

bool SampleSource::loadFromMemory(const void* data, size_t dataSize, juce::AudioFormatManager& formatManager)
{
    if (data == nullptr || dataSize == 0)
        return false;

    auto stream = std::make_unique<juce::MemoryInputStream>(data, dataSize, false);
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(std::move(stream)));
    if (reader == nullptr)
        return false;

    auto newBuffer = std::make_shared<juce::AudioBuffer<float>>(reader->numChannels, static_cast<int>(reader->lengthInSamples));
    reader->read(newBuffer.get(), 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

    setAudioBuffer(newBuffer, reader->sampleRate, "Embedded_Memory_Sample");
    return true;
}

void SampleSource::setAudioBuffer(std::shared_ptr<juce::AudioBuffer<float>> newBuffer, double sampleRate, const std::string& path)
{
    sampleBuffer = newBuffer;
    loadedSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    filePath = path;

    // Reset voice playheads
    for (auto& v : voices)
    {
        v.active = false;
        v.playhead = 0.0;
    }
}

juce::var SampleSource::getWaveformPeaks(int numPoints) const
{
    juce::Array<juce::var> peaks;
    if (!sampleBuffer || sampleBuffer->getNumSamples() == 0 || numPoints <= 0)
        return juce::var(peaks);

    int totalSamples = sampleBuffer->getNumSamples();
    int samplesPerPixel = std::max(1, totalSamples / numPoints);
    const float* channelData = sampleBuffer->getReadPointer(0);

    for (int p = 0; p < numPoints; ++p)
    {
        int start = p * samplesPerPixel;
        int end = std::min(totalSamples, start + samplesPerPixel);
        float minVal = 0.0f;
        float maxVal = 0.0f;

        for (int i = start; i < end; ++i)
        {
            float s = channelData[i];
            if (s < minVal) minVal = s;
            if (s > maxVal) maxVal = s;
        }

        auto* pointObj = new juce::DynamicObject();
        pointObj->setProperty("min", minVal);
        pointObj->setProperty("max", maxVal);
        peaks.add(juce::var(pointObj));
    }

    return juce::var(peaks);
}

void SampleSource::noteOn(int noteNumber, float velocity)
{
    if (!sampleBuffer || sampleBuffer->getNumSamples() == 0)
        return;

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

        float totalSemitones = static_cast<float>(noteNumber - 60) + pitchSemi.load(std::memory_order_relaxed)
                               + (pitchFine.load(std::memory_order_relaxed) * 0.01f);
        double pitchRatio = std::pow(2.0, totalSemitones / 12.0);
        double srRatio = loadedSampleRate / currentSampleRate;
        v.speedFactor = pitchRatio * srRatio * playbackSpeed.load(std::memory_order_relaxed);

        int totalFrames = sampleBuffer->getNumSamples();
        float sp = startPoint.load(std::memory_order_relaxed);
        float ep = endPoint.load(std::memory_order_relaxed);
        bool rev = isReverse.load(std::memory_order_relaxed);

        if (microLoopEnabled.load(std::memory_order_relaxed))
        {
            float ls = loopStart.load(std::memory_order_relaxed);
            v.loopStartFrame = ls * totalFrames;
            double loopLenSamples = (loopLengthMs.load(std::memory_order_relaxed) * 0.001) * loadedSampleRate;
            v.loopEndFrame = std::min(static_cast<double>(totalFrames - 1), v.loopStartFrame + loopLenSamples);
            v.xfadeFrames = std::min(loopLenSamples * 0.25, (crossfadeMs.load(std::memory_order_relaxed) * 0.001) * loadedSampleRate);

            v.playhead = rev ? v.loopEndFrame : v.loopStartFrame;
        }
        else
        {
            v.playhead = rev ? (ep * (totalFrames - 1)) : (sp * (totalFrames - 1));
        }

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

void SampleSource::noteOff(float /*velocity*/)
{
    for (auto& v : voices)
    {
        if (v.active)
        {
            v.envelope.noteOff();
        }
    }
}

void SampleSource::choke()
{
    for (auto& v : voices)
    {
        if (v.active)
        {
            v.envelope.choke();
        }
    }
}

bool SampleSource::isPlaying() const noexcept
{
    for (const auto& v : voices)
    {
        if (v.active)
            return true;
    }
    return false;
}

void SampleSource::interpolateSample(const juce::AudioBuffer<float>& buf, double pos, float& outL, float& outR) const noexcept
{
    int total = buf.getNumSamples();
    int idx0 = static_cast<int>(pos);
    int idx1 = idx0 + 1;
    if (idx1 >= total) idx1 = total - 1;
    if (idx0 < 0) idx0 = 0;

    float frac = static_cast<float>(pos - idx0);

    const float* leftData = buf.getReadPointer(0);
    outL = leftData[idx0] + frac * (leftData[idx1] - leftData[idx0]);

    if (buf.getNumChannels() > 1)
    {
        const float* rightData = buf.getReadPointer(1);
        outR = rightData[idx0] + frac * (rightData[idx1] - rightData[idx0]);
    }
    else
    {
        outR = outL;
    }
}

void SampleSource::processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, double hostBpm, double /*hostPpq*/)
{
    if (getMuted() || !sampleBuffer || sampleBuffer->getNumSamples() == 0)
        return;

    const auto& buf = *sampleBuffer;
    int totalFrames = buf.getNumSamples();

    float currentGain = getGain();
    float currentPan = getPan();
    bool rev = isReverse.load(std::memory_order_relaxed);
    bool loop = microLoopEnabled.load(std::memory_order_relaxed);
    float sp = startPoint.load(std::memory_order_relaxed);
    float ep = endPoint.load(std::memory_order_relaxed);

    double startFrame = sp * (totalFrames - 1);
    double endFrame = ep * (totalFrames - 1);

    float leftGain = currentGain * std::cos((currentPan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
    float rightGain = currentGain * std::sin((currentPan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);

    auto* leftOut = buffer.getWritePointer(0, startSample);
    auto* rightOut = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, startSample) : leftOut;

    for (int s = 0; s < numSamples; ++s)
    {
        float sampleSumL = 0.0f;
        float sampleSumR = 0.0f;

        for (auto& v : voices)
        {
            if (v.active)
            {
                float env = v.envelope.getNextSample();
                if (!v.envelope.isActive())
                {
                    v.active = false;
                    continue;
                }

                float sL = 0.0f, sR = 0.0f;
                interpolateSample(buf, v.playhead, sL, sR);

                if (loop)
                {
                    double loopLen = v.loopEndFrame - v.loopStartFrame;
                    if (loopLen > 10.0)
                    {
                        // Crossfade logic near loop boundary
                        if (!rev)
                        {
                            if (v.playhead >= v.loopEndFrame - v.xfadeFrames)
                            {
                                double offsetFromStart = v.playhead - (v.loopEndFrame - v.xfadeFrames);
                                double wrapPos = v.loopStartFrame + offsetFromStart;
                                float wrapL = 0.0f, wrapR = 0.0f;
                                interpolateSample(buf, wrapPos, wrapL, wrapR);
                                float fade = static_cast<float>(offsetFromStart / v.xfadeFrames);
                                sL = (1.0f - fade) * sL + fade * wrapL;
                                sR = (1.0f - fade) * sR + fade * wrapR;
                            }
                        }
                    }

                    // Advance playhead
                    if (!rev)
                    {
                        v.playhead += v.speedFactor;
                        if (v.playhead >= v.loopEndFrame)
                            v.playhead = v.loopStartFrame + (v.playhead - v.loopEndFrame);
                    }
                    else
                    {
                        v.playhead -= v.speedFactor;
                        if (v.playhead <= v.loopStartFrame)
                            v.playhead = v.loopEndFrame - (v.loopStartFrame - v.playhead);
                    }
                }
                else
                {
                    // Regular linear playback with slicing bounds
                    if (!rev)
                    {
                        v.playhead += v.speedFactor;
                        if (v.playhead >= endFrame || v.playhead >= totalFrames - 1)
                        {
                            v.active = false;
                        }
                    }
                    else
                    {
                        v.playhead -= v.speedFactor;
                        if (v.playhead <= startFrame || v.playhead <= 0.0)
                        {
                            v.active = false;
                        }
                    }
                }

                sampleSumL += sL * env;
                sampleSumR += sR * env;
            }
        }

        if (sampleSumL != 0.0f || sampleSumR != 0.0f)
        {
            float sL = sampleSumL * leftGain;
            float sR = sampleSumR * rightGain;

            glitchFx.processSample(sL, sR, hostBpm);

            leftOut[s] += sL;
            if (leftOut != rightOut)
                rightOut[s] += sR;
        }
    }
}

std::shared_ptr<SoundSource> SampleSource::clone(int newId) const
{
    auto cloned = std::make_shared<SampleSource>(newId, name + " (Clone)");
    cloned->setAssignedNote(getAssignedNote());
    cloned->setChokeGroup(getChokeGroup());
    cloned->setMuted(getMuted());
    cloned->setSoloed(getSoloed());
    cloned->setGain(getGain());
    cloned->setPan(getPan());
    cloned->setPitchSemi(getPitchSemi());
    cloned->setPitchFine(getPitchFine());

    cloned->setStartPoint(getStartPoint());
    cloned->setEndPoint(getEndPoint());
    cloned->setReverse(getReverse());
    cloned->setSpeed(getSpeed());

    cloned->setMicroLoop(getMicroLoop());
    cloned->setLoopStart(getLoopStart());
    cloned->setLoopLengthMs(getLoopLengthMs());
    cloned->setCrossfadeMs(getCrossfadeMs());

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

    if (sampleBuffer)
    {
        cloned->setAudioBuffer(sampleBuffer, loadedSampleRate, filePath);
    }

    return cloned;
}

juce::var SampleSource::toVar() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("id", sourceId);
    obj->setProperty("name", juce::String(name));
    obj->setProperty("type", "Sample");
    obj->setProperty("assignedNote", getAssignedNote());
    obj->setProperty("chokeGroup", getChokeGroup());
    obj->setProperty("muted", getMuted());
    obj->setProperty("soloed", getSoloed());
    obj->setProperty("gain", getGain());
    obj->setProperty("pan", getPan());
    obj->setProperty("pitchSemi", getPitchSemi());
    obj->setProperty("pitchFine", getPitchFine());

    obj->setProperty("filePath", juce::String(filePath));
    obj->setProperty("startPoint", getStartPoint());
    obj->setProperty("endPoint", getEndPoint());
    obj->setProperty("reverse", getReverse());
    obj->setProperty("speed", getSpeed());

    obj->setProperty("microLoop", getMicroLoop());
    obj->setProperty("loopStart", getLoopStart());
    obj->setProperty("loopLengthMs", getLoopLengthMs());
    obj->setProperty("crossfadeMs", getCrossfadeMs());

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

void SampleSource::fromVar(const juce::var& v)
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

    if (obj->hasProperty("filePath")) filePath = obj->getProperty("filePath").toString().toStdString();
    if (obj->hasProperty("startPoint")) setStartPoint(static_cast<float>(obj->getProperty("startPoint")));
    if (obj->hasProperty("endPoint")) setEndPoint(static_cast<float>(obj->getProperty("endPoint")));
    if (obj->hasProperty("reverse")) setReverse(static_cast<bool>(obj->getProperty("reverse")));
    if (obj->hasProperty("speed")) setSpeed(static_cast<float>(obj->getProperty("speed")));

    if (obj->hasProperty("microLoop")) setMicroLoop(static_cast<bool>(obj->getProperty("microLoop")));
    if (obj->hasProperty("loopStart")) setLoopStart(static_cast<float>(obj->getProperty("loopStart")));
    if (obj->hasProperty("loopLengthMs")) setLoopLengthMs(static_cast<float>(obj->getProperty("loopLengthMs")));
    if (obj->hasProperty("crossfadeMs")) setCrossfadeMs(static_cast<float>(obj->getProperty("crossfadeMs")));

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
