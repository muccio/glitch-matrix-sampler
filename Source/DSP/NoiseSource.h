#pragma once

#include "SoundSource.h"
#include <array>
#include <random>

namespace GlitchDSP
{

enum class NoiseType
{
    White = 0,
    Pink = 1,
    Crackle = 2,
    BitFlipHash = 3
};

class NoiseSource : public SoundSource
{
public:
    static constexpr int MAX_VOICES = 32;

    NoiseSource(int id, const std::string& sourceName = "Noise");
    ~NoiseSource() override = default;

    void prepare(double sampleRate, int maxBlockSize) override;
    void noteOn(int noteNumber, float velocity) override;
    void noteOff(int noteNumber, float velocity) override;
    void choke() override;
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, double hostBpm, double hostPpq) override;
    bool isPlaying() const noexcept override;

    std::shared_ptr<SoundSource> clone(int newId) const override;
    juce::var toVar() const override;
    void fromVar(const juce::var& v) override;

    NoiseType getNoiseType() const noexcept { return static_cast<NoiseType>(noiseType.load(std::memory_order_relaxed)); }
    void setNoiseType(NoiseType nt) noexcept { noiseType.store(static_cast<int>(nt), std::memory_order_relaxed); }

    float getCrackleDensity() const noexcept { return crackleDensity.load(std::memory_order_relaxed); }
    void setCrackleDensity(float d) noexcept { crackleDensity.store(std::clamp(d, 1.0f, 10000.0f), std::memory_order_relaxed); }

    float getHashRate() const noexcept { return hashRate.load(std::memory_order_relaxed); }
    void setHashRate(float hr) noexcept { hashRate.store(std::clamp(hr, 50.0f, 44100.0f), std::memory_order_relaxed); }

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
        uint32_t age = 0;
        bool active = false;
        FastEnvelope envelope;
        float pitchTracking = 1.0f;
    };

    double currentSampleRate = 44100.0;
    std::array<Voice, MAX_VOICES> voices;
    uint32_t nextVoiceAge = 1;
    GlitchFX glitchFx;

    std::atomic<int> noiseType { static_cast<int>(NoiseType::Crackle) };
    std::atomic<float> crackleDensity { 120.0f }; // impulses per sec
    std::atomic<float> hashRate { 4400.0f };      // clock rate for hash noise

    std::atomic<float> envAttackMs { 1.0f };      // fast click attack
    std::atomic<float> envHoldMs { 0.0f };
    std::atomic<float> envDecayMs { 80.0f };
    std::atomic<float> envSustain { 0.0f };      // percussive default
    std::atomic<float> envReleaseMs { 40.0f };
    std::atomic<float> envCurve { -0.8f };

    // PRNG and filter states
    uint32_t rngState = 123456789;
    inline uint32_t xorshift32() noexcept
    {
        uint32_t x = rngState;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rngState = x;
        return x;
    }

    // Pink noise filter state (Kellet filter)
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, b3 = 0.0f, b4 = 0.0f, b5 = 0.0f, b6 = 0.0f;

    // Hash noise state
    double hashPhase = 0.0;
    float currentHashSample = 0.0f;

    float generateNoise(NoiseType type, float pitchMult) noexcept;
};

} // namespace GlitchDSP
