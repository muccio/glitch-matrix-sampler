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
            else if (typeStr == "Click")
            {
                newSource = std::make_shared<ClickSource>(id, nameStr.toStdString());
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

    // Preset 5: Micro-Click Array
    {
        auto* p = new juce::DynamicObject();
        p->setProperty("name", "06. Micro-Click Array");
        p->setProperty("description", "Dirac needle impulse, resonant wood pop, and laser chirp clicks");

        auto* root = new juce::DynamicObject();
        root->setProperty("masterVolume", 0.95f);

        juce::Array<juce::var> sources;
        {
            auto click1 = std::make_shared<ClickSource>(1, "Dirac Needle");
            click1->setClickType(ClickType::Dirac);
            click1->setPulseWidthSamples(3);
            click1->setAttackMs(0.01f);
            click1->setDecayMs(10.0f);
            click1->setSustainLevel(0.0f);
            click1->setReleaseMs(5.0f);
            click1->setPan(-0.4f);
            click1->setChokeGroup(1);
            sources.add(click1->toVar());
        }
        {
            auto click2 = std::make_shared<ClickSource>(2, "Resonant Pop");
            click2->setClickType(ClickType::Resonant);
            click2->setClickFrequency(1800.0f);
            click2->setClickDamping(0.75f);
            click2->setAttackMs(0.02f);
            click2->setDecayMs(25.0f);
            click2->setSustainLevel(0.0f);
            click2->setReleaseMs(10.0f);
            click2->setPan(0.4f);
            click2->setChokeGroup(1);
            sources.add(click2->toVar());
        }
        {
            auto click3 = std::make_shared<ClickSource>(3, "Laser Chirp");
            click3->setClickType(ClickType::Chirp);
            click3->setClickFrequency(800.0f);
            click3->setClickDamping(0.5f);
            click3->setAttackMs(0.01f);
            click3->setDecayMs(40.0f);
            click3->setSustainLevel(0.0f);
            click3->setReleaseMs(15.0f);
            click3->getGlitchFX().setBitDepth(5.0f);
            click3->getGlitchFX().setBitcrushMix(0.6f);
            sources.add(click3->toVar());
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

void StateSerializer::generateRandomGlitchSet(VoiceManager& voiceManager, juce::AudioFormatManager* /*formatManager*/)
{
    voiceManager.clearAllSources();

    juce::Random rng;
    voiceManager.setMasterVolume(0.85f + rng.nextFloat() * 0.1f);

    // Archetypes: 0: Clicks & Cuts Kit, 1: Digital Entropy, 2: Stutter Machine, 3: Micro-Needle Array
    int archetype = rng.nextInt(4);

    const int numSources = 16;

    for (int i = 0; i < numSources; ++i)
    {
        int id = i + 1;
        // 16 notes starting from Middle C (C3 / MIDI 60) spaced by 1 whole tone (2 semitones) ascending: 60..90
        int assignedNote = 60 + (i * 2);
        float pan = -0.75f + (1.5f / static_cast<float>(numSources - 1)) * static_cast<float>(i) + (rng.nextFloat() - 0.5f) * 0.15f;
        pan = juce::jlimit(-1.0f, 1.0f, pan);
        int chokeGroup = (archetype == 0 || archetype == 3) ? (1 + (i % 2)) : ((i % 3 == 0) ? 1 : 0);

        std::shared_ptr<SoundSource> src = nullptr;

        int typeChoice;
        if (archetype == 0) // Clicks & Cuts Kit: predominantly clicks, layered with noise & glitch hits
        {
            typeChoice = (i % 4 == 0 || i % 4 == 1) ? 3 : ((i % 4 == 2) ? 1 : 0);
        }
        else if (archetype == 1) // Digital Entropy: diverse entropy mix of noise, clicks and morphing oscs
        {
            typeChoice = (i % 3 == 0) ? 1 : ((i % 3 == 1) ? 3 : 0);
        }
        else if (archetype == 2) // Stutter Machine: heavy rhythmic oscillators & fast transients
        {
            typeChoice = (i % 3 == 0) ? 0 : ((i % 3 == 1) ? 3 : 1);
        }
        else // Micro-Transient Needle Array: predominantly sharp micro-clicks with subtle textures
        {
            typeChoice = (i % 4 == 3) ? 1 : ((i % 8 == 7) ? 0 : 3);
        }

        // typeChoice: 0 = Oscillator, 1 = Noise, 3 = Click
        if (typeChoice == 3)
        {
            int clickModelIdx = rng.nextInt(4);
            ClickType clickType = static_cast<ClickType>(clickModelIdx);
            std::string name;
            switch (clickType)
            {
                case ClickType::Dirac:    name = "Dirac " + std::to_string(id); break;
                case ClickType::Resonant: name = "ResoPop " + std::to_string(id); break;
                case ClickType::Chirp:    name = "Chirp " + std::to_string(id); break;
                case ClickType::BitFlip:  name = "BitPulse " + std::to_string(id); break;
            }

            auto click = std::make_shared<ClickSource>(id, name);
            click->setClickType(clickType);
            click->setPulseWidthSamples(1 + rng.nextInt(4));
            click->setClickFrequency(400.0f + rng.nextFloat() * 3200.0f);
            click->setClickDamping(0.4f + rng.nextFloat() * 0.45f);
            click->setPolarity(rng.nextInt(2));
            click->setPitchTrack(rng.nextBool());

            click->setGain(0.7f + rng.nextFloat() * 0.15f);
            click->setPan(pan);
            click->setAssignedNote(assignedNote);
            click->setChokeGroup(chokeGroup);

            click->setAttackMs(0.01f + rng.nextFloat() * 0.5f);
            click->setDecayMs(10.0f + rng.nextFloat() * 50.0f);
            click->setSustainLevel(0.0f);
            click->setReleaseMs(5.0f + rng.nextFloat() * 20.0f);
            click->setCurveShape(-0.8f + rng.nextFloat() * 0.3f);

            if (rng.nextFloat() < 0.4f)
            {
                click->getGlitchFX().setBitDepth(static_cast<float>(3 + rng.nextInt(6)));
                click->getGlitchFX().setBitcrushMix(0.4f + rng.nextFloat() * 0.5f);
            }
            if (rng.nextFloat() < 0.3f)
            {
                click->getGlitchFX().setDownsampleHz(800.0f + rng.nextFloat() * 6000.0f);
                click->getGlitchFX().setDownsampleMix(0.5f + rng.nextFloat() * 0.4f);
            }
            src = click;
        }
        else if (typeChoice == 1)
        {
            int noiseModelIdx = rng.nextInt(4);
            NoiseType noiseType = static_cast<NoiseType>(noiseModelIdx);
            std::string name;
            switch (noiseType)
            {
                case NoiseType::White:       name = "WhiteBurst " + std::to_string(id); break;
                case NoiseType::Pink:        name = "PinkCrackle " + std::to_string(id); break;
                case NoiseType::Crackle:     name = "Poisson " + std::to_string(id); break;
                case NoiseType::BitFlipHash: name = "HashNoise " + std::to_string(id); break;
            }

            auto noise = std::make_shared<NoiseSource>(id, name);
            noise->setNoiseType(noiseType);
            noise->setCrackleDensity(200.0f + rng.nextFloat() * 2400.0f);
            noise->setHashRate(800.0f + rng.nextFloat() * 8000.0f);

            noise->setGain(0.65f + rng.nextFloat() * 0.15f);
            noise->setPan(pan);
            noise->setAssignedNote(assignedNote);
            noise->setChokeGroup(chokeGroup);

            noise->setAttackMs(0.05f + rng.nextFloat() * 1.5f);
            noise->setDecayMs(20.0f + rng.nextFloat() * 160.0f);
            noise->setSustainLevel(0.0f);
            noise->setReleaseMs(10.0f + rng.nextFloat() * 40.0f);
            noise->setCurveShape(-0.7f + rng.nextFloat() * 0.3f);

            if (rng.nextFloat() < 0.5f)
            {
                noise->getGlitchFX().setDownsampleHz(600.0f + rng.nextFloat() * 4000.0f);
                noise->getGlitchFX().setDownsampleMix(0.6f + rng.nextFloat() * 0.35f);
            }
            if (rng.nextFloat() < 0.35f)
            {
                noise->getGlitchFX().setBitDepth(static_cast<float>(2 + rng.nextInt(6)));
                noise->getGlitchFX().setBitcrushMix(0.5f + rng.nextFloat() * 0.45f);
            }
            src = noise;
        }
        else
        {
            int waveIdx = rng.nextInt(5);
            OscWaveform waveform = static_cast<OscWaveform>(waveIdx);
            std::string name;
            switch (waveform)
            {
                case OscWaveform::Sine:            name = "GlitchSine " + std::to_string(id); break;
                case OscWaveform::Square:          name = "SquareHit " + std::to_string(id); break;
                case OscWaveform::Saw:             name = "SawTooth " + std::to_string(id); break;
                case OscWaveform::Triangle:        name = "SubTriangle " + std::to_string(id); break;
                case OscWaveform::GlitchWavetable: name = "FoldWave " + std::to_string(id); break;
            }

            auto osc = std::make_shared<OscillatorSource>(id, name);
            osc->setWaveform(waveform);
            osc->setPulseWidth(0.2f + rng.nextFloat() * 0.6f);
            osc->setGlitchMorph(0.1f + rng.nextFloat() * 0.8f);

            const int semis[] = { -24, -12, 0, 7, 12, 19, 24 };
            osc->setPitchSemi(static_cast<float>(semis[rng.nextInt(7)]));
            osc->setPitchFine((rng.nextFloat() - 0.5f) * 20.0f);

            osc->setGain(0.7f + rng.nextFloat() * 0.15f);
            osc->setPan(pan);
            osc->setAssignedNote(assignedNote);
            osc->setChokeGroup(chokeGroup);

            osc->setAttackMs(0.1f + rng.nextFloat() * 2.0f);
            osc->setDecayMs(35.0f + rng.nextFloat() * 300.0f);
            osc->setSustainLevel(rng.nextFloat() < 0.25f ? 0.3f : 0.0f);
            osc->setReleaseMs(15.0f + rng.nextFloat() * 60.0f);
            osc->setCurveShape(-0.6f + rng.nextFloat() * 0.3f);

            if (rng.nextFloat() < 0.6f)
            {
                osc->getGlitchFX().setStutterHz(6.0f + rng.nextFloat() * 32.0f);
                osc->getGlitchFX().setStutterDuty(0.35f + rng.nextFloat() * 0.35f);
                osc->getGlitchFX().setStutterMix(0.5f + rng.nextFloat() * 0.45f);
                const int divs[] = { 1, 2, 4, 8 };
                osc->getGlitchFX().setStutterDivision(divs[rng.nextInt(4)]);
            }
            if (rng.nextFloat() < 0.45f)
            {
                osc->getGlitchFX().setBitDepth(static_cast<float>(3 + rng.nextInt(6)));
                osc->getGlitchFX().setBitcrushMix(0.4f + rng.nextFloat() * 0.5f);
            }
            if (rng.nextFloat() < 0.4f)
            {
                osc->getGlitchFX().setDownsampleHz(1000.0f + rng.nextFloat() * 8000.0f);
                osc->getGlitchFX().setDownsampleMix(0.4f + rng.nextFloat() * 0.5f);
            }
            src = osc;
        }

        if (src)
        {
            voiceManager.addSource(src);
        }
    }
}

} // namespace GlitchDSP
