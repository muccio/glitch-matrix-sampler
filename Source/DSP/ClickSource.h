#pragma once

#include "SoundSource.h"
#include <array>
#include <cmath>

namespace GlitchDSP
{

enum class ClickType
{
    Dirac = 0,    // Pure needle impulse / micro-pulse (1 - 64 samples)
    Resonant = 1, // Damped sinusoidal pop e^(-d * t) * sin(2pi * f * t)
    Chirp = 2,    // Exponential micro-sweep blip (4*f -> f)
    BitFlip = 3   // Digital bit-flip burst / circuit-bent pop
};

class ClickSource : public SoundSource
{
public:
    static constexpr int MAX_VOICES = 8;

    ClickSource(int id, const std::string& sourceName = "Click");
    ~ClickSource() override = default;

    void prepare(double sampleRate, int maxBlockSize) override;
    void noteOn(int noteNumber, float velocity) override;
    void noteOff(float velocity) override;
    void choke() override;
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, double hostBpm, double hostPpq) override;
    bool isPlaying() const noexcept override;

    std::shared_ptr<SoundSource> clone(int newId) const override;
    juce::var toVar() const override;
    void fromVar(const juce::var& v) override;

    // Click-specific parameters
    ClickType getClickType() const noexcept { return static_cast<ClickType>(clickType.load(std::memory_order_relaxed)); }
    void setClickType(ClickType ct) noexcept { clickType.store(static_cast<int>(ct), std::memory_order_relaxed); }

    int getPulseWidthSamples() const noexcept { return pulseWidthSamples.load(std::memory_order_relaxed); }
    void setPulseWidthSamples(int samples) noexcept { pulseWidthSamples.store(std::clamp(samples, 1, 128), std::memory_order_relaxed); }

    float getClickFrequency() const noexcept { return clickFrequency.load(std::memory_order_relaxed); }
    void setClickFrequency(float hz) noexcept { clickFrequency.store(std::clamp(hz, 20.0f, 20000.0f), std::memory_order_relaxed); }

    float getClickDamping() const noexcept { return clickDamping.load(std::memory_order_relaxed); }
    void setClickDamping(float d) noexcept { clickDamping.store(std::clamp(d, 0.01f, 1.0f), std::memory_order_relaxed); }

    bool getPitchTrack() const noexcept { return pitchTrack.load(std::memory_order_relaxed); }
    void setPitchTrack(bool pt) noexcept { pitchTrack.store(pt, std::memory_order_relaxed); }

    int getPolarity() const noexcept { return polarity.load(std::memory_order_relaxed); }
    void setPolarity(int p) noexcept { polarity.store(std::clamp(p, 0, 2), std::memory_order_relaxed); }

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
        int noteNumber = -1;
        bool active = false;
        FastEnvelope envelope;
        int sampleIndex = 0;
        double phase = 0.0;
        float noteFreq = 1000.0f;
    };

    double currentSampleRate = 44100.0;
    std::array<Voice, MAX_VOICES> voices;
    GlitchFX glitchFx;

    std::atomic<int> clickType { static_cast<int>(ClickType::Dirac) };
    std::atomic<int> pulseWidthSamples { 4 };        // 4 samples needle
    std::atomic<float> clickFrequency { 1200.0f };   // 1.2 kHz resonant pop default
    std::atomic<float> clickDamping { 0.65f };       // Snappy damping
    std::atomic<bool> pitchTrack { false };          // Default fixed pitch for percussive clicks
    std::atomic<int> polarity { 0 };                 // 0 = Positive (+1), 1 = Negative (-1), 2 = Bipolar (+1/-1)

    // Envelope parameters (default ultra-snappy for clicks)
    std::atomic<float> envAttackMs { 0.05f };
    std::atomic<float> envHoldMs { 0.0f };
    std::atomic<float> envDecayMs { 30.0f };
    std::atomic<float> envSustain { 0.0f };
    std::atomic<float> envReleaseMs { 15.0f };
    std::atomic<float> envCurve { -0.6f };

    float generateClickSample(Voice& v, ClickType ct, int pw, float baseFreq, float damp, bool trackPitch, int pol) noexcept;
};

} // namespace GlitchDSP
