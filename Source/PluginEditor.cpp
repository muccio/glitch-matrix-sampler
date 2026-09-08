#include "PluginProcessor.h"
#include "PluginEditor.h"

GlitchMatrixSamplerAudioProcessorEditor::GlitchMatrixSamplerAudioProcessorEditor(GlitchMatrixSamplerAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // Locate Frontend dist folder
    // 1. Check relative to current project directory
    juce::File currentFile(__FILE__);
    distDirectory = currentFile.getParentDirectory().getParentDirectory().getChildFile("Frontend").getChildFile("dist");

    // 2. If not found, check app bundle resources or executable directory
    if (!distDirectory.exists() || !distDirectory.getChildFile("index.html").exists())
    {
        auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        distDirectory = exeDir.getChildFile("Frontend").getChildFile("dist");
    }

    auto options = GlitchDSP::WebBridge::setupOptions(
        processorRef,
        [this]() -> juce::WebBrowserComponent* { return webBrowser.get(); },
        [this](const juce::String& url) { return getResource(url); }
    );

    webBrowser = std::make_unique<juce::WebBrowserComponent>(options);
    addAndMakeVisible(*webBrowser);

    webBridge = std::make_unique<GlitchDSP::WebBridge>(processorRef, *webBrowser);

    // Initial navigation to embedded UI
    webBrowser->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    setResizable(true, true);
    setResizeLimits(850, 550, 1920, 1200);
    setSize(1020, 680);
}

GlitchMatrixSamplerAudioProcessorEditor::~GlitchMatrixSamplerAudioProcessorEditor()
{
    webBridge.reset();
    webBrowser.reset();
}

void GlitchMatrixSamplerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff07090e));
}

void GlitchMatrixSamplerAudioProcessorEditor::resized()
{
    if (webBrowser != nullptr)
    {
        webBrowser->setBounds(getLocalBounds());
    }
}

juce::String GlitchMatrixSamplerAudioProcessorEditor::getMimeForExtension(const juce::String& ext)
{
    auto e = ext.toLowerCase();
    if (e == ".html" || e == ".htm") return "text/html";
    if (e == ".js" || e == ".mjs")   return "application/javascript";
    if (e == ".css")                 return "text/css";
    if (e == ".svg")                 return "image/svg+xml";
    if (e == ".png")                 return "image/png";
    if (e == ".jpg" || e == ".jpeg") return "image/jpeg";
    if (e == ".json")                return "application/json";
    if (e == ".woff2")               return "font/woff2";
    if (e == ".woff")                return "font/woff";
    if (e == ".ttf")                 return "font/ttf";
    return "application/octet-stream";
}

std::optional<juce::WebBrowserComponent::Resource> GlitchMatrixSamplerAudioProcessorEditor::getResource(const juce::String& url)
{
    juce::String cleanUrl = url;
    if (cleanUrl.startsWithChar('/'))
        cleanUrl = cleanUrl.substring(1);

    if (cleanUrl.isEmpty() || cleanUrl == "/" || cleanUrl.startsWith("index.html"))
    {
        cleanUrl = "index.html";
    }

    // Strip URL parameters / hash
    if (cleanUrl.containsChar('?'))
        cleanUrl = cleanUrl.upToFirstOccurrenceOf("?", false, false);
    if (cleanUrl.containsChar('#'))
        cleanUrl = cleanUrl.upToFirstOccurrenceOf("#", false, false);

    auto targetFile = distDirectory.getChildFile(cleanUrl);
    if (!targetFile.existsAsFile())
    {
        // Fallback to index.html for SPA client-side routing
        targetFile = distDirectory.getChildFile("index.html");
    }

    if (targetFile.existsAsFile())
    {
        juce::MemoryBlock memBlock;
        if (targetFile.loadFileAsData(memBlock))
        {
            std::vector<std::byte> byteVec(memBlock.getSize());
            std::memcpy(byteVec.data(), memBlock.getData(), memBlock.getSize());

            return juce::WebBrowserComponent::Resource {
                std::move(byteVec),
                getMimeForExtension(targetFile.getFileExtension())
            };
        }
    }

    return std::nullopt;
}
