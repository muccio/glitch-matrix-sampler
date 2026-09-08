#pragma once

#include <cmath>
#include <algorithm>

namespace GlitchDSP
{

class FastEnvelope
{
public:
    enum class Stage
    {
        Idle,
        Attack,
        Hold,
        Decay,
        Sustain,
        Release,
        ChokeRelease
    };

    FastEnvelope() = default;

    void prepare(double sampleRate) noexcept
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        reset();
    }

    void reset() noexcept
    {
        stage = Stage::Idle;
        currentLevel = 0.0f;
        stageSampleCounter = 0;
        stageTotalSamples = 0;
        startLevel = 0.0f;
        noteReleased = false;
    }

    void noteOn(float velocity = 1.0f) noexcept
    {
        vel = std::clamp(velocity, 0.0f, 1.0f);
        startLevel = currentLevel;
        stageSampleCounter = 0;
        stageTotalSamples = msToSamples(attackMs);
        stage = Stage::Attack;
        noteReleased = false;
    }

    void noteOff() noexcept
    {
        if (stage == Stage::Idle)
            return;

        noteReleased = true;

        // If sustain is 0.0 (percussive / one-shot mode):
        // The decay phase already takes the sound to silence naturally.
        // We do not abort attack or decay when sustain is zero!
        if (sustainLevel <= 0.001f)
            return;

        // If noteOff arrives while still in Attack or Hold:
        // Do not abort to Release from a near-zero level!
        // We let the voice reach peak (1.0f) during Attack/Hold,
        // and advancePostHold() will transition directly to Release.
        if (stage == Stage::Attack || stage == Stage::Hold)
            return;

        // In Decay or Sustain: transition to Release from current level
        startLevel = currentLevel;
        stageSampleCounter = 0;
        stageTotalSamples = msToSamples(releaseMs);
        stage = Stage::Release;
    }

    void choke() noexcept
    {
        if (stage == Stage::Idle)
            return;

        // Ultra-fast 0.5ms click-free release when choked
        startLevel = currentLevel;
        stageSampleCounter = 0;
        stageTotalSamples = msToSamples(0.5f);
        stage = Stage::ChokeRelease;
    }

    inline float getNextSample() noexcept
    {
        if (stage == Stage::Idle)
            return 0.0f;

        switch (stage)
        {
            case Stage::Attack:
            {
                if (stageSampleCounter >= stageTotalSamples)
                {
                    currentLevel = 1.0f;
                    enterHold();
                }
                else
                {
                    float t = static_cast<float>(stageSampleCounter) / static_cast<float>(stageTotalSamples);
                    currentLevel = startLevel + (1.0f - startLevel) * applyCurve(t, attackCurve);
                    ++stageSampleCounter;
                }
                break;
            }

            case Stage::Hold:
            {
                if (stageSampleCounter >= stageTotalSamples)
                {
                    advancePostHold();
                }
                else
                {
                    currentLevel = 1.0f;
                    ++stageSampleCounter;
                }
                break;
            }

            case Stage::Decay:
            {
                if (stageSampleCounter >= stageTotalSamples)
                {
                    if (sustainLevel <= 0.001f)
                    {
                        currentLevel = 0.0f;
                        stage = Stage::Idle;
                    }
                    else
                    {
                        currentLevel = sustainLevel;
                        if (noteReleased)
                        {
                            startLevel = currentLevel;
                            stageSampleCounter = 0;
                            stageTotalSamples = msToSamples(releaseMs);
                            stage = Stage::Release;
                        }
                        else
                        {
                            stage = Stage::Sustain;
                        }
                    }
                }
                else
                {
                    float t = static_cast<float>(stageSampleCounter) / static_cast<float>(stageTotalSamples);
                    currentLevel = 1.0f - (1.0f - sustainLevel) * applyCurve(t, decayCurve);
                    ++stageSampleCounter;
                }
                break;
            }

            case Stage::Sustain:
            {
                currentLevel = sustainLevel;
                break;
            }

            case Stage::Release:
            {
                if (stageSampleCounter >= stageTotalSamples)
                {
                    currentLevel = 0.0f;
                    stage = Stage::Idle;
                }
                else
                {
                    float t = static_cast<float>(stageSampleCounter) / static_cast<float>(stageTotalSamples);
                    currentLevel = startLevel * (1.0f - applyCurve(t, releaseCurve));
                    ++stageSampleCounter;
                }
                break;
            }

            case Stage::ChokeRelease:
            {
                if (stageSampleCounter >= stageTotalSamples)
                {
                    currentLevel = 0.0f;
                    stage = Stage::Idle;
                }
                else
                {
                    float t = static_cast<float>(stageSampleCounter) / static_cast<float>(stageTotalSamples);
                    float s = t * t * (3.0f - 2.0f * t);
                    currentLevel = startLevel * (1.0f - s);
                    ++stageSampleCounter;
                }
                break;
            }

            case Stage::Idle:
            default:
                return 0.0f;
        }

        return currentLevel * vel;
    }

