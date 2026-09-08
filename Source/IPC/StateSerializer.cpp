#include "StateSerializer.h"

namespace GlitchDSP
{

juce::var StateSerializer::serializeStateToVar(const VoiceManager& voiceManager)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("version", "1.0.0");
    root->setProperty("masterVolume", voiceManager.getMasterVolume());

    juce::Array<juce::var> sourcesArray;
    auto sources = voiceManager.getSourcesCopy();
    for (const auto& s : sources)
    {
        if (s)
            sourcesArray.add(s->toVar());
    }
    root->setProperty("sources", juce::var(sourcesArray));

    return juce::var(root);
}

juce::String StateSerializer::serializeStateToJson(const VoiceManager& voiceManager)
{
    return juce::JSON::toString(serializeStateToVar(voiceManager), true);
}

bool StateSerializer::deserializeStateFromVar(const juce::var& stateVar, VoiceManager& voiceManager, juce::AudioFormatManager* formatManager)
{
    if (!stateVar.isObject())
        return false;

    auto* root = stateVar.getDynamicObject();
    if (root == nullptr)
        return false;

    if (root->hasProperty("masterVolume"))
    {
        voiceManager.setMasterVolume(static_cast<float>(root->getProperty("masterVolume")));
    }

    if (root->hasProperty("sources") && root->getProperty("sources").isArray())
    {
        voiceManager.clearAllSources();
        const auto* arr = root->getProperty("sources").getArray();

        for (const auto& sVar : *arr)
        {
            if (!sVar.isObject())
                continue;

            auto* obj = sVar.getDynamicObject();
            if (obj == nullptr)
                continue;

            int id = static_cast<int>(obj->getProperty("id"));
            juce::String typeStr = obj->getProperty("type").toString();
            juce::String nameStr = obj->getProperty("name").toString();

            std::shared_ptr<SoundSource> newSource = nullptr;

            if (typeStr == "Oscillator")
            {
                newSource = std::make_shared<OscillatorSource>(id, nameStr.toStdString());
            }
            else if (typeStr == "Noise")
            {
                newSource = std::make_shared<NoiseSource>(id, nameStr.toStdString());
            }
            else if (typeStr == "Sample")
            {
                auto sampleSrc = std::make_shared<SampleSource>(id, nameStr.toStdString());
                if (formatManager != nullptr && obj->hasProperty("filePath"))
                {
                    juce::String path = obj->getProperty("filePath").toString();
                    if (path.isNotEmpty())
                    {
                        juce::File f(path);
                        if (f.existsAsFile())
                        {
                            sampleSrc->loadFile(f, *formatManager);
                        }
                    }
                }
                newSource = sampleSrc;
            }

            if (newSource)
            {
                newSource->fromVar(sVar);
                voiceManager.addSource(newSource);
            }
        }
    }

    return true;
}

bool StateSerializer::deserializeStateFromJson(const juce::String& jsonString, VoiceManager& voiceManager, juce::AudioFormatManager* formatManager)
{
    auto parsed = juce::JSON::parse(jsonString);
    return deserializeStateFromVar(parsed, voiceManager, formatManager);
}

juce::ValueTree StateSerializer::serializeStateToValueTree(const VoiceManager& voiceManager)
{
    juce::ValueTree vt("GlitchMatrixSamplerState");
    vt.setProperty("masterVolume", voiceManager.getMasterVolume(), nullptr);

    juce::String jsonStr = serializeStateToJson(voiceManager);
    vt.setProperty("jsonState", jsonStr, nullptr);

    return vt;
}

bool StateSerializer::deserializeStateFromValueTree(const juce::ValueTree& vt, VoiceManager& voiceManager, juce::AudioFormatManager* formatManager)
{
    if (!vt.hasType("GlitchMatrixSamplerState"))
        return false;

    if (vt.hasProperty("jsonState"))
    {
        juce::String jsonStr = vt.getProperty("jsonState").toString();
        return deserializeStateFromJson(jsonStr, voiceManager, formatManager);
    }
    return false;
}

