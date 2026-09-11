#include "VoiceManager.h"

namespace GlitchDSP
{

VoiceManager::VoiceManager()
{
    auto* empty = new SourceGraph();
    activeGraph.store(empty, std::memory_order_release);
}

VoiceManager::~VoiceManager()
{
    // Clean up graphs
    const auto* cur = activeGraph.exchange(nullptr);
    delete cur;

    for (const auto* g : retiredGraphs)
        delete g;
    retiredGraphs.clear();
}

void VoiceManager::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    currentMaxBlockSize = maxBlockSize > 0 ? maxBlockSize : 512;

    const auto* cur = activeGraph.load(std::memory_order_acquire);
    if (cur != nullptr)
    {
        for (auto& s : cur->sources)
        {
            if (s)
                s->prepare(currentSampleRate, currentMaxBlockSize);
        }
    }
}

void VoiceManager::releaseResources()
{
    panic();
}

void VoiceManager::panic() noexcept
{
    panicRequested.store(true, std::memory_order_relaxed);
}

SourceGraph* VoiceManager::buildGraph(const std::vector<std::shared_ptr<SoundSource>>& list)
{
    auto* g = new SourceGraph();
    g->sources = list;

    for (size_t i = 0; i < list.size(); ++i)
    {
        const auto& src = list[i];
        if (!src)
            continue;

        int note = src->getAssignedNote();
        if (note >= 0 && note < 128)
        {
            g->noteToIndices[note].push_back(static_cast<int>(i));
        }
        else
        {
            g->omniIndices.push_back(static_cast<int>(i));
        }

        int cg = src->getChokeGroup();
        if (cg >= 1 && cg <= 8)
        {
            g->chokeGroupIndices[cg].push_back(static_cast<int>(i));
        }
    }

    return g;
}

void VoiceManager::addSource(std::shared_ptr<SoundSource> source)
{
    if (!source)
        return;

    std::lock_guard<std::mutex> lock(messageThreadMutex);
    source->prepare(currentSampleRate, currentMaxBlockSize);

    auto list = getSourcesCopy();
    list.push_back(source);

    auto* newG = buildGraph(list);
    const auto* oldG = activeGraph.exchange(newG, std::memory_order_acq_rel);
    if (oldG != nullptr)
        retiredGraphs.push_back(oldG);

    collectGarbage();
}

bool VoiceManager::removeSource(int sourceId)
{
    std::lock_guard<std::mutex> lock(messageThreadMutex);
    auto list = getSourcesCopy();

    auto it = std::remove_if(list.begin(), list.end(),
                             [sourceId](const std::shared_ptr<SoundSource>& s) {
                                 return s && s->getId() == sourceId;
                             });

    if (it == list.end())
        return false;

    list.erase(it, list.end());

    auto* newG = buildGraph(list);
    const auto* oldG = activeGraph.exchange(newG, std::memory_order_acq_rel);
    if (oldG != nullptr)
        retiredGraphs.push_back(oldG);

    collectGarbage();
    return true;
}

std::shared_ptr<SoundSource> VoiceManager::cloneSource(int sourceId)
{
    std::lock_guard<std::mutex> lock(messageThreadMutex);
    auto list = getSourcesCopy();

    std::shared_ptr<SoundSource> target = nullptr;
    int maxId = 0;
    for (const auto& s : list)
    {
        if (s)
        {
            if (s->getId() > maxId)
                maxId = s->getId();
            if (s->getId() == sourceId)
                target = s;
        }
    }

    if (!target)
        return nullptr;

    auto cloned = target->clone(maxId + 1);
    cloned->prepare(currentSampleRate, currentMaxBlockSize);
    list.push_back(cloned);

    auto* newG = buildGraph(list);
    const auto* oldG = activeGraph.exchange(newG, std::memory_order_acq_rel);
    if (oldG != nullptr)
        retiredGraphs.push_back(oldG);

    collectGarbage();
    return cloned;
}

std::shared_ptr<SoundSource> VoiceManager::getSourceById(int sourceId) const
{
    const auto* cur = activeGraph.load(std::memory_order_acquire);
    if (cur == nullptr)
        return nullptr;

    for (const auto& s : cur->sources)
    {
        if (s && s->getId() == sourceId)
            return s;
    }
    return nullptr;
}

