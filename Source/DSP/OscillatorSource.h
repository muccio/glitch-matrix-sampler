#pragma once

#include "SoundSource.h"
#include <array>

namespace GlitchDSP
{

enum class OscWaveform
{
    Sine = 0,
    Square = 1,
    Saw = 2,
    Triangle = 3,
    GlitchWavetable = 4
};

class OscillatorSource : public SoundSource
{
public:
    static constexpr int MAX_VOICES = 8;

    OscillatorSource(int id, const std::string& sourceName = "Oscillator");
    ~OscillatorSource() override = default;

    void prepare(double sampleRate, int maxBlockSize) override;
    void noteOn(int noteNumber, float velocity) override;
    void noteOff(float velocity) override;
    void choke() override;
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, double hostBpm, double hostPpq) override;
    bool isPlaying() const noexcept override;

    std::shared_ptr<SoundSource> clone(int newId) const override;
    juce::var toVar() const override;
    void fromVar(const juce::var& v) override;

    // Oscillator specific parameters
    OscWaveform getWaveform() const noexcept { return static_cast<OscWaveform>(waveform.load(std::memory_order_relaxed)); }
    void setWaveform(OscWaveform wf) noexcept { waveform.store(static_cast<int>(wf), std::memory_order_relaxed); }

    float getPulseWidth() const noexcept { return pulseWidth.load(std::memory_order_relaxed); }
    void setPulseWidth(float pw) noexcept { pulseWidth.store(std::clamp(pw, 0.05f, 0.95f), std::memory_order_relaxed); }

    float getGlitchMorph() const noexcept { return glitchMorph.load(std::memory_order_relaxed); }
    void setGlitchMorph(float m) noexcept { glitchMorph.store(std::clamp(m, 0.0f, 1.0f), std::memory_order_relaxed); }

    float getFrequency() const noexcept { return frequency.load(std::memory_order_relaxed); }
    void setFrequency(float f) noexcept;

    bool getPitchTrack() const noexcept { return pitchTrack.load(std::memory_order_relaxed); }
    void setPitchTrack(bool pt) noexcept;

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
        double phase = 0.0;
        double phaseInc = 0.0;
        int noteNumber = -1;
        bool active = false;
        FastEnvelope envelope;
    };

    double currentSampleRate = 44100.0;
    std::array<Voice, MAX_VOICES> voices;
    GlitchFX glitchFx;

    std::atomic<int> waveform { static_cast<int>(OscWaveform::Sine) };
    std::atomic<float> pulseWidth { 0.5f };
    std::atomic<float> glitchMorph { 0.0f };
    std::atomic<float> frequency { 440.0f };
    std::atomic<bool> pitchTrack { true };

    std::atomic<float> envAttackMs { 2.0f };
    std::atomic<float> envHoldMs { 0.0f };
    std::atomic<float> envDecayMs { 150.0f };
    std::atomic<float> envSustain { 0.6f };
    std::atomic<float> envReleaseMs { 100.0f };
    std::atomic<float> envCurve { -0.5f };

    // Anti-aliased PolyBLEP helpers
    static inline double polyBlep(double t, double dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        else if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    float generateSample(Voice& v, OscWaveform wf, float pw, float morph) noexcept;
};

} // namespace GlitchDSP
