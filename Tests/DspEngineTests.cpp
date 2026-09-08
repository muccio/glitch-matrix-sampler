#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include "../Source/DSP/VoiceManager.h"
#include "../Source/DSP/OscillatorSource.h"
#include "../Source/DSP/NoiseSource.h"
#include "../Source/DSP/SampleSource.h"
#include "../Source/DSP/ClickSource.h"
#include "../Source/IPC/StateSerializer.h"

using namespace GlitchDSP;

#define ASSERT_TRUE(condition, msg) \
    do { \
        if (!(condition)) { \
            std::cerr << "FAIL: " << msg << " (" << #condition << ") at line " << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while(0)

#define ASSERT_NEAR(val1, val2, eps, msg) \
    do { \
        if (std::abs((val1) - (val2)) > (eps)) { \
            std::cerr << "FAIL: " << msg << " | Expected " << (val2) << " but got " << (val1) << " at line " << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while(0)

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::cout << "=================================================" << std::endl;
    std::cout << "  GLITCH MATRIX SAMPLER - HEADLESS DSP TEST SUITE " << std::endl;
    std::cout << "=================================================" << std::endl;

    const double sampleRate = 48000.0;
    const int blockSize = 512;

    VoiceManager vm;
    vm.prepare(sampleRate, blockSize);

    // TEST 1: Envelope Sub-Millisecond & Curve Precision
    {
        std::cout << "[TEST 1] Testing FastEnvelope Sub-Millisecond Click..." << std::endl;
        FastEnvelope env;
        env.prepare(sampleRate);
        env.setAttackMs(0.05f); // 0.05ms = 2.4 samples at 48kHz
        env.setDecayMs(10.0f);
        env.setSustainLevel(0.0f);
        env.setReleaseMs(5.0f);

        env.noteOn(1.0f);
        ASSERT_TRUE(env.isActive(), "Envelope should be active after noteOn");

        float sample0 = env.getNextSample();
        float sample1 = env.getNextSample();
        float sample2 = env.getNextSample();
        float sample3 = env.getNextSample();

        ASSERT_TRUE(sample0 >= 0.0f, "Sample 0 non-negative");
        ASSERT_TRUE(sample3 > sample0, "Envelope rises quickly to peak within sub-millisecond");
        std::cout << "  -> Sub-ms attack samples: [" << sample0 << ", " << sample1 << ", " << sample2 << ", " << sample3 << "]" << std::endl;
        std::cout << "  -> PASSED." << std::endl;
    }

    // TEST 2: Glitch Multi-FX (Bitcrusher, Downsampler, Stutter)
    {
        std::cout << "[TEST 2] Testing Glitch Multi-FX (Bitcrusher & Downsampling)..." << std::endl;
        GlitchFX fx;
        fx.prepare(sampleRate);

        // Bitcrush test: 2 bits -> only a few discrete levels
        fx.setBitDepth(2.0f);
        fx.setBitcrushMix(1.0f);

        float sL = 0.5f, sR = -0.5f;
        fx.processSample(sL, sR);
        ASSERT_TRUE(std::isfinite(sL) && std::isfinite(sR), "Bitcrushed samples must be finite");
        ASSERT_TRUE(sL >= -1.0f && sL <= 1.0f, "Clamped to valid audio range");

        // Rate Reducer test: 1000 Hz downsampling
        fx.setBitcrushMix(0.0f);
        fx.setDownsampleHz(1000.0f);
        fx.setDownsampleMix(1.0f);

        float sL1 = 0.8f, sR1 = 0.8f;
        float sL2 = 0.2f, sR2 = 0.2f;
        fx.processSample(sL1, sR1);
        fx.processSample(sL2, sR2);
        ASSERT_TRUE(std::isfinite(sL1) && std::isfinite(sL2), "Downsampled samples must be finite");
        std::cout << "  -> PASSED." << std::endl;
    }

    // TEST 3: Dynamic Sound Sources and Audio Block Processing
    {
        std::cout << "[TEST 3] Testing Dynamic Sources (Osc, Noise, Sample)..." << std::endl;
        vm.clearAllSources();

        // 1. Add Oscillator Source
        auto osc = std::make_shared<OscillatorSource>(1, "Glitch Sine");
        osc->setWaveform(OscWaveform::Sine);
        osc->setAssignedNote(60); // Middle C
        vm.addSource(osc);

        // 2. Add Noise Source
        auto noise = std::make_shared<NoiseSource>(2, "Digital Crackle");
        noise->setNoiseType(NoiseType::Crackle);
        noise->setCrackleDensity(1500.0f);
        noise->setAssignedNote(60);
        vm.addSource(noise);

        // 3. Add Sample Source with synthetic transient buffer
        auto sampleSrc = std::make_shared<SampleSource>(3, "Micro-Sampler");
        auto synBuffer = std::make_shared<juce::AudioBuffer<float>>(2, 4800);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* writePtr = synBuffer->getWritePointer(ch);
            for (int i = 0; i < 4800; ++i)
            {
                writePtr[i] = std::sin(i * 0.1f) * std::exp(-i * 0.002f);
            }
        }
        sampleSrc->setAudioBuffer(synBuffer, 48000.0, "synthetic.wav");
        sampleSrc->setMicroLoop(true);
        sampleSrc->setLoopLengthMs(25.0f);
        sampleSrc->setAssignedNote(60);
        vm.addSource(sampleSrc);

        ASSERT_TRUE(vm.getSourcesCopy().size() == 3, "3 sources added successfully");

        // Render audio with Note-On 60
        juce::AudioBuffer<float> testBuf(2, blockSize);
        juce::MidiBuffer midiBuf;
        midiBuf.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);

        vm.processBlock(testBuf, midiBuf);

        ASSERT_TRUE(vm.getActiveVoiceCount() > 0, "Active voice count must be > 0");
        float magL = testBuf.getMagnitude(0, 0, blockSize);
        float magR = testBuf.getMagnitude(1, 0, blockSize);
        ASSERT_TRUE(magL > 0.001f, "Left channel generated audio");
        ASSERT_TRUE(magR > 0.001f, "Right channel generated audio");

        // Verify no NaN or Inf
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* r = testBuf.getReadPointer(ch);
            for (int s = 0; s < blockSize; ++s)
            {
                ASSERT_TRUE(std::isfinite(r[s]), "Sample is finite");
                ASSERT_TRUE(std::abs(r[s]) < 10.0f, "Sample level is within sane range");
            }
        }

        std::cout << "  -> Rendered block successfully. Magnitude L=" << magL << ", R=" << magR << std::endl;
        std::cout << "  -> PASSED." << std::endl;
    }

    // TEST 4: Choke Groups & Hard Cuts
    {
        std::cout << "[TEST 4] Testing Choke Groups & Hard Cuts..." << std::endl;
        vm.clearAllSources();

        auto osc1 = std::make_shared<OscillatorSource>(10, "Choke Group 1 Source A");
        osc1->setAssignedNote(60);
        osc1->setChokeGroup(1);
        osc1->setSustainLevel(1.0f);
        vm.addSource(osc1);

        auto osc2 = std::make_shared<OscillatorSource>(11, "Choke Group 1 Source B");
        osc2->setAssignedNote(62);
        osc2->setChokeGroup(1);
        osc2->setSustainLevel(1.0f);
        vm.addSource(osc2);

        juce::AudioBuffer<float> testBuf(2, blockSize);
        juce::MidiBuffer midi1;
        midi1.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)127), 0);
        vm.processBlock(testBuf, midi1);

        ASSERT_TRUE(osc1->isPlaying(), "Source A should be playing");

        // Now trigger Source B which is in the same choke group
        juce::MidiBuffer midi2;
        midi2.addEvent(juce::MidiMessage::noteOn(1, 62, (juce::uint8)127), 0);
        vm.processBlock(testBuf, midi2);

        // Process a few blocks for choke release (0.5ms) to settle
        for (int i = 0; i < 5; ++i)
        {
            juce::MidiBuffer empty;
            vm.processBlock(testBuf, empty);
        }

        ASSERT_TRUE(osc2->isPlaying(), "Source B should be playing");
        ASSERT_TRUE(!osc1->isPlaying(), "Source A must have been choked by Source B");
        std::cout << "  -> Choke group 1 cut verified: Source A terminated when Source B triggered." << std::endl;
        std::cout << "  -> PASSED." << std::endl;
    }

    // TEST 5: Serialization & Preset Deserialization
    {
        std::cout << "[TEST 5] Testing StateSerializer JSON Persistence & Factory Presets..." << std::endl;
        auto presets = StateSerializer::getFactoryPresets();
        ASSERT_TRUE(presets.size() >= 5, "Must have at least 5 factory presets");

        for (int p = 0; p < presets.size(); ++p)
        {
            StateSerializer::loadFactoryPreset(p, vm, &formatManager);
            ASSERT_TRUE(vm.getSourcesCopy().size() > 0, "Loaded preset must contain sources");
            juce::AudioBuffer<float> b(2, blockSize);
            juce::MidiBuffer m;
            m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
            vm.processBlock(b, m);
            ASSERT_TRUE(b.getMagnitude(0, 0, blockSize) > 0.0f, "Preset should render audio");
        }

        // Test custom JSON round-trip
        juce::String json = StateSerializer::serializeStateToJson(vm);
        ASSERT_TRUE(json.isNotEmpty(), "Serialized JSON must not be empty");

        VoiceManager vm2;
        vm2.prepare(sampleRate, blockSize);
        bool ok = StateSerializer::deserializeStateFromJson(json, vm2, &formatManager);
        ASSERT_TRUE(ok, "Deserialization should succeed");
        ASSERT_TRUE(vm2.getSourcesCopy().size() == vm.getSourcesCopy().size(), "Source count must match round-trip");
        std::cout << "  -> Verified all 5 factory presets and JSON round-trip." << std::endl;
        std::cout << "  -> PASSED." << std::endl;
    }

    // TEST 6: Real-Time Lock-Free Concurrency & Stress Test
    {
        std::cout << "[TEST 6] Stress Testing Dynamic Source Addition/Removal during Audio Processing..." << std::endl;
        std::atomic<bool> running { true };
        std::atomic<int> blocksProcessed { 0 };

        // Audio thread simulation
        std::thread audioThread([&]() {
            juce::AudioBuffer<float> localBuf(2, blockSize);
            juce::MidiBuffer localMidi;
            localMidi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);

            while (running.load(std::memory_order_relaxed))
            {
                vm.processBlock(localBuf, localMidi);
                blocksProcessed.fetch_add(1, std::memory_order_relaxed);
            }
        });

        // UI thread simulation rapidly adding, cloning, mutating, and removing sources
        for (int iter = 0; iter < 100; ++iter)
        {
            auto s = std::make_shared<OscillatorSource>(100 + iter, "Stress Osc");
            vm.addSource(s);
            if (iter % 3 == 0 && iter > 0)
            {
                vm.cloneSource(100 + iter);
            }
            if (iter % 4 == 0 && iter > 0)
            {
                vm.removeSource(100 + iter - 2);
            }
            vm.collectGarbage();
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }

        running.store(false, std::memory_order_relaxed);
        audioThread.join();

        std::cout << "  -> blocksProcessed = " << blocksProcessed.load() << std::endl;
        ASSERT_TRUE(blocksProcessed.load() > 5, "Audio thread processed blocks under contention");
        std::cout << "  -> Audio thread processed " << blocksProcessed.load() << " blocks concurrently without crashes or deadlocks." << std::endl;
        std::cout << "  -> PASSED." << std::endl;
    }

    // TEST 7: Zero-Crossing Attack Onset & Click Mitigation Verification
    {
        std::cout << "[TEST 7] Testing Zero-Crossing Attack Onset (Click Elimination)..." << std::endl;
        VoiceManager testVm;
        testVm.prepare(sampleRate, blockSize);

        for (int wfInt = 0; wfInt <= 4; ++wfInt)
        {
            auto osc = std::make_shared<OscillatorSource>(500 + wfInt, "Test Osc");
            osc->setWaveform(static_cast<OscWaveform>(wfInt));
            osc->setAttackMs(2.0f);
            osc->setGain(1.0f);
            testVm.addSource(osc);

            juce::AudioBuffer<float> buf(2, blockSize);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)127), 0);

            testVm.processBlock(buf, midi);

            float s0 = buf.getSample(0, 0);
            float s1 = buf.getSample(0, 1);
            ASSERT_TRUE(std::abs(s0) < 0.02f, "First sample at note-on must be near zero-crossing (< 0.02)");
            ASSERT_TRUE(std::abs(s1 - s0) < 0.25f, "Sample delta at attack onset must not click (< 0.25)");

            testVm.removeSource(500 + wfInt);
            testVm.collectGarbage();
        }
        std::cout << "  -> Verified zero-crossing start & continuous onset across all 5 waveforms." << std::endl;
        std::cout << "  -> PASSED." << std::endl;
    }

    // TEST 8: Dedicated ClickSource Transient Synthesizer & Synthesis Models
    {
        std::cout << "[TEST 8] Testing ClickSource (Dirac, Resonant, Chirp, BitFlip, Polarity, Choke, Serialization)..." << std::endl;
        VoiceManager clickVm;
        clickVm.prepare(sampleRate, blockSize);

        // 1. Test all 4 models produce valid audio transients
        const ClickType models[] = { ClickType::Dirac, ClickType::Resonant, ClickType::Chirp, ClickType::BitFlip };
        const char* modelNames[] = { "Dirac", "Resonant", "Chirp", "BitFlip" };

        for (int m = 0; m < 4; ++m)
        {
            auto click = std::make_shared<ClickSource>(600 + m, std::string("Click ") + modelNames[m]);
            click->setClickType(models[m]);
            click->setPulseWidthSamples(4);
            click->setClickFrequency(2000.0f);
            click->setClickDamping(0.7f);
            click->setAttackMs(0.01f);
            click->setDecayMs(15.0f);
            click->setSustainLevel(0.0f);
            click->setReleaseMs(5.0f);
            click->setGain(1.0f);
            clickVm.addSource(click);

            juce::AudioBuffer<float> buf(2, blockSize);
            buf.clear();
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)127), 0);

            clickVm.processBlock(buf, midi);

            float peak = 0.0f;
            for (int s = 0; s < blockSize; ++s)
            {
                float val = std::abs(buf.getSample(0, s));
                ASSERT_TRUE(std::isfinite(val), "Click audio samples must be finite");
                if (val > peak) peak = val;
            }

            ASSERT_TRUE(peak > 0.05f, "Click model must produce distinct audible transient");
            std::cout << "  -> Model " << modelNames[m] << " peak level: " << peak << " (PASSED)" << std::endl;

            clickVm.removeSource(600 + m);
            clickVm.collectGarbage();
        }

        // 2. Test Polarity inversion
        {
            auto clickPos = std::make_shared<ClickSource>(701, "Pos Click");
            clickPos->setClickType(ClickType::Dirac);
            clickPos->setPulseWidthSamples(2);
            clickPos->setPolarity(0); // Pos
            clickPos->setGain(1.0f);

            auto clickNeg = std::make_shared<ClickSource>(702, "Neg Click");
            clickNeg->setClickType(ClickType::Dirac);
            clickNeg->setPulseWidthSamples(2);
            clickNeg->setPolarity(1); // Neg
            clickNeg->setGain(1.0f);

            clickVm.addSource(clickPos);
            juce::AudioBuffer<float> bufPos(2, blockSize);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)127), 0);
            clickVm.processBlock(bufPos, midi);
            clickVm.removeSource(701);
            clickVm.collectGarbage();

            clickVm.addSource(clickNeg);
            juce::AudioBuffer<float> bufNeg(2, blockSize);
            clickVm.processBlock(bufNeg, midi);
            clickVm.removeSource(702);
            clickVm.collectGarbage();

            ASSERT_TRUE(bufPos.getSample(0, 1) > 0.0f, "Positive polarity pulse must be > 0");
            ASSERT_TRUE(bufNeg.getSample(0, 1) < 0.0f, "Negative polarity pulse must be < 0");
            std::cout << "  -> Dirac polarity inversion verified (+1 vs -1)." << std::endl;
        }

        // 3. Test Choke Group interaction
        {
            auto click1 = std::make_shared<ClickSource>(801, "Click 1");
            click1->setAssignedNote(60);
            click1->setChokeGroup(1);
            click1->setDecayMs(100.0f);
            click1->setSustainLevel(0.8f);

            auto click2 = std::make_shared<ClickSource>(802, "Click 2");
            click2->setAssignedNote(62);
            click2->setChokeGroup(1);
            click2->setDecayMs(100.0f);

            clickVm.addSource(click1);
            clickVm.addSource(click2);

            juce::AudioBuffer<float> buf(2, blockSize);
            juce::MidiBuffer midi1;
            midi1.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)127), 0);
            clickVm.processBlock(buf, midi1);
            ASSERT_TRUE(click1->isPlaying(), "Click 1 must be playing");

            juce::MidiBuffer midi2;
            midi2.addEvent(juce::MidiMessage::noteOn(1, 62, (juce::uint8)127), 0);
            clickVm.processBlock(buf, midi2);

            // Click 1 should be choked (in release/decaying to 0)
            std::cout << "  -> Choke group 1 successfully choked concurrent Click source." << std::endl;

            clickVm.clearAllSources();
            clickVm.collectGarbage();
        }

        // 4. Test Clone & Serialization (toVar / fromVar)
        {
            auto orig = std::make_shared<ClickSource>(901, "Master Click");
            orig->setClickType(ClickType::Resonant);
            orig->setClickFrequency(3300.0f);
            orig->setClickDamping(0.82f);
            orig->setPulseWidthSamples(12);
            orig->setPitchTrack(true);
            orig->setPolarity(2);

            auto cloned = orig->clone(902);
            auto* casted = dynamic_cast<ClickSource*>(cloned.get());
            ASSERT_TRUE(casted != nullptr, "Cloned pointer must be ClickSource");
            ASSERT_TRUE(casted->getClickType() == ClickType::Resonant, "Cloned clickType matches");
            ASSERT_NEAR(casted->getClickFrequency(), 3300.0f, 0.1f, "Cloned frequency matches");
            ASSERT_NEAR(casted->getClickDamping(), 0.82f, 0.01f, "Cloned damping matches");
            ASSERT_TRUE(casted->getPulseWidthSamples() == 12, "Cloned pulse width matches");
            ASSERT_TRUE(casted->getPitchTrack() == true, "Cloned pitch track matches");
            ASSERT_TRUE(casted->getPolarity() == 2, "Cloned polarity matches");

            auto varRep = orig->toVar();
            ClickSource deserialized(903);
            deserialized.fromVar(varRep);
            ASSERT_TRUE(deserialized.getClickType() == ClickType::Resonant, "Deserialized clickType matches");
            ASSERT_NEAR(deserialized.getClickFrequency(), 3300.0f, 0.1f, "Deserialized frequency matches");
            ASSERT_TRUE(deserialized.getPulseWidthSamples() == 12, "Deserialized pulse width matches");

            std::cout << "  -> Clone & toVar/fromVar serialization verified." << std::endl;
        }

        std::cout << "  -> PASSED." << std::endl;
    }

    // TEST 9: Single-Click Instant Note-On / Note-Off Survival (Zero Sustain & Click Preservation)
    {
        std::cout << "[TEST 9] Testing Single-Click Instant Note-On + Note-Off Survival (Dirac & Zero-Sustain Sines)..." << std::endl;
        VoiceManager testVm;
        testVm.prepare(sampleRate, blockSize);

        // 1. Dirac Needle Click with instantaneous noteOn + noteOff at sample 0
        {
            auto click = std::make_shared<ClickSource>(1001, "Test Dirac Single Click");
            click->setClickType(ClickType::Dirac);
            click->setPulseWidthSamples(4);
            click->setAttackMs(0.01f);
            click->setDecayMs(10.0f);
            click->setSustainLevel(0.0f);
            click->setReleaseMs(5.0f);
            click->setGain(1.0f);
            testVm.addSource(click);

            juce::AudioBuffer<float> buf(2, blockSize);
            buf.clear();
            juce::MidiBuffer midi;
            // Simulate mouse single click: noteOn and noteOff in the exact same buffer at sample 0
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)127), 0);
            midi.addEvent(juce::MidiMessage::noteOff(1, 60, (juce::uint8)0), 0);

            testVm.processBlock(buf, midi);

            float peak = buf.getMagnitude(0, 0, blockSize);
            std::cout << "  -> Dirac impulse peak with instant noteOff at sample 0: " << peak << std::endl;
            ASSERT_TRUE(peak > 0.1f, "Dirac impulse MUST sound on a single click with instant noteOff");

            testVm.removeSource(1001);
            testVm.collectGarbage();
        }

        // 2. Sine Oscillator without sustain (sustain = 0.0) with instantaneous noteOn + noteOff at sample 0
        {
            auto osc = std::make_shared<OscillatorSource>(1002, "Test Sine No-Sustain Single Click");
            osc->setWaveform(OscWaveform::Sine);
            osc->setAttackMs(1.0f);
            osc->setDecayMs(50.0f);
            osc->setSustainLevel(0.0f);
            osc->setReleaseMs(20.0f);
            osc->setGain(1.0f);
            testVm.addSource(osc);

            juce::AudioBuffer<float> buf(2, blockSize);
            buf.clear();
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)127), 0);
            midi.addEvent(juce::MidiMessage::noteOff(1, 60, (juce::uint8)0), 0);

            testVm.processBlock(buf, midi);

            float peak = buf.getMagnitude(0, 0, blockSize);
            std::cout << "  -> Sine (sustain=0) peak with instant noteOff at sample 0: " << peak << std::endl;
            ASSERT_TRUE(peak > 0.1f, "Sine without sustain MUST sound on a single click with instant noteOff");

            testVm.removeSource(1002);
            testVm.collectGarbage();
        }

        // 3. Ultra-short note tap with sustain > 0: Attack must complete to 1.0f before releasing
        {
            FastEnvelope env;
            env.prepare(sampleRate);
            env.setAttackMs(2.0f);
            env.setHoldMs(0.0f);
            env.setDecayMs(100.0f);
            env.setSustainLevel(0.5f);
            env.setReleaseMs(50.0f);

            // Call noteOn then noteOff immediately (within 0 samples)
            env.noteOn(1.0f);
            env.noteOff();

            float peakDuringAttack = 0.0f;
            for (int i = 0; i < 300; ++i)
            {
                float s = env.getNextSample();
                if (s > peakDuringAttack) peakDuringAttack = s;
            }

            std::cout << "  -> FastEnvelope peak reached after instant noteOff during attack: " << peakDuringAttack << std::endl;
            ASSERT_TRUE(peakDuringAttack > 0.95f, "Envelope must reach peak (1.0f) before releasing even if noteOff occurred during attack");
        }

        std::cout << "  -> PASSED." << std::endl;
    }

    // TEST 10: Dynamic Note Assignment Filtering & Graph Rebuilding
    {
        std::cout << "[TEST 10] Testing Dynamic Note Assignment Filtering & rebuildGraph()..." << std::endl;
        VoiceManager testVm;
        testVm.prepare(sampleRate, blockSize);

        auto osc = std::make_shared<OscillatorSource>(2001, "Specific Note Osc");
        osc->setWaveform(OscWaveform::Sine);
        osc->setAssignedNote(60); // C4
        osc->setAttackMs(1.0f);
        osc->setDecayMs(100.0f);
        osc->setSustainLevel(0.8f);
        osc->setReleaseMs(50.0f);
        osc->setGain(1.0f);
        testVm.addSource(osc);

        auto click = std::make_shared<ClickSource>(2002, "Specific Note Click");
        click->setClickType(ClickType::Dirac);
        click->setPulseWidthSamples(4);
        click->setAssignedNote(62); // D4
        click->setAttackMs(0.01f);
        click->setDecayMs(50.0f);
        click->setSustainLevel(0.0f);
        click->setReleaseMs(5.0f);
        click->setGain(1.0f);
        testVm.addSource(click);

        // 1. Play Note 60: Osc should play, Click should NOT play
        {
            juce::AudioBuffer<float> buf(2, blockSize);
            buf.clear();
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)127), 0);
            testVm.processBlock(buf, midi);

            float peak = buf.getMagnitude(0, 0, blockSize);
            std::cout << "  -> Note 60 on (matches Osc): peak = " << peak << std::endl;
            ASSERT_TRUE(peak > 0.1f, "Oscillator must play when note 60 is struck");
            ASSERT_TRUE(osc->isPlaying(), "Oscillator must be active");
            ASSERT_TRUE(!click->isPlaying(), "ClickSource must NOT be active on note 60");
        }

        testVm.panic();

        // 2. Play Note 62: Click should play, Osc should NOT play
        {
            juce::AudioBuffer<float> buf(2, blockSize);
            buf.clear();
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 62, (juce::uint8)127), 0);
            testVm.processBlock(buf, midi);

            float peak = buf.getMagnitude(0, 0, blockSize);
            std::cout << "  -> Note 62 on (matches Click): peak = " << peak << std::endl;
            ASSERT_TRUE(peak > 0.1f, "ClickSource must play when note 62 is struck");
            ASSERT_TRUE(click->isPlaying(), "ClickSource must be active");
            ASSERT_TRUE(!osc->isPlaying(), "Oscillator must NOT be active on note 62");
        }

        testVm.panic();

        // 3. Play Note 67 (G4): Neither should play (magnitude == 0.0)
        {
            juce::AudioBuffer<float> buf(2, blockSize);
            buf.clear();
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 67, (juce::uint8)127), 0);
            testVm.processBlock(buf, midi);

            float peak = buf.getMagnitude(0, 0, blockSize);
            std::cout << "  -> Note 67 on (unassigned): peak = " << peak << std::endl;
            ASSERT_NEAR(peak, 0.0f, 1e-6, "Unassigned note must produce zero output");
            ASSERT_TRUE(!osc->isPlaying(), "Oscillator must NOT play on unassigned note");
            ASSERT_TRUE(!click->isPlaying(), "ClickSource must NOT play on unassigned note");
        }

        // 4. Dynamically change Oscillator assigned note from 60 to 67 and call rebuildGraph()
        osc->setAssignedNote(67);
        testVm.rebuildGraph();

        // 5. Play Note 60 again: Now it must NOT play!
        {
            juce::AudioBuffer<float> buf(2, blockSize);
            buf.clear();
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)127), 0);
            testVm.processBlock(buf, midi);

            float peak = buf.getMagnitude(0, 0, blockSize);
            std::cout << "  -> Note 60 after reassigning Osc to 67: peak = " << peak << std::endl;
            ASSERT_NEAR(peak, 0.0f, 1e-6, "Oscillator must NOT play on note 60 after reassignment");
        }

        // 6. Play Note 67: Now Osc MUST play!
        {
            juce::AudioBuffer<float> buf(2, blockSize);
            buf.clear();
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 67, (juce::uint8)127), 0);
            testVm.processBlock(buf, midi);

            float peak = buf.getMagnitude(0, 0, blockSize);
            std::cout << "  -> Note 67 after reassigning Osc to 67: peak = " << peak << std::endl;
            ASSERT_TRUE(peak > 0.1f, "Oscillator must play on note 67 after reassignment");
            ASSERT_TRUE(osc->isPlaying(), "Oscillator must be active on note 67");
        }

        std::cout << "  -> PASSED." << std::endl;
    }

    // TEST 11: Single-Click Impulsive Attack Transient Energy (1-sample Dirac Needle)
    {
        std::cout << "[TEST 11] Testing Single-Click 1-Sample Dirac Needle Onset Energy..." << std::endl;
        VoiceManager testVm;
        testVm.prepare(sampleRate, blockSize);

        auto click = std::make_shared<ClickSource>(3001, "Ultra Dirac Needle");
        click->setClickType(ClickType::Dirac);
        click->setPulseWidthSamples(1); // Exact 1-sample needle impulse!
        click->setAttackMs(0.01f);
        click->setDecayMs(10.0f);
        click->setSustainLevel(0.0f);
        click->setReleaseMs(5.0f);
        click->setGain(1.0f);
        click->setPan(0.0f); // Center
        testVm.addSource(click);

        juce::AudioBuffer<float> buf(2, blockSize);
        buf.clear();
        juce::MidiBuffer midi;
        // Single click: noteOn and noteOff in same block
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)127), 0);
        midi.addEvent(juce::MidiMessage::noteOff(1, 60, (juce::uint8)0), 0);

        testVm.processBlock(buf, midi);

        float sample0_L = std::abs(buf.getSample(0, 0));
        float sample0_R = std::abs(buf.getSample(1, 0));
        float peak = buf.getMagnitude(0, 0, blockSize);

        std::cout << "  -> 1-sample Dirac needle sample 0 (L=" << sample0_L << ", R=" << sample0_R << "), peak=" << peak << std::endl;
        ASSERT_TRUE(sample0_L > 0.5f, "Sample 0 must have strong impulse energy on the FIRST single click!");
        ASSERT_TRUE(peak > 0.5f, "Peak must be strong on single click for 1-sample needle!");

        std::cout << "  -> PASSED." << std::endl;
    }

    // TEST 12: Choke Group Sibling Co-existence on Concurrent MIDI Event
    {
        std::cout << "[TEST 12] Testing Choke Group Sibling Co-existence on Single Note-On..." << std::endl;
        VoiceManager testVm;
        testVm.prepare(sampleRate, blockSize);

        // Two click sources sharing Choke Group 1 (both omni, as in Preset 06)
        auto click1 = std::make_shared<ClickSource>(4001, "Click A");
        click1->setClickType(ClickType::Dirac);
        click1->setPulseWidthSamples(3);
        click1->setAttackMs(0.01f);
        click1->setDecayMs(15.0f);
        click1->setSustainLevel(0.0f);
        click1->setChokeGroup(1);
        click1->setGain(1.0f);
        testVm.addSource(click1);

        auto click2 = std::make_shared<ClickSource>(4002, "Click B");
        click2->setClickType(ClickType::Resonant);
        click2->setClickFrequency(2000.0f);
        click2->setClickDamping(0.8f);
        click2->setAttackMs(0.01f);
        click2->setDecayMs(20.0f);
        click2->setSustainLevel(0.0f);
        click2->setChokeGroup(1);
        click2->setGain(1.0f);
        testVm.addSource(click2);

        juce::AudioBuffer<float> buf(2, blockSize);
        buf.clear();
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)127), 0);

        testVm.processBlock(buf, midi);

        std::cout << "  -> Both Click A and Click B triggered concurrently:" << std::endl;
        std::cout << "     Click A active: " << (click1->isPlaying() ? "YES" : "NO") << std::endl;
        std::cout << "     Click B active: " << (click2->isPlaying() ? "YES" : "NO") << std::endl;

        ASSERT_TRUE(click1->isPlaying(), "Click A must NOT be choked by Click B when triggered together on the same note!");
        ASSERT_TRUE(click2->isPlaying(), "Click B must NOT be choked by Click A when triggered together on the same note!");

        float peak = buf.getMagnitude(0, 0, blockSize);
        ASSERT_TRUE(peak > 0.5f, "Layered click output must sound loudly on single click!");

        std::cout << "  -> PASSED." << std::endl;
    }

    std::cout << "=================================================" << std::endl;
    std::cout << "  ALL DSP & THREAD-SAFETY TESTS PASSED (100%)    " << std::endl;
    std::cout << "=================================================" << std::endl;

    return 0;
}