    bool isActive() const noexcept { return stage != Stage::Idle; }
    Stage getStage() const noexcept { return stage; }
    float getCurrentLevel() const noexcept { return currentLevel; }

    void setAttackMs(float ms) noexcept { attackMs = std::max(0.05f, ms); }
    void setHoldMs(float ms) noexcept { holdMs = std::max(0.0f, ms); }
    void setDecayMs(float ms) noexcept { decayMs = std::max(0.1f, ms); }
    void setSustainLevel(float lvl) noexcept { sustainLevel = std::clamp(lvl, 0.0f, 1.0f); }
    void setReleaseMs(float ms) noexcept { releaseMs = std::max(0.1f, ms); }

    void setAttackCurve(float curve) noexcept { attackCurve = std::clamp(curve, -1.0f, 1.0f); }
    void setDecayCurve(float curve) noexcept { decayCurve = std::clamp(curve, -1.0f, 1.0f); }
    void setReleaseCurve(float curve) noexcept { releaseCurve = std::clamp(curve, -1.0f, 1.0f); }

    float getAttackMs() const noexcept { return attackMs; }
    float getHoldMs() const noexcept { return holdMs; }
    float getDecayMs() const noexcept { return decayMs; }
    float getSustainLevel() const noexcept { return sustainLevel; }
    float getReleaseMs() const noexcept { return releaseMs; }
    float getAttackCurve() const noexcept { return attackCurve; }
    float getDecayCurve() const noexcept { return decayCurve; }
    float getReleaseCurve() const noexcept { return releaseCurve; }

private:
    inline int msToSamples(float ms) const noexcept
    {
        return std::max(1, static_cast<int>(std::round(ms * 0.001 * currentSampleRate)));
    }

    void enterHold() noexcept
    {
        stageSampleCounter = 0;
        stageTotalSamples = msToSamples(holdMs);
        if (stageTotalSamples <= 0 || holdMs <= 0.001f)
            advancePostHold();
        else
            stage = Stage::Hold;
    }

    void advancePostHold() noexcept
    {
        if (noteReleased && sustainLevel > 0.001f)
        {
            // Note was already released during Attack or Hold:
            // Release directly from peak level (1.0f)!
            startLevel = 1.0f;
            stageSampleCounter = 0;
            stageTotalSamples = msToSamples(releaseMs);
            stage = Stage::Release;
        }
        else
        {
            // Note is still held OR sustain is 0.0 (percussive one-shot):
            // Proceed into Decay!
            enterDecay();
        }
    }

    void enterDecay() noexcept
    {
        stageSampleCounter = 0;
        stageTotalSamples = msToSamples(decayMs);
        if (stageTotalSamples <= 0)
        {
            if (sustainLevel <= 0.001f)
            {
                currentLevel = 0.0f;
                stage = Stage::Idle;
            }
            else
            {
                currentLevel = sustainLevel;
                if (noteReleased)
                {
                    startLevel = currentLevel;
                    stageSampleCounter = 0;
                    stageTotalSamples = msToSamples(releaseMs);
                    stage = Stage::Release;
                }
                else
                {
                    stage = Stage::Sustain;
                }
            }
        }
        else
        {
            stage = Stage::Decay;
        }
    }

    // Click-free continuous curve with smooth boundary transitions f'(0)=0 and f'(1)=0
    static inline float applyCurve(float t, float curve) noexcept
    {
        t = std::clamp(t, 0.0f, 1.0f);
        if (t <= 0.0f) return 0.0f;
        if (t >= 1.0f) return 1.0f;

        float u = t;
        if (curve > 0.01f)
        {
            // Logarithmic / punchy fast rise
            float p = 1.0f + curve * 3.0f;
            u = 1.0f - std::pow(1.0f - t, p);
        }
        else if (curve < -0.01f)
        {
            // Exponential / slow start rise
            float p = 1.0f + (-curve) * 3.0f;
            u = std::pow(t, p);
        }

        // Hermite smoothstep ensuring zero derivative at t=0 and t=1
        return u * u * (3.0f - 2.0f * u);
    }

    double currentSampleRate = 44100.0;
    Stage stage = Stage::Idle;
    float currentLevel = 0.0f;
    float startLevel = 0.0f;
    float vel = 1.0f;
    bool noteReleased = false;
    int stageSampleCounter = 0;
    int stageTotalSamples = 0;

    float attackMs = 2.0f;     // Default snappy 2ms
    float holdMs = 0.0f;
    float decayMs = 120.0f;
    float sustainLevel = 0.5f;
    float releaseMs = 80.0f;

    float attackCurve = -0.5f;  // Snappy exponential attack
    float decayCurve = -0.5f;   // Exponential decay
    float releaseCurve = -0.5f; // Exponential release
};

} // namespace GlitchDSP
