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
    static constexpr int MAX_VOICES = 32;

    ClickSource(int id, const std::string& sourceName = "Click");
    ~ClickSource() override = default;

    void prepare(double sampleRate, int maxBlockSize) override;
    void noteOn(int noteNumber, float velocity) override;
    void noteOff(int noteNumber, float velocity) override;
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

    // Fast Envelope access (retained for backward compatibility)
    void setAttackMs(float ms) noexcept { envAttackMs.store(ms, std::memory_order_relaxed); }
    void setHoldMs(float ms) noexcept { envHoldMs.store(ms, std::memory_order_relaxed); }
    void setDecayMs(float ms) noexcept { envDecayMs.store(ms, std::memory_order_relaxed); }
    void setSustainLevel(float lvl) noexcept { envSustain.store(lvl, std::memory_order_relaxed); }
    void setReleaseMs(float ms) noexcept { envReleaseMs.store(ms, std::memory_order_relaxed); }
    void setCurveShape(float shape) noexcept { envCurve.store(shape, std::memory_order_relaxed); }

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
        int samplesRemaining = 0; // Pure 1-sample unit impulse
        float amplitude = 1.0f;
    };

    double currentSampleRate = 44100.0;
    std::array<Voice, MAX_VOICES> voices;
    std::atomic<int> activityHoldCounter { 0 };
    GlitchFX glitchFx;

    std::atomic<int> clickType { static_cast<int>(ClickType::Dirac) };
    std::atomic<int> pulseWidthSamples { 1 };        // Unit impulse: 1 sample
    std::atomic<float> clickFrequency { 1200.0f };
    std::atomic<float> clickDamping { 0.65f };
    std::atomic<bool> pitchTrack { false };
    std::atomic<int> polarity { 0 };                 // 0 = Positive (+1), 1 = Negative (-1), 2 = Bipolar alternating

    // Envelope parameters (retained for backward compatibility)
    std::atomic<float> envAttackMs { 0.0f };
    std::atomic<float> envHoldMs { 0.0f };
    std::atomic<float> envDecayMs { 0.0f };
    std::atomic<float> envSustain { 0.0f };
    std::atomic<float> envReleaseMs { 0.0f };
    std::atomic<float> envCurve { 0.0f };
};

} // namespace GlitchDSP
