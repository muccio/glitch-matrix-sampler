import { GlobalState, SoundSourceData, VoiceStats, WaveformPeak, SourceType } from '../types/matrix';

declare global {
  interface Window {
    __JUCE__?: {
      backend?: {
        emitEvent?: (eventId: string, payload: any) => void;
        addEventListener?: (eventId: string, callback: (...args: any[]) => void) => [string, number] | void;
        removeEventListener?: (token: any) => void;
        [key: string]: any;
      };
      initialisationData?: Record<string, any>;
    };
  }
}

type StateListener = (state: GlobalState) => void;
type StatsListener = (stats: VoiceStats) => void;
type WaveformListener = (sourceId: number, peaks: WaveformPeak[]) => void;

class PromiseHandler {
  private lastPromiseId = 1;
  private promises = new Map<number, { resolve: (val: any) => void; reject: (err: any) => void }>();
  private isAttached = false;

  public attach() {
    if (this.isAttached) return;
    if (typeof window !== 'undefined' && window.__JUCE__?.backend?.addEventListener) {
      window.__JUCE__.backend.addEventListener(
        "__juce__complete",
        (payload: any) => {
          try {
            const data = typeof payload === 'string' ? JSON.parse(payload) : payload;
            const promiseId = data.promiseId;
            const result = data.result;
            if (this.promises.has(promiseId)) {
              this.promises.get(promiseId)!.resolve(result);
              this.promises.delete(promiseId);
            }
          } catch (e) {
            console.error('[NativeBridge] Failed to handle __juce__complete:', e);
          }
        }
      );
      this.isAttached = true;
    }
  }

  public createPromise(): [number, Promise<any>] {
    const promiseId = this.lastPromiseId++;
    const promise = new Promise<any>((resolve, reject) => {
      this.promises.set(promiseId, { resolve, reject });
      // Set timeout in case backend fails
      setTimeout(() => {
        if (this.promises.has(promiseId)) {
          this.promises.delete(promiseId);
          reject(new Error(`Promise ${promiseId} timed out`));
        }
      }, 5000);
    });
    return [promiseId, promise];
  }
}

class NativeBridgeService {
  private promiseHandler = new PromiseHandler();
  private stateListeners: Set<StateListener> = new Set();
  private statsListeners: Set<StatsListener> = new Set();
  private waveformListeners: Set<WaveformListener> = new Set();
  private isConnectedToJuce = false;

  // Mock State for standalone web browser preview
  private mockState: GlobalState = {
    version: "1.0.0",
    masterVolume: 0.9,
    sources: [
      {
        id: 1,
        name: "Sub Glitch Sine",
        type: "Oscillator",
        assignedNote: 60,
        outputBus: 0,
        chokeGroup: 1,
        muted: false,
        soloed: false,
        gain: 0.85,
        pan: 0.0,
        pitchSemi: 0,
        pitchFine: 0,
        attackMs: 1.5,
        holdMs: 0.0,
        decayMs: 300,
        sustain: 0.0,
        releaseMs: 50,
        curve: -0.6,
        bitDepth: 16,
        bitcrushMix: 0.0,
        downsampleHz: 44100,
        downsampleMix: 0.0,
        stutterHz: 8,
        stutterDuty: 0.5,
        stutterMix: 0.0,
        stutterSync: false,
        stutterDivision: 2,
        waveform: 0,
        pulseWidth: 0.5,
        glitchMorph: 0.2,
        frequency: 440,
        pitchTrack: true
      }
    ]
  };

  constructor() {
    this.init();
  }

  private init() {
    this.checkJuceConnection();
  }