juce::Array<juce::var> StateSerializer::getFactoryPresets()
{
    juce::Array<juce::var> presets;

    // Preset 0: Init Patch
    {
        auto* p = new juce::DynamicObject();
        p->setProperty("name", "01. Init Glitch Sine");
        p->setProperty("description", "Pure sine sub with snappy transient click");

        auto* root = new juce::DynamicObject();
        root->setProperty("masterVolume", 1.0f);

        juce::Array<juce::var> sources;
        {
            auto osc = std::make_shared<OscillatorSource>(1, "Sub Sine");
            osc->setWaveform(OscWaveform::Sine);
            osc->setAttackMs(1.0f);
            osc->setDecayMs(350.0f);
            osc->setSustainLevel(0.0f);
            osc->setReleaseMs(50.0f);
            sources.add(osc->toVar());
        }
        {
            auto noise = std::make_shared<NoiseSource>(2, "Click Transient");
            noise->setNoiseType(NoiseType::Crackle);
            noise->setCrackleDensity(2000.0f);
            noise->setAttackMs(0.1f); // sub-ms click
            noise->setDecayMs(15.0f);
            noise->setSustainLevel(0.0f);
            noise->setReleaseMs(10.0f);
            sources.add(noise->toVar());
        }
        root->setProperty("sources", juce::var(sources));
        p->setProperty("state", juce::var(root));
        presets.add(juce::var(p));
    }

    // Preset 1: Clicks & Cuts IDM
    {
        auto* p = new juce::DynamicObject();
        p->setProperty("name", "02. Clicks & Cuts IDM");
        p->setProperty("description", "High-density Poisson crackle with bitcrushed tonal clicks");

        auto* root = new juce::DynamicObject();
        root->setProperty("masterVolume", 0.95f);

        juce::Array<juce::var> sources;
        {
            auto osc = std::make_shared<OscillatorSource>(1, "Glitch Tone");
            osc->setWaveform(OscWaveform::Square);
            osc->setPulseWidth(0.25f);
            osc->setAttackMs(0.5f);
            osc->setDecayMs(45.0f);
            osc->setSustainLevel(0.0f);
            osc->setReleaseMs(20.0f);
            osc->getGlitchFX().setBitDepth(4.0f);
            osc->getGlitchFX().setBitcrushMix(0.85f);
            osc->setChokeGroup(1);
            sources.add(osc->toVar());
        }
        {
            auto noise = std::make_shared<NoiseSource>(2, "Poisson Crackle");
            noise->setNoiseType(NoiseType::Crackle);
            noise->setCrackleDensity(350.0f);
            noise->setAttackMs(0.1f);
            noise->setDecayMs(80.0f);
            noise->setSustainLevel(0.0f);
            noise->setReleaseMs(30.0f);
            noise->setChokeGroup(1);
            sources.add(noise->toVar());
        }
        {
            auto noise2 = std::make_shared<NoiseSource>(3, "Digital Hiss");
            noise2->setNoiseType(NoiseType::BitFlipHash);
            noise2->setHashRate(1200.0f);
            noise2->setAttackMs(1.0f);
            noise2->setDecayMs(120.0f);
            noise2->setSustainLevel(0.0f);
            noise2->setReleaseMs(40.0f);
            noise2->getGlitchFX().setDownsampleHz(800.0f);
            noise2->getGlitchFX().setDownsampleMix(1.0f);
            sources.add(noise2->toVar());
        }
        root->setProperty("sources", juce::var(sources));
        p->setProperty("state", juce::var(root));
        presets.add(juce::var(p));
    }

    // Preset 2: Cyberpunk Glitch Bass
    {
        auto* p = new juce::DynamicObject();
        p->setProperty("name", "03. Cyberpunk Glitch Bass");
        p->setProperty("description", "Morphable fold wavetable with rate reduction and stutter");

        auto* root = new juce::DynamicObject();
        root->setProperty("masterVolume", 0.9f);

        juce::Array<juce::var> sources;
        {
            auto osc = std::make_shared<OscillatorSource>(1, "Fold Wavetable");
            osc->setWaveform(OscWaveform::GlitchWavetable);
            osc->setGlitchMorph(0.75f);
            osc->setPitchSemi(-12.0f);
            osc->setAttackMs(2.0f);
            osc->setDecayMs(500.0f);
            osc->setSustainLevel(0.7f);
            osc->setReleaseMs(120.0f);
            osc->getGlitchFX().setDownsampleHz(2200.0f);
            osc->getGlitchFX().setDownsampleMix(0.6f);
            osc->getGlitchFX().setStutterHz(16.0f);
            osc->getGlitchFX().setStutterDuty(0.65f);
            osc->getGlitchFX().setStutterMix(0.5f);
            sources.add(osc->toVar());
        }
        root->setProperty("sources", juce::var(sources));
        p->setProperty("state", juce::var(root));
        presets.add(juce::var(p));
    }

    // Preset 3: Micro-Stutter Glitch
    {
        auto* p = new juce::DynamicObject();
        p->setProperty("name", "04. Micro-Stutter Machine");
        p->setProperty("description", "PolyBLEP saw run through high-frequency stutter gater");

        auto* root = new juce::DynamicObject();
        root->setProperty("masterVolume", 0.85f);

        juce::Array<juce::var> sources;
        {
            auto osc = std::make_shared<OscillatorSource>(1, "Stutter Saw");
            osc->setWaveform(OscWaveform::Saw);
            osc->setAttackMs(0.5f);
            osc->setDecayMs(600.0f);
            osc->setSustainLevel(0.5f);
            osc->setReleaseMs(80.0f);
            osc->getGlitchFX().setStutterHz(32.0f);
            osc->getGlitchFX().setStutterDuty(0.5f);
            osc->getGlitchFX().setStutterMix(0.9f);
            sources.add(osc->toVar());
        }
        root->setProperty("sources", juce::var(sources));
        p->setProperty("state", juce::var(root));
        presets.add(juce::var(p));
    }

    // Preset 4: Digital Entropy
    {
        auto* p = new juce::DynamicObject();
        p->setProperty("name", "05. Digital Entropy");
        p->setProperty("description", "Bit-flip noise generator with choked multi-layer percussions");

        auto* root = new juce::DynamicObject();
        root->setProperty("masterVolume", 0.9f);

        juce::Array<juce::var> sources;
        {
            auto noise = std::make_shared<NoiseSource>(1, "Hash Click");
            noise->setNoiseType(NoiseType::BitFlipHash);
            noise->setHashRate(8000.0f);
            noise->setAttackMs(0.2f);
            noise->setDecayMs(30.0f);
            noise->setSustainLevel(0.0f);
            noise->setReleaseMs(15.0f);
            noise->setChokeGroup(2);
            sources.add(noise->toVar());
        }
        {
            auto noise2 = std::make_shared<NoiseSource>(2, "Pink Tail");
            noise2->setNoiseType(NoiseType::Pink);
            noise2->setAttackMs(1.0f);
            noise2->setDecayMs(180.0f);
            noise2->setSustainLevel(0.0f);
            noise2->setReleaseMs(60.0f);
            noise2->getGlitchFX().setBitDepth(3.0f);
            noise2->getGlitchFX().setBitcrushMix(0.8f);
            noise2->setChokeGroup(2);
            sources.add(noise2->toVar());
        }
        root->setProperty("sources", juce::var(sources));
        p->setProperty("state", juce::var(root));
        presets.add(juce::var(p));
    }

    return presets;
}

void StateSerializer::loadFactoryPreset(int presetIndex, VoiceManager& voiceManager, juce::AudioFormatManager* formatManager)
{
    auto presets = getFactoryPresets();
    if (presetIndex >= 0 && presetIndex < presets.size())
    {
        auto pVar = presets[presetIndex];
        if (pVar.isObject())
        {
            auto* pObj = pVar.getDynamicObject();
            if (pObj != nullptr && pObj->hasProperty("state"))
            {
                deserializeStateFromVar(pObj->getProperty("state"), voiceManager, formatManager);
            }
        }
    }
}

} // namespace GlitchDSP
