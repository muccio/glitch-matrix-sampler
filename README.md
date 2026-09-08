# GlitchMatrixSampler

A production-ready, cross-platform VST3/AU instrument plugin and standalone synthesizer designed for glitch music, clicks & cuts, micro-sampling, and IDM. Built with **JUCE 8 (C++20)**, modern **CMake**, and an embedded modern Web UI (**Vite + React + Tailwind CSS**).

---

## ⚡ Overview & Architecture

GlitchMatrixSampler combines real-time DSP safety with an ultra-responsive cyberpunk Web UI. It allows musicians and sound designers to dynamically spawn, clone, layer, and manipulate an arbitrary number of sound sources at runtime.

### 🛡️ Audio Thread Safety (100% Lock-Free & Allocation-Free)
- **RCU Dynamic Source Graph**: Sound sources are added, cloned, modified, or removed on the Message/UI thread. The real-time audio thread reads an immutable atomic pointer (`activeGraph.load()`).
- **Zero Allocations in `processBlock`**: All dynamic memory allocations, graph reorganizations, and object destructions are deferred to the message thread via deferred reclamation (`collectGarbage()`).
- **Sibling-Aware Choking**: Choke groups (1–8) instantly mute older voices while permitting multi-layer simultaneous triggers without mutual cutting.

---

## 🎛️ Sound Sources & DSP Engine

GlitchMatrixSampler features 4 polymorphic sound source types:

1. **⚡ Click Generator (`ClickSource`)**:
   - **Dirac Impulse**: Single-sample to multi-sample needles with zero-phase attack onset.
   - **Resonant Pop**: Second-order bandpass resonance modeled after vintage circuit pop / glitch clicks.
   - **Chirp**: High-speed exponential frequency sweeps for percussive transients.
   - **Bit-Flip**: Raw digital square pulses simulating register glitching.
   - Dedicated Polarity inversion, Pitch Tracking, Damping, Pulse-Width, and Frequency controls.

2. **🌊 Oscillator (`OscillatorSource`)**:
   - PolyBLEP anti-aliased Sine, Triangle, Saw, Square waveforms + Glitch Table.
   - Zero-crossing phase reset on note-on to eliminate unwanted attack DC clicks.

3. **📻 Noise Generator (`NoiseSource`)**:
   - White, Pink, Poisson Crackle (controllable density), and Bit-Flip Hash noise algorithms.

4. **📼 Micro-Sampler (`SampleSource`)**:
   - Audio file playback with micro-looping, slice points, reverse playback, and granular scrub.

### ⏱️ FastEnvelope (AHDSR)
- Sub-millisecond attacks (down to 0.01 ms) with immediate sample-0 onset energy.
- Musical power curves (exponential snappy to logarithmic punch).
- Hold phase, decay with zero-sustain one-shot preservation, and anti-click choke release.
- Double-click inline numeric editing in the UI.

### 💥 Glitch Multi-FX
- **Bitcrusher**: 1 to 16 bits quantization.
- **Rate Reducer**: Downsampling with sample-and-hold step emulation.
- **Stutter / Gater**: Tempo-synced / millisecond chopping with hard gating.

---

## 🖥️ UI / IPC Architecture

- **Embedded Web UI**: Built with React 18, Vite, TypeScript, and Tailwind CSS.
- **JUCE 8 Integration**: Powered by `juce::WebBrowserComponent` using the native `__juce__invoke` protocol.
- **Local Asset Serving**: Zero external network dependencies; all assets are served via a custom JUCE `ResourceProvider`.
- **Keyboard & Inspector**: Interactive audition keyboard with pitch-strip, detailed source card rack, and per-source parameter inspector.

---

## 🛠️ Building GlitchMatrixSampler

### Prerequisites
- macOS 12+ (Apple Silicon or Intel), Linux, or Windows 10/11
- C++20 compatible compiler (Clang, GCC, or MSVC)
- CMake 3.22 or higher
- Node.js 18+ and npm

### 1. Build the Frontend Web UI
```bash
cd Frontend
npm install
npm run build
cd ..
```

### 2. Configure and Build the Plugin & Standalone Targets
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j8
```

The compiled binaries will be located under `build/GlitchMatrixSampler_artefacts/`:
- **Standalone**: `build/GlitchMatrixSampler_artefacts/Standalone/GlitchMatrixSampler.app`
- **VST3**: `build/GlitchMatrixSampler_artefacts/VST3/GlitchMatrixSampler.vst3`
- **AU**: `build/GlitchMatrixSampler_artefacts/AU/GlitchMatrixSampler.component`

---

## 🧪 Running the DSP Unit Test Suite

The project includes a standalone headless unit test runner verifying real-time thread safety, envelope curve math, click generator models, choke group routing, and JSON preset serialization:

```bash
./build/GlitchMatrixSampler_Tests
```

---

## 📄 License
Proprietary / All rights reserved.