std::vector<std::shared_ptr<SoundSource>> VoiceManager::getSourcesCopy() const
{
    const auto* cur = activeGraph.load(std::memory_order_acquire);
    if (cur == nullptr)
        return {};
    return cur->sources;
}

void VoiceManager::clearAllSources()
{
    std::lock_guard<std::mutex> lock(messageThreadMutex);
    auto* newG = new SourceGraph();
    const auto* oldG = activeGraph.exchange(newG, std::memory_order_acq_rel);
    if (oldG != nullptr)
        retiredGraphs.push_back(oldG);

    collectGarbage();
}

void VoiceManager::rebuildGraph()
{
    std::lock_guard<std::mutex> lock(messageThreadMutex);
    auto list = getSourcesCopy();
    auto* newG = buildGraph(list);
    const auto* oldG = activeGraph.exchange(newG, std::memory_order_acq_rel);
    if (oldG != nullptr)
        retiredGraphs.push_back(oldG);

    collectGarbage();
}

void VoiceManager::collectGarbage()
{
    if (retiredGraphs.empty())
        return;

    const auto* reading = audioReadingGraph.load(std::memory_order_acquire);
    std::vector<const SourceGraph*> remaining;

    for (const auto* g : retiredGraphs)
    {
        if (g != reading)
        {
            delete g;
        }
        else
        {
            remaining.push_back(g);
        }
    }

    retiredGraphs = std::move(remaining);
}

