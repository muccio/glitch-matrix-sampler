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
        glitchMorph: 0.2
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
          ...(type === 'Oscillator' ? { waveform: 0, pulseWidth: 0.5, glitchMorph: 0.0 } : {}),
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
