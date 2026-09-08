#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "DSP/VoiceManager.h"
#include "IPC/StateSerializer.h"

class GlitchMatrixSamplerAudioProcessor : public juce::AudioProcessor
{
public:
    GlitchMatrixSamplerAudioProcessor();
    ~GlitchMatrixSamplerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    GlitchDSP::VoiceManager& getVoiceManager() noexcept { return voiceManager; }
    const GlitchDSP::VoiceManager& getVoiceManager() const noexcept { return voiceManager; }

    juce::AudioFormatManager& getFormatManager() noexcept { return formatManager; }

    // Thread-safe injection of UI virtual keyboard notes into processBlock
    void injectNoteOn(int noteNumber, float velocity);
    void injectNoteOff(int noteNumber);

private:
    GlitchDSP::VoiceManager voiceManager;
    juce::AudioFormatManager formatManager;

    // Lock-free FIFO for UI MIDI injection
    struct MidiEvent
    {
        int note = 0;
        float velocity = 0.0f;
        bool isNoteOn = false;
    };
    static constexpr int UI_MIDI_FIFO_SIZE = 64;
    std::array<MidiEvent, UI_MIDI_FIFO_SIZE> uiMidiBuffer;
    juce::AbstractFifo uiMidiFifo { UI_MIDI_FIFO_SIZE };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlitchMatrixSamplerAudioProcessor)
};