void VoiceManager::processBlock(juce::AudioBuffer<float>& buffer,
                                juce::MidiBuffer& midiMessages,
                                double hostBpm,
                                double hostPpq)
{
    const auto* graph = activeGraph.load(std::memory_order_acquire);
    audioReadingGraph.store(graph, std::memory_order_release);

    int numSamples = buffer.getNumSamples();
    buffer.clear();

    if (graph == nullptr || graph->sources.empty())
    {
        audioReadingGraph.store(nullptr, std::memory_order_release);
        activeVoiceCount.store(0, std::memory_order_relaxed);
        peakLevelL.store(0.0f, std::memory_order_relaxed);
        peakLevelR.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // Handle panic request
    if (panicRequested.exchange(false, std::memory_order_relaxed))
    {
        for (auto& s : graph->sources)
        {
            if (s) s->choke();
        }
    }

    // Helper lambda to dispatch a single MIDI message across the source graph
    auto dispatchMidiMessage = [graph](const juce::MidiMessage& msg)
    {
        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber();
            float vel = msg.getFloatVelocity();

            // Track all sources triggered by this MIDI event to avoid mutual sibling choking
            std::array<bool, 256> isTriggeredThisNote;
            isTriggeredThisNote.fill(false);

            if (note >= 0 && note < 128)
            {
                for (int idx : graph->noteToIndices[note])
                {
                    if (idx >= 0 && idx < static_cast<int>(isTriggeredThisNote.size()))
                        isTriggeredThisNote[idx] = true;
                }
            }

            for (int idx : graph->omniIndices)
            {
                if (idx >= 0 && idx < static_cast<int>(isTriggeredThisNote.size()))
                    isTriggeredThisNote[idx] = true;
            }

            // Note-specific sources
            if (note >= 0 && note < 128)
            {
                for (int idx : graph->noteToIndices[note])
                {
                    if (idx >= 0 && idx < static_cast<int>(graph->sources.size()))
                    {
                        auto& src = graph->sources[idx];
                        if (src)
                        {
                            int cg = src->getChokeGroup();
                            if (cg >= 1 && cg <= 8)
                            {
                                for (int otherIdx : graph->chokeGroupIndices[cg])
                                {
                                    // Do NOT choke sibling sources that are being triggered concurrently by the same MIDI event
                                    if (otherIdx < static_cast<int>(isTriggeredThisNote.size()) && !isTriggeredThisNote[otherIdx])
                                    {
                                        if (otherIdx < static_cast<int>(graph->sources.size()) && graph->sources[otherIdx])
                                            graph->sources[otherIdx]->choke();
                                    }
                                }
                            }
                            src->noteOn(note, vel);
                        }
                    }
                }
            }

            // Omni sources
            for (int idx : graph->omniIndices)
            {
                if (idx >= 0 && idx < static_cast<int>(graph->sources.size()))
                {
                    auto& src = graph->sources[idx];
                    if (src)
                    {
                        int cg = src->getChokeGroup();
                        if (cg >= 1 && cg <= 8)
                        {
                            for (int otherIdx : graph->chokeGroupIndices[cg])
                            {
                                // Do NOT choke sibling sources that are being triggered concurrently by the same MIDI event
                                if (otherIdx < static_cast<int>(isTriggeredThisNote.size()) && !isTriggeredThisNote[otherIdx])
                                {
                                    if (otherIdx < static_cast<int>(graph->sources.size()) && graph->sources[otherIdx])
                                        graph->sources[otherIdx]->choke();
                                }
                            }
                        }
                        src->noteOn(note, vel);
                    }
                }
            }
        }
        else if (msg.isNoteOff())
        {
            int note = msg.getNoteNumber();
            float vel = msg.getFloatVelocity();

            if (note >= 0 && note < 128)
            {
                for (int idx : graph->noteToIndices[note])
                {
                    if (idx >= 0 && idx < static_cast<int>(graph->sources.size()))
                    {
                        if (graph->sources[idx])
                            graph->sources[idx]->noteOff(note, vel);
                    }
                }
            }

            for (int idx : graph->omniIndices)
            {
                if (idx >= 0 && idx < static_cast<int>(graph->sources.size()))
                {
                    if (graph->sources[idx])
                        graph->sources[idx]->noteOff(note, vel);
                }
            }
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (auto& s : graph->sources)
            {
                if (s) s->choke();
            }
        }
    };

    // Check solo status
    bool anySoloed = false;
    for (const auto& s : graph->sources)
    {
        if (s && s->getSoloed())
        {
            anySoloed = true;
            break;
        }
    }

    // Sample-accurate rendering: split block into sub-blocks by MIDI event timestamps
    if (midiMessages.isEmpty())
    {
        for (auto& s : graph->sources)
        {
            if (!s) continue;
            if (anySoloed && !s->getSoloed()) continue;
            s->processBlock(buffer, 0, numSamples, hostBpm, hostPpq);
        }
    }
    else
    {
        int curSample = 0;
        auto midiIt = midiMessages.cbegin();
        auto midiEnd = midiMessages.cend();

        while (curSample < numSamples)
        {
            // Dispatch all MIDI events that occur at or before curSample
            while (midiIt != midiEnd && (*midiIt).samplePosition <= curSample)
            {
                dispatchMidiMessage((*midiIt).getMessage());
                ++midiIt;
            }

            // Find next MIDI event timestamp (clamped to buffer bounds)
            int nextMidiSample = numSamples;
            if (midiIt != midiEnd)
            {
                nextMidiSample = std::clamp((*midiIt).samplePosition, curSample, numSamples);
            }

            int samplesToRender = nextMidiSample - curSample;
            if (samplesToRender > 0)
            {
                for (auto& s : graph->sources)
                {
                    if (!s) continue;
                    if (anySoloed && !s->getSoloed()) continue;
                    s->processBlock(buffer, curSample, samplesToRender, hostBpm, hostPpq);
                }
                curSample = nextMidiSample;
            }
            else if (midiIt != midiEnd && (*midiIt).samplePosition <= curSample)
            {
                // Continue loop to dispatch another event at the same sample
            }
            else
            {
                curSample = nextMidiSample;
            }
        }

        // Dispatch any trailing MIDI messages with timestamp >= numSamples
        while (midiIt != midiEnd)
        {
            dispatchMidiMessage((*midiIt).getMessage());
            ++midiIt;
        }
    }

    // Telemetry voice counting
    int playingCount = 0;
    for (auto& s : graph->sources)
    {
        if (s && s->isPlaying())
            playingCount++;
    }

    // Apply master volume
    float master = masterVolume.load(std::memory_order_relaxed);
    if (master != 1.0f)
    {
        buffer.applyGain(master);
    }

    // Telemetry
    activeVoiceCount.store(playingCount, std::memory_order_relaxed);
    peakLevelL.store(buffer.getMagnitude(0, 0, numSamples), std::memory_order_relaxed);
    if (buffer.getNumChannels() > 1)
        peakLevelR.store(buffer.getMagnitude(1, 0, numSamples), std::memory_order_relaxed);
    else
        peakLevelR.store(peakLevelL.load(std::memory_order_relaxed), std::memory_order_relaxed);

    audioReadingGraph.store(nullptr, std::memory_order_release);
}

} // namespace GlitchDSP
