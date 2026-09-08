export type SourceType = 'Oscillator' | 'Noise' | 'Sample' | 'Click';
export type OscWaveform = 'Sine' | 'Square' | 'Saw' | 'Triangle' | 'GlitchWavetable';
export type NoiseType = 'White' | 'Pink' | 'Crackle' | 'BitFlipHash';
export type ClickModel = 'Dirac' | 'Resonant' | 'Chirp' | 'BitFlip';

export interface EnvelopeData {
  attackMs: number;
  holdMs: number;
  decayMs: number;
  sustain: number;
  releaseMs: number;
  curve: number;
}

export interface GlitchFXData {
  bitDepth: number;
  bitcrushMix: number;
  downsampleHz: number;
  downsampleMix: number;
  stutterHz: number;
  stutterDuty: number;
  stutterMix: number;
  stutterSync: boolean;
  stutterDivision: number;
}

export interface SoundSourceData {
  id: number;
  name: string;
  type: SourceType;
  assignedNote: number; // -1 for ALL/omni, 0-127 for specific note
  chokeGroup: number;   // 0 = Off, 1-8
  muted: boolean;
  soloed: boolean;
  gain: number;
  pan: number;
  pitchSemi: number;
  pitchFine: number;

  // Envelope & FX
  attackMs: number;
  holdMs: number;
  decayMs: number;
  sustain: number;
  releaseMs: number;
  curve: number;

  bitDepth: number;
  bitcrushMix: number;
  downsampleHz: number;
  downsampleMix: number;
  stutterHz: number;
  stutterDuty: number;
  stutterMix: number;
  stutterSync: boolean;
  stutterDivision: number;

  // Oscillator-specific
  waveform?: number;
  pulseWidth?: number;
  glitchMorph?: number;

  // Noise-specific
  noiseType?: number;
  crackleDensity?: number;
  hashRate?: number;

  // Sample-specific
  filePath?: string;
  startPoint?: number;
  endPoint?: number;
  reverse?: boolean;
  speed?: number;
  microLoop?: boolean;
  loopStart?: number;
  loopLengthMs?: number;
  crossfadeMs?: number;

  // Click-specific
  clickType?: number;
  clickWidthSamples?: number;
  clickFrequency?: number;
  clickDamping?: number;
  clickPitchTrack?: boolean;
  clickPolarity?: number;
}

export interface WaveformPeak {
  min: number;
  max: number;
}

export interface PresetItem {
  name: string;
  description: string;
  state?: {
    masterVolume: number;
    sources: SoundSourceData[];
  };
}

export interface GlobalState {
  version: string;
  masterVolume: number;
  sources: SoundSourceData[];
}

export interface VoiceStats {
  activeVoices: number;
  peakL: number;
  peakR: number;
}
