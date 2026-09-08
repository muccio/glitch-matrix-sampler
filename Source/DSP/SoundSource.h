#pragma once

#include <string>
#include <atomic>
#include <memory>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "FastEnvelope.h"
#include "GlitchFX.h"

namespace GlitchDSP
{

enum class SourceType
{
    Oscillator = 0,
    Noise = 1,
    Sample = 2,
    Click = 3
};

class SoundSource
{
public:
    SoundSource(int id, const std::string& sourceName, SourceType sourceType)
        : sourceId(id), name(sourceName), type(sourceType)
    {
    }

    virtual ~SoundSource() = default;

    int getId() const noexcept { return sourceId; }
    const std::string& getName() const noexcept { return name; }
    void setName(const std::string& newName) { name = newName; }
    SourceType getType() const noexcept { return type; }

    // Routing & Choking
    int getAssignedNote() const noexcept { return assignedNote.load(std::memory_order_relaxed); }
    void setAssignedNote(int note) noexcept { assignedNote.store(note, std::memory_order_relaxed); }

    int getChokeGroup() const noexcept { return chokeGroup.load(std::memory_order_relaxed); }
    void setChokeGroup(int group) noexcept { chokeGroup.store(std::clamp(group, 0, 8), std::memory_order_relaxed); }

    bool getMuted() const noexcept { return isMuted.load(std::memory_order_relaxed); }
    void setMuted(bool muted) noexcept { isMuted.store(muted, std::memory_order_relaxed); }

    bool getSoloed() const noexcept { return isSoloed.load(std::memory_order_relaxed); }
    void setSoloed(bool soloed) noexcept { isSoloed.store(soloed, std::memory_order_relaxed); }

    // Common Mixing Parameters
    float getGain() const noexcept { return gain.load(std::memory_order_relaxed); }
    void setGain(float g) noexcept { gain.store(std::clamp(g, 0.0f, 4.0f), std::memory_order_relaxed); }

    float getPan() const noexcept { return pan.load(std::memory_order_relaxed); }
    void setPan(float p) noexcept { pan.store(std::clamp(p, -1.0f, 1.0f), std::memory_order_relaxed); }

    float getPitchSemi() const noexcept { return pitchSemi.load(std::memory_order_relaxed); }
    void setPitchSemi(float semi) noexcept { pitchSemi.store(std::clamp(semi, -48.0f, 48.0f), std::memory_order_relaxed); }

    float getPitchFine() const noexcept { return pitchFine.load(std::memory_order_relaxed); }
    void setPitchFine(float fine) noexcept { pitchFine.store(std::clamp(fine, -100.0f, 100.0f), std::memory_order_relaxed); }

    // DSP Lifecycle
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void noteOn(int noteNumber, float velocity) = 0;
    virtual void noteOff(float velocity) = 0;
    virtual void choke() = 0;
    virtual void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, double hostBpm, double hostPpq) = 0;
    virtual bool isPlaying() const noexcept = 0;

    // Cloning & Serialization
    virtual std::shared_ptr<SoundSource> clone(int newId) const = 0;
    virtual juce::var toVar() const = 0;
    virtual void fromVar(const juce::var& v) = 0;

protected:
    int sourceId = 0;
    std::string name;
    SourceType type;

    // Routing atomics
    std::atomic<int> assignedNote { -1 }; // -1 = all notes / omni
    std::atomic<int> chokeGroup { 0 };   // 0 = none, 1-8 = choke group
    std::atomic<bool> isMuted { false };
    std::atomic<bool> isSoloed { false };

    // Mixing atomics
    std::atomic<float> gain { 0.8f };
    std::atomic<float> pan { 0.0f };
    std::atomic<float> pitchSemi { 0.0f };
    std::atomic<float> pitchFine { 0.0f };
};

} // namespace GlitchDSP
