#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../DSP/VoiceManager.h"

class GlitchMatrixSamplerAudioProcessor;

namespace GlitchDSP
{

class WebBridge : public juce::Timer
{
public:
    WebBridge(GlitchMatrixSamplerAudioProcessor& proc, juce::WebBrowserComponent& browser);
    ~WebBridge() override;

    // Register all native C++ functions on the JUCE 8 WebBrowserComponent Options
    static juce::WebBrowserComponent::Options setupOptions(
        GlitchMatrixSamplerAudioProcessor& proc,
        std::function<juce::WebBrowserComponent*()> getBrowserFn,
        std::function<std::optional<juce::WebBrowserComponent::Resource>(const juce::String&)> resourceProviderFn);

    void sendStateSync();
    void sendVoiceStats();

    void timerCallback() override;

private:
    GlitchMatrixSamplerAudioProcessor& processor;
    juce::WebBrowserComponent& webBrowser;
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::FileChooser> fileChooser;
};

} // namespace GlitchDSP
