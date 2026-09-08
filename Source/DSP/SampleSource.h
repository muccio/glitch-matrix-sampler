#pragma once

#include "SoundSource.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>

namespace GlitchDSP
{

class SampleSource : public SoundSource
{
public:
    static constexpr int MAX_VOICES = 8;

    SampleSource(int id, const std::string& sourceName = "Sample");
    ~SampleSource() override = default;

    void prepare(double sampleRate, int maxBlockSize) override;
    void noteOn(int noteNumber, float velocity) override;
    void noteOff(float velocity) override;
    void choke() override;
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, double hostBpm, double hostPpq) override;
    bool isPlaying() const noexcept override;

    std::shared_ptr<SoundSource> clone(int newId) const override;
    juce::var toVar() const override;
    void fromVar(const juce::var& v) override;

    // Sample file loading
    bool loadFile(const juce::File& file, juce::AudioFormatManager& formatManager);
    bool loadFromMemory(const void* data, size_t dataSize, juce::AudioFormatManager& formatManager);
    void setAudioBuffer(std::shared_ptr<juce::AudioBuffer<float>> newBuffer, double sampleRate, const std::string& path);

    const std::string& getFilePath() const noexcept { return filePath; }
    int getNumSampleFrames() const noexcept { return sampleBuffer ? sampleBuffer->getNumSamples() : 0; }
    double getLoadedSampleRate() const noexcept { return loadedSampleRate; }

    // Waveform Thumbnail extraction for Web UI (returns array of min/max pairs)
    juce::var getWaveformPeaks(int numPoints = 256) const;

    // Slicing & Playback Parameters
    float getStartPoint() const noexcept { return startPoint.load(std::memory_order_relaxed); }
    void setStartPoint(float sp) noexcept { startPoint.store(std::clamp(sp, 0.0f, 1.0f), std::memory_order_relaxed); }

    float getEndPoint() const noexcept { return endPoint.load(std::memory_order_relaxed); }
    void setEndPoint(float ep) noexcept { endPoint.store(std::clamp(ep, 0.0f, 1.0f), std::memory_order_relaxed); }

    bool getReverse() const noexcept { return isReverse.load(std::memory_order_relaxed); }
    void setReverse(bool rev) noexcept { isReverse.store(rev, std::memory_order_relaxed); }

    float getSpeed() const noexcept { return playbackSpeed.load(std::memory_order_relaxed); }
    void setSpeed(float spd) noexcept { playbackSpeed.store(std::clamp(spd, 0.05f, 8.0f), std::memory_order_relaxed); }

    // Micro-looping
    bool getMicroLoop() const noexcept { return microLoopEnabled.load(std::memory_order_relaxed); }
    void setMicroLoop(bool loop) noexcept { microLoopEnabled.store(loop, std::memory_order_relaxed); }

    float getLoopStart() const noexcept { return loopStart.load(std::memory_order_relaxed); }
    void setLoopStart(float ls) noexcept { loopStart.store(std::clamp(ls, 0.0f, 1.0f), std::memory_order_relaxed); }

    float getLoopLengthMs() const noexcept { return loopLengthMs.load(std::memory_order_relaxed); }
    void setLoopLengthMs(float ms) noexcept { loopLengthMs.store(std::clamp(ms, 1.0f, 300.0f), std::memory_order_relaxed); }

    float getCrossfadeMs() const noexcept { return crossfadeMs.load(std::memory_order_relaxed); }
    void setCrossfadeMs(float ms) noexcept { crossfadeMs.store(std::clamp(ms, 0.1f, 50.0f), std::memory_order_relaxed); }

    // Fast Envelope access
    void setAttackMs(float ms) noexcept;
    void setHoldMs(float ms) noexcept;
    void setDecayMs(float ms) noexcept;
    void setSustainLevel(float lvl) noexcept;
    void setReleaseMs(float ms) noexcept;
    void setCurveShape(float shape) noexcept;

    float getAttackMs() const noexcept { return envAttackMs.load(std::memory_order_relaxed); }
    float getHoldMs() const noexcept { return envHoldMs.load(std::memory_order_relaxed); }
    float getDecayMs() const noexcept { return envDecayMs.load(std::memory_order_relaxed); }
    float getSustainLevel() const noexcept { return envSustain.load(std::memory_order_relaxed); }
    float getReleaseMs() const noexcept { return envReleaseMs.load(std::memory_order_relaxed); }
    float getCurveShape() const noexcept { return envCurve.load(std::memory_order_relaxed); }

    // Glitch FX access
    GlitchFX& getGlitchFX() noexcept { return glitchFx; }
    const GlitchFX& getGlitchFX() const noexcept { return glitchFx; }

private:
    struct Voice
    {
        double playhead = 0.0;
        double speedFactor = 1.0;
        int noteNumber = -1;
        bool active = false;
        FastEnvelope envelope;

        // Micro-loop state
        double loopStartFrame = 0.0;
        double loopEndFrame = 0.0;
        double xfadeFrames = 0.0;
    };

    double currentSampleRate = 44100.0;
    double loadedSampleRate = 44100.0;
    std::string filePath;
    std::shared_ptr<juce::AudioBuffer<float>> sampleBuffer;

    std::array<Voice, MAX_VOICES> voices;
    GlitchFX glitchFx;

    std::atomic<float> startPoint { 0.0f };
    std::atomic<float> endPoint { 1.0f };
    std::atomic<bool> isReverse { false };
    std::atomic<float> playbackSpeed { 1.0f };

    std::atomic<bool> microLoopEnabled { false };
    std::atomic<float> loopStart { 0.0f };
    std::atomic<float> loopLengthMs { 20.0f }; // 20ms micro-loop default
    std::atomic<float> crossfadeMs { 2.0f };

    std::atomic<float> envAttackMs { 1.0f };
    std::atomic<float> envHoldMs { 0.0f };
    std::atomic<float> envDecayMs { 250.0f };
    std::atomic<float> envSustain { 0.8f };
    std::atomic<float> envReleaseMs { 50.0f };
    std::atomic<float> envCurve { -0.5f };

    void interpolateSample(const juce::AudioBuffer<float>& buf, double pos, float& outL, float& outR) const noexcept;
};

} // namespace GlitchDSP
