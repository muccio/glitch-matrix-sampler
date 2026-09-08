#include "WebBridge.h"
#include "../PluginProcessor.h"
#include "StateSerializer.h"

namespace GlitchDSP
{

WebBridge::WebBridge(GlitchMatrixSamplerAudioProcessor& proc, juce::WebBrowserComponent& browser)
    : processor(proc), webBrowser(browser)
{
    formatManager.registerBasicFormats();
    startTimerHz(30); // 30 Hz timer for telemetry and GC
}

WebBridge::~WebBridge()
{
    stopTimer();
}

void WebBridge::timerCallback()
{
    // 1. Reclaim retired RCU snapshots on message thread
    processor.getVoiceManager().collectGarbage();

    // 2. Send live voice stats & meters to Web UI
    sendVoiceStats();
}

void WebBridge::sendStateSync()
{
    auto stateVar = StateSerializer::serializeStateToVar(processor.getVoiceManager());
    webBrowser.emitEventIfBrowserIsVisible("stateSync", stateVar);
}

void WebBridge::sendVoiceStats()
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("activeVoices", processor.getVoiceManager().getActiveVoiceCount());
    obj->setProperty("peakL", processor.getVoiceManager().getPeakLevelL());
    obj->setProperty("peakR", processor.getVoiceManager().getPeakLevelR());
    webBrowser.emitEventIfBrowserIsVisible("voiceStats", juce::var(obj));
}

