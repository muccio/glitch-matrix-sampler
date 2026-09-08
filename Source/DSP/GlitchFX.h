#pragma once

#include <cmath>
#include <algorithm>
#include <juce_audio_basics/juce_audio_basics.h>

namespace GlitchDSP
{

class GlitchFX
{
public:
    GlitchFX() = default;

    void prepare(double sampleRate) noexcept
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        reset();
    }

    void reset() noexcept
    {
        srrPhaseL = 0.0;
        srrPhaseR = 0.0;
        srrHeldL = 0.0f;
        srrHeldR = 0.0f;

        stutterPhase = 0.0;
        gateGain = 1.0f;
    }

    // Process a single sample through Bitcrusher, Sample Rate Reducer, and Stutter
    inline void processSample(float& sampleL, float& sampleR, double hostBpm = 120.0) noexcept
    {
        // 1. Bitcrusher
        if (bitcrushMix > 0.001f)
        {
            float crushedL = processBitcrush(sampleL, bitDepth);
            float crushedR = processBitcrush(sampleR, bitDepth);
            sampleL = (1.0f - bitcrushMix) * sampleL + bitcrushMix * crushedL;
            sampleR = (1.0f - bitcrushMix) * sampleR + bitcrushMix * crushedR;
        }

        // 2. Sample Rate Reducer (Downsampler)
        if (downsampleMix > 0.001f && downsampleHz < currentSampleRate)
        {
            double phaseInc = std::clamp(downsampleHz / currentSampleRate, 0.0001, 1.0);
            srrPhaseL += phaseInc;
            if (srrPhaseL >= 1.0)
            {
                srrPhaseL -= std::floor(srrPhaseL);
                srrHeldL = sampleL;
            }

            srrPhaseR += phaseInc;
            if (srrPhaseR >= 1.0)
            {
                srrPhaseR -= std::floor(srrPhaseR);
                srrHeldR = sampleR;
            }

            sampleL = (1.0f - downsampleMix) * sampleL + downsampleMix * srrHeldL;
            sampleR = (1.0f - downsampleMix) * sampleR + downsampleMix * srrHeldR;
        }

        // 3. Stutter / Micro-Gater
        if (stutterMix > 0.001f)
        {
            double effRateHz = getEffectiveStutterRate(hostBpm);
            double phaseInc = effRateHz / currentSampleRate;
            stutterPhase += phaseInc;
            if (stutterPhase >= 1.0)
                stutterPhase -= std::floor(stutterPhase);

            // Gate calculation with smoothed edges (0.5ms smooth ramp)
            float targetGain = (stutterPhase < stutterDuty) ? 1.0f : 0.0f;
            float smoothCoeff = 0.05f; // fast smoothing filter
            gateGain += smoothCoeff * (targetGain - gateGain);

            float gatedL = sampleL * gateGain;
            float gatedR = sampleR * gateGain;
            sampleL = (1.0f - stutterMix) * sampleL + stutterMix * gatedL;
            sampleR = (1.0f - stutterMix) * sampleR + stutterMix * gatedR;
        }
    }

    // Parameters setters
    void setBitDepth(float bits) noexcept { bitDepth = std::clamp(bits, 1.0f, 16.0f); }
    void setBitcrushMix(float mix) noexcept { bitcrushMix = std::clamp(mix, 0.0f, 1.0f); }

    void setDownsampleHz(float hz) noexcept { downsampleHz = std::clamp(hz, 50.0f, 48000.0f); }
    void setDownsampleMix(float mix) noexcept { downsampleMix = std::clamp(mix, 0.0f, 1.0f); }

    void setStutterHz(float hz) noexcept { stutterHz = std::clamp(hz, 0.5f, 200.0f); }
    void setStutterDuty(float duty) noexcept { stutterDuty = std::clamp(duty, 0.01f, 0.99f); }
    void setStutterMix(float mix) noexcept { stutterMix = std::clamp(mix, 0.0f, 1.0f); }
    void setStutterSync(bool sync) noexcept { stutterSync = sync; }
    void setStutterDivision(int divIndex) noexcept { stutterDivision = std::clamp(divIndex, 0, 7); }

    float getBitDepth() const noexcept { return bitDepth; }
    float getBitcrushMix() const noexcept { return bitcrushMix; }
    float getDownsampleHz() const noexcept { return downsampleHz; }
    float getDownsampleMix() const noexcept { return downsampleMix; }
    float getStutterHz() const noexcept { return stutterHz; }
    float getStutterDuty() const noexcept { return stutterDuty; }
    float getStutterMix() const noexcept { return stutterMix; }
    bool getStutterSync() const noexcept { return stutterSync; }
    int getStutterDivision() const noexcept { return stutterDivision; }

private:
    static inline float processBitcrush(float in, float bits) noexcept
    {
        float steps = std::pow(2.0f, bits - 1.0f);
        if (steps < 1.0f) steps = 1.0f;
        return std::clamp(std::round(in * steps) / steps, -1.0f, 1.0f);
    }

    double getEffectiveStutterRate(double bpm) const noexcept
    {
        if (!stutterSync || bpm <= 0.0)
            return stutterHz;

        // Divisions: 0=1/4, 1=1/8, 2=1/16, 3=1/32, 4=1/64, 5=1/128, 6=1/8T, 7=1/16T
        double beatsPerSecond = bpm / 60.0;
        switch (stutterDivision)
        {
            case 0: return beatsPerSecond * 1.0;   // 1/4
            case 1: return beatsPerSecond * 2.0;   // 1/8
            case 2: return beatsPerSecond * 4.0;   // 1/16
            case 3: return beatsPerSecond * 8.0;   // 1/32
            case 4: return beatsPerSecond * 16.0;  // 1/64
            case 5: return beatsPerSecond * 32.0;  // 1/128
            case 6: return beatsPerSecond * 3.0;   // 1/8 triplet
            case 7: return beatsPerSecond * 6.0;   // 1/16 triplet
            default: return beatsPerSecond * 4.0;
        }
    }

    double currentSampleRate = 44100.0;

    // Bitcrusher
    float bitDepth = 16.0f;
    float bitcrushMix = 0.0f;

    // Sample Rate Reducer
    float downsampleHz = 44100.0f;
    float downsampleMix = 0.0f;
    double srrPhaseL = 0.0;
    double srrPhaseR = 0.0;
    float srrHeldL = 0.0f;
    float srrHeldR = 0.0f;

    // Stutter / Micro-Gater
    float stutterHz = 8.0f;
    float stutterDuty = 0.5f;
    float stutterMix = 0.0f;
    bool stutterSync = false;
    int stutterDivision = 2; // 1/16 note default
    double stutterPhase = 0.0;
    float gateGain = 1.0f;
};

} // namespace GlitchDSP