  private checkJuceConnection() {
    const tryConnect = () => {
      if (typeof window !== 'undefined' && window.__JUCE__?.backend?.emitEvent) {
        if (!this.isConnectedToJuce) {
          this.isConnectedToJuce = true;
          console.log('[NativeBridge] Connected to JUCE 8 WebBrowser backend.');

          const backend = window.__JUCE__.backend;
          this.promiseHandler.attach();

          if (backend.addEventListener) {
            backend.addEventListener('stateSync', (payload: any) => {
              try {
                const data: GlobalState = typeof payload === 'string' ? JSON.parse(payload) : payload;
                this.notifyStateListeners(data);
              } catch (e) {
                console.error('[NativeBridge] Failed to parse stateSync:', e);
              }
            });

            backend.addEventListener('voiceStats', (payload: any) => {
              try {
                const data: VoiceStats = typeof payload === 'string' ? JSON.parse(payload) : payload;
                this.statsListeners.forEach(cb => cb(data));
              } catch (e) {
                console.error('[NativeBridge] Failed to parse voiceStats:', e);
              }
            });

            backend.addEventListener('sampleWaveform', (peaksPayload: any) => {
              try {
                const peaks: WaveformPeak[] = typeof peaksPayload === 'string' ? JSON.parse(peaksPayload) : peaksPayload;
                // Default to selected source or first
                this.waveformListeners.forEach(cb => cb(1, peaks));
              } catch (e) {
                console.error('[NativeBridge] Failed to parse sampleWaveform:', e);
              }
            });
          }

          // Request initial state from C++
          this.requestState();
        }
        return true;
      }
      return false;
    };

    if (!tryConnect()) {
      let attempts = 0;
      const timer = setInterval(() => {
        attempts++;
        if (tryConnect() || attempts > 60) {
          clearInterval(timer);
          if (!this.isConnectedToJuce) {
            console.log('[NativeBridge] JUCE backend not found. Running in browser mock mode.');
            // Simulate metering in browser
            setInterval(() => {
              this.statsListeners.forEach(cb => cb({
                activeVoices: Math.floor(Math.random() * 3),
                peakL: Math.random() * 0.6,
                peakR: Math.random() * 0.6
              }));
            }, 100);
          }
        }
      }, 50);
    }
  }

  private async callNative(fnName: string, ...args: any[]): Promise<any> {
    const juceBackend = (typeof window !== 'undefined' && window.__JUCE__?.backend);

    if (juceBackend && typeof juceBackend.emitEvent === 'function') {
      const [promiseId, promise] = this.promiseHandler.createPromise();

      juceBackend.emitEvent("__juce__invoke", {
        name: fnName,
        params: args,
        resultId: promiseId,
      });

      try {
        const result = await promise;
        // If result contains the full state object, update immediately
        if (result && typeof result === 'object' && Array.isArray(result.sources)) {
          this.notifyStateListeners(result as GlobalState);
        }
        return result;
      } catch (err) {
        console.warn(`[NativeBridge] callNative '${fnName}' warning/error:`, err);
      }
    } else {
      // Mock logic in standalone browser
      this.handleMockCall(fnName, args);
    }
  }