juce::WebBrowserComponent::Options WebBridge::setupOptions(
    GlitchMatrixSamplerAudioProcessor& proc,
    std::function<juce::WebBrowserComponent*()> getBrowserFn,
    std::function<std::optional<juce::WebBrowserComponent::Resource>(const juce::String&)> resourceProviderFn)
{
    auto options = juce::WebBrowserComponent::Options{}
        .withResourceProvider(resourceProviderFn)
        .withNativeIntegrationEnabled();

    // 1. addSource(type)
    options = options.withNativeFunction("addSource", [&proc, getBrowserFn](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        juce::String type = args.size() > 0 ? args[0].toString() : "Oscillator";
        int newId = 1;
        auto sources = proc.getVoiceManager().getSourcesCopy();
        for (const auto& s : sources)
        {
            if (s && s->getId() >= newId)
                newId = s->getId() + 1;
        }

        std::shared_ptr<SoundSource> newSrc = nullptr;
        if (type == "Oscillator")
            newSrc = std::make_shared<OscillatorSource>(newId, "Oscillator " + std::to_string(newId));
        else if (type == "Noise")
            newSrc = std::make_shared<NoiseSource>(newId, "Noise " + std::to_string(newId));
        else if (type == "Sample")
            newSrc = std::make_shared<SampleSource>(newId, "Sample " + std::to_string(newId));

        if (newSrc)
        {
            proc.getVoiceManager().addSource(newSrc);
        }
        auto stateVar = StateSerializer::serializeStateToVar(proc.getVoiceManager());
        if (auto* b = getBrowserFn())
        {
            b->emitEventIfBrowserIsVisible("stateSync", stateVar);
        }
        completion(stateVar);
    });

    // 2. removeSource(sourceId)
    options = options.withNativeFunction("removeSource", [&proc, getBrowserFn](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() > 0)
        {
            int id = static_cast<int>(args[0]);
            proc.getVoiceManager().removeSource(id);
        }
        auto stateVar = StateSerializer::serializeStateToVar(proc.getVoiceManager());
        if (auto* b = getBrowserFn())
        {
            b->emitEventIfBrowserIsVisible("stateSync", stateVar);
        }
        completion(stateVar);
    });

    // 3. cloneSource(sourceId)
    options = options.withNativeFunction("cloneSource", [&proc, getBrowserFn](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() > 0)
        {
            int id = static_cast<int>(args[0]);
            proc.getVoiceManager().cloneSource(id);
        }
        auto stateVar = StateSerializer::serializeStateToVar(proc.getVoiceManager());
        if (auto* b = getBrowserFn())
        {
            b->emitEventIfBrowserIsVisible("stateSync", stateVar);
        }
        completion(stateVar);
    });

    // 4. updateParameter(sourceId, paramId, value)
    options = options.withNativeFunction("updateParameter", [&proc](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() >= 3)
        {
            int id = static_cast<int>(args[0]);
            juce::String paramId = args[1].toString();
            const auto& val = args[2];

            auto src = proc.getVoiceManager().getSourceById(id);
            if (src)
            {
                if (paramId == "name") src->setName(val.toString().toStdString());
                else if (paramId == "assignedNote") src->setAssignedNote(static_cast<int>(val));
                else if (paramId == "chokeGroup") src->setChokeGroup(static_cast<int>(val));
                else if (paramId == "muted") src->setMuted(static_cast<bool>(val));
                else if (paramId == "soloed") src->setSoloed(static_cast<bool>(val));
                else if (paramId == "gain") src->setGain(static_cast<float>(val));
                else if (paramId == "pan") src->setPan(static_cast<float>(val));
                else if (paramId == "pitchSemi") src->setPitchSemi(static_cast<float>(val));
                else if (paramId == "pitchFine") src->setPitchFine(static_cast<float>(val));
                else if (paramId == "attackMs")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get())) osc->setAttackMs(static_cast<float>(val));
                    else if (auto* ns = dynamic_cast<NoiseSource*>(src.get())) ns->setAttackMs(static_cast<float>(val));
                    else if (auto* sm = dynamic_cast<SampleSource*>(src.get())) sm->setAttackMs(static_cast<float>(val));
                }
                else if (paramId == "holdMs")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get())) osc->setHoldMs(static_cast<float>(val));
                    else if (auto* ns = dynamic_cast<NoiseSource*>(src.get())) ns->setHoldMs(static_cast<float>(val));
                    else if (auto* sm = dynamic_cast<SampleSource*>(src.get())) sm->setHoldMs(static_cast<float>(val));
                }
                else if (paramId == "decayMs")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get())) osc->setDecayMs(static_cast<float>(val));
                    else if (auto* ns = dynamic_cast<NoiseSource*>(src.get())) ns->setDecayMs(static_cast<float>(val));
                    else if (auto* sm = dynamic_cast<SampleSource*>(src.get())) sm->setDecayMs(static_cast<float>(val));
                }
                else if (paramId == "sustain")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get())) osc->setSustainLevel(static_cast<float>(val));
                    else if (auto* ns = dynamic_cast<NoiseSource*>(src.get())) ns->setSustainLevel(static_cast<float>(val));
                    else if (auto* sm = dynamic_cast<SampleSource*>(src.get())) sm->setSustainLevel(static_cast<float>(val));
                }
                else if (paramId == "releaseMs")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get())) osc->setReleaseMs(static_cast<float>(val));
                    else if (auto* ns = dynamic_cast<NoiseSource*>(src.get())) ns->setReleaseMs(static_cast<float>(val));
                    else if (auto* sm = dynamic_cast<SampleSource*>(src.get())) sm->setReleaseMs(static_cast<float>(val));
                }
                else if (paramId == "curve")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get())) osc->setCurveShape(static_cast<float>(val));
                    else if (auto* ns = dynamic_cast<NoiseSource*>(src.get())) ns->setCurveShape(static_cast<float>(val));
                    else if (auto* sm = dynamic_cast<SampleSource*>(src.get())) sm->setCurveShape(static_cast<float>(val));
                }
                else if (paramId == "bitDepth")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get())) osc->getGlitchFX().setBitDepth(static_cast<float>(val));
                    else if (auto* ns = dynamic_cast<NoiseSource*>(src.get())) ns->getGlitchFX().setBitDepth(static_cast<float>(val));
                    else if (auto* sm = dynamic_cast<SampleSource*>(src.get())) sm->getGlitchFX().setBitDepth(static_cast<float>(val));
                }
                else if (paramId == "bitcrushMix")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get())) osc->getGlitchFX().setBitcrushMix(static_cast<float>(val));
                    else if (auto* ns = dynamic_cast<NoiseSource*>(src.get())) ns->getGlitchFX().setBitcrushMix(static_cast<float>(val));
                    else if (auto* sm = dynamic_cast<SampleSource*>(src.get())) sm->getGlitchFX().setBitcrushMix(static_cast<float>(val));
                }
                else if (paramId == "downsampleHz")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get())) osc->getGlitchFX().setDownsampleHz(static_cast<float>(val));
                    else if (auto* ns = dynamic_cast<NoiseSource*>(src.get())) ns->getGlitchFX().setDownsampleHz(static_cast<float>(val));
                    else if (auto* sm = dynamic_cast<SampleSource*>(src.get())) sm->getGlitchFX().setDownsampleHz(static_cast<float>(val));
                }
                else if (paramId == "downsampleMix")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get())) osc->getGlitchFX().setDownsampleMix(static_cast<float>(val));
                    else if (auto* ns = dynamic_cast<NoiseSource*>(src.get())) ns->getGlitchFX().setDownsampleMix(static_cast<float>(val));
                    else if (auto* sm = dynamic_cast<SampleSource*>(src.get())) sm->getGlitchFX().setDownsampleMix(static_cast<float>(val));
                }
                else if (paramId == "stutterHz")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get())) osc->getGlitchFX().setStutterHz(static_cast<float>(val));
                    else if (auto* ns = dynamic_cast<NoiseSource*>(src.get())) ns->getGlitchFX().setStutterHz(static_cast<float>(val));
                    else if (auto* sm = dynamic_cast<SampleSource*>(src.get())) sm->getGlitchFX().setStutterHz(static_cast<float>(val));
                }
                else if (paramId == "stutterDuty")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get())) osc->getGlitchFX().setStutterDuty(static_cast<float>(val));
                    else if (auto* ns = dynamic_cast<NoiseSource*>(src.get())) ns->getGlitchFX().setStutterDuty(static_cast<float>(val));
                    else if (auto* sm = dynamic_cast<SampleSource*>(src.get())) sm->getGlitchFX().setStutterDuty(static_cast<float>(val));
                }
                else if (paramId == "stutterMix")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get())) osc->getGlitchFX().setStutterMix(static_cast<float>(val));
                    else if (auto* ns = dynamic_cast<NoiseSource*>(src.get())) ns->getGlitchFX().setStutterMix(static_cast<float>(val));
                    else if (auto* sm = dynamic_cast<SampleSource*>(src.get())) sm->getGlitchFX().setStutterMix(static_cast<float>(val));
                }
                else if (paramId == "stutterSync")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get())) osc->getGlitchFX().setStutterSync(static_cast<bool>(val));
                    else if (auto* ns = dynamic_cast<NoiseSource*>(src.get())) ns->getGlitchFX().setStutterSync(static_cast<bool>(val));
                    else if (auto* sm = dynamic_cast<SampleSource*>(src.get())) sm->getGlitchFX().setStutterSync(static_cast<bool>(val));
                }
                else if (paramId == "stutterDivision")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get())) osc->getGlitchFX().setStutterDivision(static_cast<int>(val));
                    else if (auto* ns = dynamic_cast<NoiseSource*>(src.get())) ns->getGlitchFX().setStutterDivision(static_cast<int>(val));
                    else if (auto* sm = dynamic_cast<SampleSource*>(src.get())) sm->getGlitchFX().setStutterDivision(static_cast<int>(val));
                }
                // Osc specific
                else if (paramId == "waveform")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get()))
                        osc->setWaveform(static_cast<OscWaveform>(static_cast<int>(val)));
                }
                else if (paramId == "pulseWidth")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get()))
                        osc->setPulseWidth(static_cast<float>(val));
                }
                else if (paramId == "glitchMorph")
                {
                    if (auto* osc = dynamic_cast<OscillatorSource*>(src.get()))
                        osc->setGlitchMorph(static_cast<float>(val));
                }
                // Noise specific
                else if (paramId == "noiseType")
                {
                    if (auto* ns = dynamic_cast<NoiseSource*>(src.get()))
                        ns->setNoiseType(static_cast<NoiseType>(static_cast<int>(val)));
                }
                else if (paramId == "crackleDensity")
                {
                    if (auto* ns = dynamic_cast<NoiseSource*>(src.get()))
                        ns->setCrackleDensity(static_cast<float>(val));
                }
                else if (paramId == "hashRate")
                {
                    if (auto* ns = dynamic_cast<NoiseSource*>(src.get()))
                        ns->setHashRate(static_cast<float>(val));
                }
                // Sample specific
                else if (paramId == "startPoint")
                {
                    if (auto* sm = dynamic_cast<SampleSource*>(src.get()))
                        sm->setStartPoint(static_cast<float>(val));
                }
                else if (paramId == "endPoint")
                {
                    if (auto* sm = dynamic_cast<SampleSource*>(src.get()))
                        sm->setEndPoint(static_cast<float>(val));
                }
                else if (paramId == "reverse")
                {
                    if (auto* sm = dynamic_cast<SampleSource*>(src.get()))
                        sm->setReverse(static_cast<bool>(val));
                }
                else if (paramId == "speed")
                {
                    if (auto* sm = dynamic_cast<SampleSource*>(src.get()))
                        sm->setSpeed(static_cast<float>(val));
                }
                else if (paramId == "microLoop")
                {
                    if (auto* sm = dynamic_cast<SampleSource*>(src.get()))
                        sm->setMicroLoop(static_cast<bool>(val));
                }
                else if (paramId == "loopStart")
                {
                    if (auto* sm = dynamic_cast<SampleSource*>(src.get()))
                        sm->setLoopStart(static_cast<float>(val));
                }
                else if (paramId == "loopLengthMs")
                {
                    if (auto* sm = dynamic_cast<SampleSource*>(src.get()))
                        sm->setLoopLengthMs(static_cast<float>(val));
                }
                else if (paramId == "crossfadeMs")
                {
                    if (auto* sm = dynamic_cast<SampleSource*>(src.get()))
                        sm->setCrossfadeMs(static_cast<float>(val));
                }
            }
        }
        completion(juce::var(true));
    });

    // 5. openFileDialog(sourceId)
    options = options.withNativeFunction("openFileDialog", [&proc, getBrowserFn](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() > 0)
        {
            int id = static_cast<int>(args[0]);
            auto src = proc.getVoiceManager().getSourceById(id);
            if (auto* sm = dynamic_cast<SampleSource*>(src.get()))
            {
                auto chooser = std::make_shared<juce::FileChooser>(
                    "Select an audio file (WAV, AIFF, FLAC)...",
                    juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                    "*.wav;*.aif;*.aiff;*.flac"
                );

                chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [&proc, getBrowserFn, sm, chooser](const juce::FileChooser& fc) {
                        auto result = fc.getResult();
                        if (result.existsAsFile())
                        {
                            sm->loadFile(result, proc.getFormatManager());
                            if (auto* b = getBrowserFn())
                            {
                                auto stateVar = StateSerializer::serializeStateToVar(proc.getVoiceManager());
                                b->emitEventIfBrowserIsVisible("stateSync", stateVar);
                                b->emitEventIfBrowserIsVisible("sampleWaveform", sm->getWaveformPeaks(256));
                            }
                        }
                    });
            }
        }
        completion(juce::var(true));
    });

    // 6. setMasterVolume(vol)
    options = options.withNativeFunction("setMasterVolume", [&proc](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() > 0)
            proc.getVoiceManager().setMasterVolume(static_cast<float>(args[0]));
        completion(juce::var(true));
    });

    // 7. panic()
    options = options.withNativeFunction("panic", [&proc](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        proc.getVoiceManager().panic();
        completion(juce::var(true));
    });

    // 8. loadPreset(index)
    options = options.withNativeFunction("loadPreset", [&proc, getBrowserFn](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() > 0)
        {
            int idx = static_cast<int>(args[0]);
            StateSerializer::loadFactoryPreset(idx, proc.getVoiceManager(), &proc.getFormatManager());
        }
        auto stateVar = StateSerializer::serializeStateToVar(proc.getVoiceManager());
        if (auto* b = getBrowserFn())
        {
            b->emitEventIfBrowserIsVisible("stateSync", stateVar);
        }
        completion(stateVar);
    });

    // 9. noteOn(note, velocity)
    options = options.withNativeFunction("noteOn", [&proc](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() >= 2)
        {
            int note = static_cast<int>(args[0]);
            float vel = static_cast<float>(args[1]);
            proc.injectNoteOn(note, vel);
        }
        completion(juce::var(true));
    });

    // 10. noteOff(note)
    options = options.withNativeFunction("noteOff", [&proc](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() > 0)
        {
            int note = static_cast<int>(args[0]);
            proc.injectNoteOff(note);
        }
        completion(juce::var(true));
    });

    // 11. requestState()
    options = options.withNativeFunction("requestState", [&proc, getBrowserFn](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        auto stateVar = StateSerializer::serializeStateToVar(proc.getVoiceManager());
        if (auto* b = getBrowserFn())
        {
            b->emitEventIfBrowserIsVisible("stateSync", stateVar);
        }
        completion(stateVar);
    });

    return options;
}

} // namespace GlitchDSP
