#include "PluginProcessor.h"
#include "PluginEditor.h"

GlitchMatrixSamplerAudioProcessor::GlitchMatrixSamplerAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();

    // Initialize with Factory Preset 0 (Init Glitch Sine)
    GlitchDSP::StateSerializer::loadFactoryPreset(0, voiceManager, &formatManager);
}

GlitchMatrixSamplerAudioProcessor::~GlitchMatrixSamplerAudioProcessor()
{
}

const juce::String GlitchMatrixSamplerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool GlitchMatrixSamplerAudioProcessor::acceptsMidi() const
{
    return true;
}

bool GlitchMatrixSamplerAudioProcessor::producesMidi() const
{
    return false;
}

bool GlitchMatrixSamplerAudioProcessor::isMidiEffect() const
{
    return false;
}

double GlitchMatrixSamplerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int GlitchMatrixSamplerAudioProcessor::getNumPrograms()
{
    return 5;
}

int GlitchMatrixSamplerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void GlitchMatrixSamplerAudioProcessor::setCurrentProgram(int index)
{
    GlitchDSP::StateSerializer::loadFactoryPreset(index, voiceManager, &formatManager);
}

const juce::String GlitchMatrixSamplerAudioProcessor::getProgramName(int index)
{
    auto presets = GlitchDSP::StateSerializer::getFactoryPresets();
    if (index >= 0 && index < presets.size())
    {
        return presets[index].getProperty("name", "Preset").toString();
    }
    return {};
}

void GlitchMatrixSamplerAudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/)
{
}

void GlitchMatrixSamplerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    voiceManager.prepare(sampleRate, samplesPerBlock);
}

void GlitchMatrixSamplerAudioProcessor::releaseResources()
{
    voiceManager.releaseResources();
}

bool GlitchMatrixSamplerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void GlitchMatrixSamplerAudioProcessor::injectNoteOn(int noteNumber, float velocity)
{
    int start1, size1, start2, size2;
    uiMidiFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0)
    {
        uiMidiBuffer[start1] = { noteNumber, velocity, true };
        uiMidiFifo.finishedWrite(1);
    }
}

void GlitchMatrixSamplerAudioProcessor::injectNoteOff(int noteNumber)
{
    int start1, size1, start2, size2;
    uiMidiFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0)
    {
        uiMidiBuffer[start1] = { noteNumber, 0.0f, false };
        uiMidiFifo.finishedWrite(1);
    }
}

void GlitchMatrixSamplerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Drain UI Virtual Keyboard MIDI queue
    int numReady = uiMidiFifo.getNumReady();
    if (numReady > 0)
    {
        int start1, size1, start2, size2;
        uiMidiFifo.prepareToRead(numReady, start1, size1, start2, size2);

        for (int i = 0; i < size1; ++i)
        {
            const auto& ev = uiMidiBuffer[start1 + i];
            if (ev.isNoteOn)
                midiMessages.addEvent(juce::MidiMessage::noteOn(1, ev.note, ev.velocity), 0);
            else
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, ev.note, ev.velocity), 0);
        }

        for (int i = 0; i < size2; ++i)
        {
            const auto& ev = uiMidiBuffer[start2 + i];
            if (ev.isNoteOn)
                midiMessages.addEvent(juce::MidiMessage::noteOn(1, ev.note, ev.velocity), 0);
            else
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, ev.note, ev.velocity), 0);
        }

        uiMidiFifo.finishedRead(size1 + size2);
    }

    // Host timing & tempo
    double bpm = 120.0;
    double ppq = 0.0;
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (pos->getBpm()) bpm = *pos->getBpm();
            if (pos->getPpqPosition()) ppq = *pos->getPpqPosition();
        }
    }

    // Audio rendering
    voiceManager.processBlock(buffer, midiMessages, bpm, ppq);
}

bool GlitchMatrixSamplerAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* GlitchMatrixSamplerAudioProcessor::createEditor()
{
    return new GlitchMatrixSamplerAudioProcessorEditor(*this);
}

void GlitchMatrixSamplerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto vt = GlitchDSP::StateSerializer::serializeStateToValueTree(voiceManager);
    std::unique_ptr<juce::XmlElement> xml(vt.createXml());
    copyXmlToBinary(*xml, destData);
}

void GlitchMatrixSamplerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName("GlitchMatrixSamplerState"))
    {
        auto vt = juce::ValueTree::fromXml(*xmlState);
        GlitchDSP::StateSerializer::deserializeStateFromValueTree(vt, voiceManager, &formatManager);
    }
}

// JUCE Plugin Creation Entrypoint
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GlitchMatrixSamplerAudioProcessor();
}