  private handleMockCall(fnName: string, args: any[]) {
    switch (fnName) {
      case 'addSource': {
        const type: SourceType = args[0] || 'Oscillator';
        const newId = (this.mockState.sources.length > 0)
          ? Math.max(...this.mockState.sources.map(s => s.id)) + 1
          : 1;
        const newSource: SoundSourceData = {
          id: newId,
          name: `${type} ${newId}`,
          type,
          assignedNote: -1,
          outputBus: 0,
          chokeGroup: 0,
          muted: false,
          soloed: false,
          gain: 0.8,
          pan: 0.0,
          pitchSemi: 0,
          pitchFine: 0,
          attackMs: 2.0,
          holdMs: 0.0,
          decayMs: 150,
          sustain: 0.5,
          releaseMs: 80,
          curve: -0.5,
          bitDepth: 16,
          bitcrushMix: 0.0,
          downsampleHz: 44100,
          downsampleMix: 0.0,
          stutterHz: 8,
          stutterDuty: 0.5,
          stutterMix: 0.0,
          stutterSync: false,
          stutterDivision: 2,
          ...(type === 'Oscillator' ? { waveform: 0, pulseWidth: 0.5, glitchMorph: 0.0, frequency: 440, pitchTrack: true } : {}),
          ...(type === 'Noise' ? { noiseType: 2, crackleDensity: 500, hashRate: 4400 } : {}),
          ...(type === 'Sample' ? {
            filePath: "untitled_click.wav",
            startPoint: 0.0,
            endPoint: 1.0,
            reverse: false,
            speed: 1.0,
            microLoop: false,
            loopStart: 0.0,
            loopLengthMs: 20,
            crossfadeMs: 2.0
          } : {}),
          ...(type === 'Click' ? {
            name: `Click ${newId}`,
            attackMs: 0.0,
            holdMs: 0.0,
            decayMs: 0.0,
            sustain: 0.0,
            releaseMs: 0.0,
            curve: 0.0,
            clickType: 0,
            clickWidthSamples: 1,
            clickFrequency: 1200,
            clickDamping: 0.65,
            clickPitchTrack: false,
            clickPolarity: 0
          } : {})
        };
        this.mockState.sources.push(newSource);
        this.notifyStateListeners({ ...this.mockState });
        break;
      }
      case 'removeSource': {
        const id = args[0];
        this.mockState.sources = this.mockState.sources.filter(s => s.id !== id);
        this.notifyStateListeners({ ...this.mockState });
        break;
      }
      case 'cloneSource': {
        const id = args[0];
        const target = this.mockState.sources.find(s => s.id === id);
        if (target) {
          const newId = Math.max(...this.mockState.sources.map(s => s.id)) + 1;
          const clone = JSON.parse(JSON.stringify(target));
          clone.id = newId;
          clone.name = `${target.name} (Copy)`;
          this.mockState.sources.push(clone);
          this.notifyStateListeners({ ...this.mockState });
        }
        break;
      }
      case 'updateParameter': {
        const [sourceId, paramId, value] = args;
        const src = this.mockState.sources.find(s => s.id === sourceId);
        if (src) {
          (src as any)[paramId] = value;
          this.notifyStateListeners({ ...this.mockState });
        }
        break;
      }
      case 'setMasterVolume': {
        this.mockState.masterVolume = args[0];
        this.notifyStateListeners({ ...this.mockState });
        break;
      }
      case 'openFileDialog': {
        const sourceId = args[0];
        const src = this.mockState.sources.find(s => s.id === sourceId);
        if (src) {
          src.filePath = "amen_glitch_break_174bpm.wav";
          this.notifyStateListeners({ ...this.mockState });
        }
        break;
      }
      case 'requestState': {
        this.notifyStateListeners({ ...this.mockState });
        break;
      }
      case 'randomizeSet': {
        const archetype = Math.floor(Math.random() * 4);
        const numSources = 16;
        const newSources: SoundSourceData[] = [];

        for (let i = 0; i < numSources; i++) {
          const id = i + 1;
          // 16 notes starting from Middle C (C3 / MIDI 60) spaced by 1 whole tone (2 semitones) ascending: 60..90
          const assignedNote = 60 + (i * 2);
          const pan = Math.min(1.0, Math.max(-1.0, -0.75 + (1.5 / Math.max(1, numSources - 1)) * i + (Math.random() - 0.5) * 0.15));
          const chokeGroup = (archetype === 0 || archetype === 3) ? (1 + (i % 2)) : ((i % 3 === 0) ? 1 : 0);

          let typeChoice: SourceType;
          if (archetype === 0) {
            typeChoice = (i % 4 === 0 || i % 4 === 1) ? 'Click' : ((i % 4 === 2) ? 'Noise' : 'Oscillator');
          } else if (archetype === 1) {
            typeChoice = (i % 3 === 0) ? 'Noise' : ((i % 3 === 1) ? 'Click' : 'Oscillator');
          } else if (archetype === 2) {
            typeChoice = (i % 3 === 0) ? 'Oscillator' : ((i % 3 === 1) ? 'Click' : 'Noise');
          } else {
            typeChoice = (i % 4 === 3) ? 'Noise' : ((i % 8 === 7) ? 'Oscillator' : 'Click');
          }

          const hasBitcrush = Math.random() < 0.45;
          const hasDownsample = Math.random() < 0.4;
          const hasStutter = typeChoice === 'Oscillator' && Math.random() < 0.6;

          const base: SoundSourceData = {
            id,
            name: `${typeChoice} ${id}`,
            type: typeChoice,
            assignedNote,
            outputBus: 0,
            chokeGroup,
            muted: false,
            soloed: false,
            gain: parseFloat((0.68 + Math.random() * 0.16).toFixed(2)),
            pan: parseFloat(pan.toFixed(2)),
            pitchSemi: 0,
            pitchFine: 0,
            attackMs: parseFloat((0.01 + Math.random() * 1.5).toFixed(2)),
            holdMs: 0,
            decayMs: Math.floor(15 + Math.random() * 180),
            sustain: 0,
            releaseMs: Math.floor(10 + Math.random() * 40),
            curve: parseFloat((-0.8 + Math.random() * 0.4).toFixed(2)),
            bitDepth: hasBitcrush ? Math.floor(3 + Math.random() * 6) : 16,
            bitcrushMix: hasBitcrush ? parseFloat((0.4 + Math.random() * 0.5).toFixed(2)) : 0,
            downsampleHz: hasDownsample ? Math.floor(800 + Math.random() * 6000) : 44100,
            downsampleMix: hasDownsample ? parseFloat((0.4 + Math.random() * 0.5).toFixed(2)) : 0,
            stutterHz: hasStutter ? Math.floor(6 + Math.random() * 26) : 8,
            stutterDuty: hasStutter ? parseFloat((0.35 + Math.random() * 0.35).toFixed(2)) : 0.5,
            stutterMix: hasStutter ? parseFloat((0.5 + Math.random() * 0.45).toFixed(2)) : 0,
            stutterSync: false,
            stutterDivision: [1, 2, 4, 8][Math.floor(Math.random() * 4)]
          };

          if (typeChoice === 'Click') {
            const clickType = Math.floor(Math.random() * 4);
            const clickNames = ["Dirac", "ResoPop", "Chirp", "BitPulse"];
            base.name = `${clickNames[clickType]} ${id}`;
            base.clickType = clickType;
            base.clickWidthSamples = 1 + Math.floor(Math.random() * 4);
            base.clickFrequency = Math.floor(400 + Math.random() * 3200);
            base.clickDamping = parseFloat((0.4 + Math.random() * 0.45).toFixed(2));
            base.clickPolarity = Math.random() > 0.5 ? 1 : 0;
            base.clickPitchTrack = Math.random() > 0.5;
            base.attackMs = 0.01;
            base.decayMs = Math.floor(8 + Math.random() * 45);
          } else if (typeChoice === 'Noise') {
            const noiseType = Math.floor(Math.random() * 4);
            const noiseNames = ["WhiteBurst", "PinkCrackle", "Poisson", "HashNoise"];
            base.name = `${noiseNames[noiseType]} ${id}`;
            base.noiseType = noiseType;
            base.crackleDensity = Math.floor(200 + Math.random() * 2400);
            base.hashRate = Math.floor(800 + Math.random() * 8000);
          } else {
            const waveIdx = Math.floor(Math.random() * 5);
            const waveNames = ["GlitchSine", "SquareHit", "SawTooth", "SubTriangle", "FoldWave"];
            base.name = `${waveNames[waveIdx]} ${id}`;
            base.waveform = waveIdx;
            base.pulseWidth = parseFloat((0.2 + Math.random() * 0.6).toFixed(2));
            base.glitchMorph = parseFloat((0.1 + Math.random() * 0.8).toFixed(2));
            const semis = [-24, -12, 0, 7, 12, 19, 24];
            base.pitchSemi = semis[Math.floor(Math.random() * semis.length)];
            base.pitchFine = Math.floor((Math.random() - 0.5) * 20);
            base.frequency = 440;
            base.pitchTrack = true;
          }

          newSources.push(base);
        }

        this.mockState.sources = newSources;
        this.notifyStateListeners({ ...this.mockState });
        break;
      }
    }
  }

