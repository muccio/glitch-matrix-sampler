import { GlobalState, SoundSourceData, VoiceStats, WaveformPeak, SourceType } from '../types/matrix';

declare global {
  interface Window {
    __JUCE__?: {
      backend?: {
        emit?: (eventName: string, ...args: any[]) => void;
        addEventListener?: (eventName: string, callback: (...args: any[]) => void) => void;
        removeEventListener?: (eventName: string, callback: (...args: any[]) => void) => void;
        [key: string]: any;
      };
    };
  }
}

type StateListener = (state: GlobalState) => void;
type StatsListener = (stats: VoiceStats) => void;
type WaveformListener = (sourceId: number, peaks: WaveformPeak[]) => void;

class NativeBridgeService {
  private isInsideJuce: boolean = false;
  private stateListeners: Set<StateListener> = new Set();
  private statsListeners: Set<StatsListener> = new Set();
  private waveformListeners: Set<WaveformListener> = new Set();

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
      },
      {
        id: 2,
        name: "Poisson Clicker",
        type: "Noise",
        assignedNote: 60,
        chokeGroup: 1,
        muted: false,
        soloed: false,
        gain: 0.9,
        pan: 0.15,
        pitchSemi: 12,
        pitchFine: 0,
        attackMs: 0.1,
        holdMs: 0.0,
        decayMs: 25,
        sustain: 0.0,
        releaseMs: 10,
        curve: -0.9,
        bitDepth: 3,
        bitcrushMix: 0.7,
        downsampleHz: 12000,
        downsampleMix: 0.3,
        stutterHz: 16,
        stutterDuty: 0.5,
        stutterMix: 0.0,
        stutterSync: false,
        stutterDivision: 2,
        noiseType: 2,
        crackleDensity: 2400,
        hashRate: 4400
      },
      {
        id: 3,
        name: "IDM Micro-Sample",
        type: "Sample",
        assignedNote: -1,
        chokeGroup: 2,
        muted: false,
        soloed: false,
        gain: 0.8,
        pan: -0.2,
        pitchSemi: 0,
        pitchFine: 0,
        attackMs: 1.0,
        holdMs: 0.0,
        decayMs: 200,
        sustain: 0.7,
        releaseMs: 80,
        curve: -0.5,
        bitDepth: 12,
        bitcrushMix: 0.4,
        downsampleHz: 8000,
        downsampleMix: 0.5,
        stutterHz: 32,
        stutterDuty: 0.6,
        stutterMix: 0.65,
        stutterSync: true,
        stutterDivision: 3,
        filePath: "drum_break_amen_glitch.wav",
        startPoint: 0.12,
        endPoint: 0.85,
        reverse: false,
        speed: 1.2,
        microLoop: true,
        loopStart: 0.25,
        loopLengthMs: 35,
        crossfadeMs: 3.5
      }
    ]
  };

  constructor() {
    this.init();
  }

  private init() {
    if (typeof window !== 'undefined' && window.__JUCE__ && window.__JUCE__.backend) {
      this.isInsideJuce = true;
      const backend = window.__JUCE__.backend;

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

        backend.addEventListener('sampleWaveform', (sourceId: number, peaksPayload: any) => {
          try {
            const peaks: WaveformPeak[] = typeof peaksPayload === 'string' ? JSON.parse(peaksPayload) : peaksPayload;
            this.waveformListeners.forEach(cb => cb(sourceId, peaks));
          } catch (e) {
            console.error('[NativeBridge] Failed to parse sampleWaveform:', e);
          }
        });
      }
    } else {
      console.log('[NativeBridge] Running in standalone web browser mode (Mock IPC active).');
      // Simulate periodic audio metering in browser
      setInterval(() => {
        this.statsListeners.forEach(cb => cb({
          activeVoices: Math.floor(Math.random() * 4),
          peakL: Math.random() * 0.7,
          peakR: Math.random() * 0.7
        }));
      }, 100);
    }
  }

  private callNative(fnName: string, ...args: any[]) {
    if (this.isInsideJuce && window.__JUCE__?.backend) {
      const backend = window.__JUCE__.backend;
      if (typeof backend[fnName] === 'function') {
        backend[fnName](...args);
      } else if (typeof backend.emit === 'function') {
        backend.emit(fnName, ...args);
      }
    } else {
      // Mock logic in browser
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
    if (this.isInsideJuce) {
      this.callNative('requestState');
    } else {
      this.notifyStateListeners({ ...this.mockState });
    }
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
