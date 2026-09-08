#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../DSP/VoiceManager.h"
#include "../DSP/OscillatorSource.h"
#include "../DSP/NoiseSource.h"
#include "../DSP/SampleSource.h"
#include "../DSP/ClickSource.h"

namespace GlitchDSP
{

class StateSerializer
{
public:
    static juce::var serializeStateToVar(const VoiceManager& voiceManager);
    static juce::String serializeStateToJson(const VoiceManager& voiceManager);

    static bool deserializeStateFromVar(const juce::var& stateVar, VoiceManager& voiceManager, juce::AudioFormatManager* formatManager = nullptr);
    static bool deserializeStateFromJson(const juce::String& jsonString, VoiceManager& voiceManager, juce::AudioFormatManager* formatManager = nullptr);

    static juce::ValueTree serializeStateToValueTree(const VoiceManager& voiceManager);
    static bool deserializeStateFromValueTree(const juce::ValueTree& vt, VoiceManager& voiceManager, juce::AudioFormatManager* formatManager = nullptr);

    // Factory Presets
    static juce::Array<juce::var> getFactoryPresets();
    static void loadFactoryPreset(int presetIndex, VoiceManager& voiceManager, juce::AudioFormatManager* formatManager = nullptr);
};

} // namespace GlitchDSP
