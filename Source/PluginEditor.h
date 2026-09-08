#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "IPC/WebBridge.h"

class GlitchMatrixSamplerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GlitchMatrixSamplerAudioProcessorEditor(GlitchMatrixSamplerAudioProcessor&);
    ~GlitchMatrixSamplerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

private:
    GlitchMatrixSamplerAudioProcessor& processorRef;
    std::unique_ptr<juce::WebBrowserComponent> webBrowser;
    std::unique_ptr<GlitchDSP::WebBridge> webBridge;

    juce::File distDirectory;
    static juce::String getMimeForExtension(const juce::String& ext);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlitchMatrixSamplerAudioProcessorEditor)
};