  private notifyStateListeners(state: GlobalState) {
    this.stateListeners.forEach(cb => cb(state));
  }

  // Public API
  public addSource(type: SourceType) {
    this.callNative('addSource', type);
  }

  public removeSource(sourceId: number) {
    this.callNative('removeSource', sourceId);
  }

  public cloneSource(sourceId: number) {
    this.callNative('cloneSource', sourceId);
  }

  public updateParameter(sourceId: number, paramId: string, value: number | string | boolean) {
    this.callNative('updateParameter', sourceId, paramId, value);
  }

  public openFileDialog(sourceId: number) {
    this.callNative('openFileDialog', sourceId);
  }

  public loadSampleFile(sourceId: number, filePath: string) {
    this.callNative('loadSampleFile', sourceId, filePath);
  }

  public setMasterVolume(vol: number) {
    this.callNative('setMasterVolume', vol);
  }

  public panic() {
    this.callNative('panic');
  }

  public loadPreset(presetIndex: number) {
    this.callNative('loadPreset', presetIndex);
  }

  public savePreset(presetJson: string) {
    this.callNative('savePreset', presetJson);
  }

  public randomizeSet() {
    this.callNative('randomizeSet');
  }

  public noteOn(note: number, velocity: number = 0.8) {
    this.callNative('noteOn', note, velocity);
  }

  public noteOff(note: number) {
    this.callNative('noteOff', note);
  }

  public requestState() {
    this.callNative('requestState');
  }

  public subscribeState(listener: StateListener): () => void {
    this.stateListeners.add(listener);
    this.requestState();
    return () => this.stateListeners.delete(listener);
  }

  public subscribeVoiceStats(listener: StatsListener): () => void {
    this.statsListeners.add(listener);
    return () => this.statsListeners.delete(listener);
  }

  public subscribeWaveform(listener: WaveformListener): () => void {
    this.waveformListeners.add(listener);
    return () => this.waveformListeners.delete(listener);
  }
}

export const NativeBridge = new NativeBridgeService();
