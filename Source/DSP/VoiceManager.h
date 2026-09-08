#pragma once

#include <vector>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <juce_audio_basics/juce_audio_basics.h>
#include "SoundSource.h"

namespace GlitchDSP
{

struct SourceGraph
{
    std::vector<std::shared_ptr<SoundSource>> sources;
    std::array<std::vector<int>, 128> noteToIndices;
    std::vector<int> omniIndices;
    std::array<std::vector<int>, 9> chokeGroupIndices; // 0 = none, 1..8 = choke group
};

class VoiceManager
{
public:
    VoiceManager();
    ~VoiceManager();

    // DSP Audio Thread methods (100% lock-free, wait-free, allocation-free)
    void prepare(double sampleRate, int maxBlockSize);
    void releaseResources();
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiMessages,
                      double hostBpm = 120.0,
                      double hostPpq = 0.0);

    // Audio thread telemetry
    int getActiveVoiceCount() const noexcept { return activeVoiceCount.load(std::memory_order_relaxed); }
    float getPeakLevelL() const noexcept { return peakLevelL.load(std::memory_order_relaxed); }
    float getPeakLevelR() const noexcept { return peakLevelR.load(std::memory_order_relaxed); }

    // Master volume control (atomic)
    float getMasterVolume() const noexcept { return masterVolume.load(std::memory_order_relaxed); }
    void setMasterVolume(float vol) noexcept { masterVolume.store(std::clamp(vol, 0.0f, 2.0f), std::memory_order_relaxed); }

    void panic() noexcept;

    // Message Thread methods (mutating source graph via RCU snapshot swap)
    void addSource(std::shared_ptr<SoundSource> source);
    bool removeSource(int sourceId);
    std::shared_ptr<SoundSource> cloneSource(int sourceId);
    std::shared_ptr<SoundSource> getSourceById(int sourceId) const;
    std::vector<std::shared_ptr<SoundSource>> getSourcesCopy() const;
    void clearAllSources();

    // Deferred reclamation (must be called periodically on Message Thread, e.g. 30Hz timer)
    void collectGarbage();

private:
    double currentSampleRate = 44100.0;
    int currentMaxBlockSize = 512;

    // RCU Atomic Snapshot
    std::atomic<const SourceGraph*> activeGraph { nullptr };
    std::atomic<const SourceGraph*> audioReadingGraph { nullptr };

    // Garbage collection list on message thread
    std::vector<const SourceGraph*> retiredGraphs;
    mutable std::mutex messageThreadMutex; // Protects only message-thread operations

    // Telemetry and Master
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<float> peakLevelL { 0.0f };
    std::atomic<float> peakLevelR { 0.0f };
    std::atomic<float> masterVolume { 1.0f };
    std::atomic<bool> panicRequested { false };

    // Source Graph building helper (UI thread only)
    static SourceGraph* buildGraph(const std::vector<std::shared_ptr<SoundSource>>& list);
};

} // namespace GlitchDSP
