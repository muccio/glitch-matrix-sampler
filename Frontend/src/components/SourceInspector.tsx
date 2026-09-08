import React from 'react';
import { SoundSourceData } from '../types/matrix';
import { EnvelopeEditor } from './EnvelopeEditor';
import { GlitchFXPanel } from './GlitchFXPanel';
import { WaveformViewer } from './WaveformViewer';
import { Sliders, Volume2, MoveHorizontal, Disc, Cpu, Radio, Zap } from 'lucide-react';

interface SourceInspectorProps {
  source: SoundSourceData | null;
  onUpdate: (paramId: string, value: any) => void;
}

const OSC_WAVEFORMS = ['Sine', 'Square', 'Saw', 'Triangle', 'Glitch Wavetable'];
const NOISE_TYPES = ['White', 'Pink', 'Crackle', 'Bit-Flip Hash'];
const CLICK_MODELS = ['Dirac Needle', 'Resonant Pop', 'Micro Chirp', 'Bit-Flip'];
const POLARITY_OPTIONS = ['+ Pos', '- Neg', '± Bipolar'];

export const SourceInspector: React.FC<SourceInspectorProps> = ({ source, onUpdate }) => {
  if (!source) {
    return (
      <div className="flex-1 flex flex-col items-center justify-center text-glitch-dim text-xs select-none">
        <Sliders className="w-8 h-8 text-glitch-border mb-2 animate-pulse" />
        <span>SELECT A SOUND SOURCE FROM THE LEFT MATRIX</span>
      </div>
    );
  }

  return (
    <div className="flex-1 overflow-y-auto p-4 flex flex-col gap-4 select-none custom-scrollbar">
      {/* Top Header: Source Info & Common Tuning / Mixing */}
      <div className="bg-glitch-panel/40 border border-glitch-border rounded p-3 flex items-center justify-between">
        {/* Source Name & Type Badge */}
        <div className="flex items-center gap-3">
          <div className="flex items-center gap-1.5 px-2.5 py-1 bg-glitch-dark rounded border border-glitch-border text-xs font-bold text-glitch-cyan">
            {source.type === 'Oscillator' && <Cpu className="w-3.5 h-3.5 text-glitch-cyan" />}
            {source.type === 'Noise' && <Radio className="w-3.5 h-3.5 text-glitch-amber" />}
            {source.type === 'Sample' && <Disc className="w-3.5 h-3.5 text-glitch-pink" />}
            {source.type === 'Click' && <Zap className="w-3.5 h-3.5 text-emerald-400" />}
            <span className={source.type === 'Click' ? 'text-emerald-400' : ''}>{source.type.toUpperCase()}</span>
          </div>

          <input
            type="text"
            value={source.name}
            onChange={(e) => onUpdate('name', e.target.value)}
            className="bg-glitch-dark/80 text-glitch-text font-bold text-sm px-2.5 py-1 rounded border border-glitch-border focus:border-glitch-cyan outline-none"
          />
        </div>

        {/* Global Pitch & Pan & Gain Strip */}
        <div className="flex items-center gap-4 text-xs">
          {/* SEMITONES */}
          <div className="flex items-center gap-1 bg-glitch-dark px-2 py-1 rounded border border-glitch-border">
            <span className="text-[10px] text-glitch-dim">SEMI:</span>
            <input
              type="number"
              min="-48"
              max="48"
              value={source.pitchSemi}
              onChange={(e) => onUpdate('pitchSemi', parseFloat(e.target.value))}
              className="w-10 bg-transparent text-glitch-cyan font-bold text-right outline-none"
            />
          </div>

          {/* FINE CENTS */}
          <div className="flex items-center gap-1 bg-glitch-dark px-2 py-1 rounded border border-glitch-border">
            <span className="text-[10px] text-glitch-dim">FINE:</span>
            <input
              type="number"
              min="-100"
              max="100"
              value={source.pitchFine}
              onChange={(e) => onUpdate('pitchFine', parseFloat(e.target.value))}
              className="w-10 bg-transparent text-glitch-cyan font-bold text-right outline-none"
            />
          </div>

          {/* PAN */}
          <div className="flex items-center gap-1 bg-glitch-dark px-2 py-1 rounded border border-glitch-border">
            <MoveHorizontal className="w-3 h-3 text-glitch-dim" />
            <span className="text-[10px] text-glitch-dim">PAN:</span>
            <input
              type="range"
              min="-1"
              max="1"
              step="0.05"
              value={source.pan}
              onChange={(e) => onUpdate('pan', parseFloat(e.target.value))}
              className="w-16 h-1 accent-glitch-cyan cursor-pointer"
            />
          </div>

          {/* GAIN */}
          <div className="flex items-center gap-1 bg-glitch-dark px-2 py-1 rounded border border-glitch-border">
            <Volume2 className="w-3 h-3 text-glitch-dim" />
            <span className="text-[10px] text-glitch-dim">GAIN:</span>
            <input
              type="range"
              min="0"
              max="2.0"
              step="0.02"
              value={source.gain}
              onChange={(e) => onUpdate('gain', parseFloat(e.target.value))}
              className="w-16 h-1 accent-glitch-cyan cursor-pointer"
            />
            <span className="text-[10px] text-glitch-cyan font-bold w-7 text-right">
              {(source.gain * 100).toFixed(0)}%
            </span>
          </div>
        </div>
      </div>

      {/* TYPE SPECIFIC PANEL */}
      {source.type === 'Oscillator' && (
        <div className="bg-glitch-panel/40 border border-glitch-border rounded p-3 flex flex-col gap-2.5">
          <span className="text-xs font-bold text-glitch-cyan tracking-wider">
            POLYBLEP OSCILLATOR & GLITCH WAVETABLE
          </span>

          {/* Waveform selection tabs */}
          <div className="grid grid-cols-5 gap-1.5">
            {OSC_WAVEFORMS.map((name, idx) => (
              <button
                key={idx}
                onClick={() => onUpdate('waveform', idx)}
                className={`py-1.5 px-2 rounded text-[11px] font-bold border transition-all ${
                  (source.waveform ?? 0) === idx
                    ? 'bg-glitch-cyan text-glitch-dark border-glitch-cyan shadow-neon-cyan'
                    : 'bg-glitch-surface text-glitch-dim border-glitch-border hover:text-glitch-text'
                }`}
              >
                {name.toUpperCase()}
              </button>
            ))}
          </div>

          {/* Specific controls: Pulse Width & Morph */}
          <div className="grid grid-cols-2 gap-3 text-xs pt-1">
            <div className="bg-glitch-dark/70 p-2 rounded border border-glitch-border flex flex-col gap-1">
              <div className="flex justify-between text-[10px]">
                <span className="text-glitch-dim">PULSE WIDTH (PWM):</span>
                <span className="text-glitch-cyan font-bold">
                  {((source.pulseWidth ?? 0.5) * 100).toFixed(0)}%
                </span>
              </div>
              <input
                type="range"
                min="0.05"
                max="0.95"
                step="0.01"
                value={source.pulseWidth ?? 0.5}
                onChange={(e) => onUpdate('pulseWidth', parseFloat(e.target.value))}
                className="w-full h-1 accent-glitch-cyan cursor-pointer"
              />
            </div>

            <div className="bg-glitch-dark/70 p-2 rounded border border-glitch-border flex flex-col gap-1">
              <div className="flex justify-between text-[10px]">
                <span className="text-glitch-dim">GLITCH WAVETABLE MORPH:</span>
                <span className="text-glitch-cyan font-bold">
                  {((source.glitchMorph ?? 0.0) * 100).toFixed(0)}%
                </span>
              </div>
              <input
                type="range"
                min="0"
                max="1"
                step="0.01"
                value={source.glitchMorph ?? 0.0}
                onChange={(e) => onUpdate('glitchMorph', parseFloat(e.target.value))}
                className="w-full h-1 accent-glitch-cyan cursor-pointer"
              />
            </div>
          </div>
        </div>
      )}

      {source.type === 'Noise' && (
        <div className="bg-glitch-panel/40 border border-glitch-border rounded p-3 flex flex-col gap-2.5">
          <span className="text-xs font-bold text-glitch-amber tracking-wider">
            NOISE & DIGITAL IMPULSE GENERATOR
          </span>

          {/* Noise Type selection tabs */}
          <div className="grid grid-cols-4 gap-1.5">
            {NOISE_TYPES.map((name, idx) => (
              <button
                key={idx}
                onClick={() => onUpdate('noiseType', idx)}
                className={`py-1.5 px-2 rounded text-[11px] font-bold border transition-all ${
                  (source.noiseType ?? 0) === idx
                    ? 'bg-glitch-amber text-glitch-dark border-glitch-amber shadow-neon-amber'
                    : 'bg-glitch-surface text-glitch-dim border-glitch-border hover:text-glitch-text'
                }`}
              >
                {name.toUpperCase()}
              </button>
            ))}
          </div>

          <div className="grid grid-cols-2 gap-3 text-xs pt-1">
            <div className="bg-glitch-dark/70 p-2 rounded border border-glitch-border flex flex-col gap-1">
              <div className="flex justify-between text-[10px]">
                <span className="text-glitch-dim">CRACKLE DENSITY (POISSON):</span>
                <span className="text-glitch-amber font-bold">
                  {Math.round(source.crackleDensity ?? 100)} /s
                </span>
              </div>
              <input
                type="range"
                min="5"
                max="5000"
                step="5"
                value={source.crackleDensity ?? 100}
                onChange={(e) => onUpdate('crackleDensity', parseFloat(e.target.value))}
                className="w-full h-1 accent-glitch-amber cursor-pointer"
              />
            </div>

            <div className="bg-glitch-dark/70 p-2 rounded border border-glitch-border flex flex-col gap-1">
              <div className="flex justify-between text-[10px]">
                <span className="text-glitch-dim">HASH CLOCK RATE:</span>
                <span className="text-glitch-amber font-bold">
                  {Math.round(source.hashRate ?? 4400)} Hz
                </span>
              </div>
              <input
                type="range"
                min="50"
                max="22000"
                step="50"
                value={source.hashRate ?? 4400}
                onChange={(e) => onUpdate('hashRate', parseFloat(e.target.value))}
                className="w-full h-1 accent-glitch-amber cursor-pointer"
              />
            </div>
          </div>
        </div>
      )}

      {source.type === 'Sample' && (
        <WaveformViewer
          sourceId={source.id}
          filePath={source.filePath}
          startPoint={source.startPoint ?? 0.0}
          endPoint={source.endPoint ?? 1.0}
          reverse={source.reverse ?? false}
          speed={source.speed ?? 1.0}
          microLoop={source.microLoop ?? false}
          loopStart={source.loopStart ?? 0.0}
          loopLengthMs={source.loopLengthMs ?? 20.0}
          crossfadeMs={source.crossfadeMs ?? 2.0}
          onChange={onUpdate}
        />
      )}

      {/* CLICK TRANSIENT SYNTHESIZER PANEL */}
      {source.type === 'Click' && (
        <div className="bg-glitch-panel/40 border border-glitch-border rounded p-3 flex flex-col gap-2.5">
          <div className="flex items-center justify-between">
            <span className="text-xs font-bold text-emerald-400 tracking-wider flex items-center gap-1.5">
              <Zap className="w-3.5 h-3.5" />
              MICRO-CLICK TRANSIENT SYNTHESIZER
            </span>
            <span className="text-[10px] text-glitch-dim">SUB-MILLISECOND PRECISION</span>
          </div>

          {/* Click Model selection tabs */}
          <div className="grid grid-cols-4 gap-1.5">
            {CLICK_MODELS.map((name, idx) => (
              <button
                key={idx}
                onClick={() => onUpdate('clickType', idx)}
                className={`py-1.5 px-2 rounded text-[11px] font-bold border transition-all ${
                  (source.clickType ?? 0) === idx
                    ? 'bg-emerald-500 text-glitch-dark border-emerald-500 shadow-neon-green'
                    : 'bg-glitch-surface text-glitch-dim border-glitch-border hover:text-glitch-text'
                }`}
              >
                {name.toUpperCase()}
              </button>
            ))}
          </div>

          {/* Model-specific controls */}
          <div className="grid grid-cols-3 gap-3 text-xs pt-1">
            {/* Pulse Width (for Dirac & BitFlip) */}
            <div className="bg-glitch-dark/70 p-2 rounded border border-glitch-border flex flex-col gap-1">
              <div className="flex justify-between text-[10px]">
                <span className="text-glitch-dim">PULSE WIDTH:</span>
                <span className="text-emerald-400 font-bold">
                  {source.clickWidthSamples ?? 4} smp
                </span>
              </div>
              <input
                type="range"
                min="1"
                max="64"
                step="1"
                value={source.clickWidthSamples ?? 4}
                onChange={(e) => onUpdate('clickWidthSamples', parseInt(e.target.value, 10))}
                className="w-full h-1 accent-emerald-400 cursor-pointer"
              />
            </div>

            {/* Resonance Frequency (for Resonant & Chirp) */}
            <div className="bg-glitch-dark/70 p-2 rounded border border-glitch-border flex flex-col gap-1">
              <div className="flex justify-between text-[10px]">
                <span className="text-glitch-dim">FREQ / TONE:</span>
                <span className="text-emerald-400 font-bold">
                  {Math.round(source.clickFrequency ?? 1200)} Hz
                </span>
              </div>
              <input
                type="range"
                min="50"
                max="16000"
                step="10"
                value={source.clickFrequency ?? 1200}
                onChange={(e) => onUpdate('clickFrequency', parseFloat(e.target.value))}
                className="w-full h-1 accent-emerald-400 cursor-pointer"
              />
            </div>

            {/* Damping / Snap */}
            <div className="bg-glitch-dark/70 p-2 rounded border border-glitch-border flex flex-col gap-1">
              <div className="flex justify-between text-[10px]">
                <span className="text-glitch-dim">DAMPING / SNAP:</span>
                <span className="text-emerald-400 font-bold">
                  {((source.clickDamping ?? 0.65) * 100).toFixed(0)}%
                </span>
              </div>
              <input
                type="range"
                min="0.05"
                max="0.99"
                step="0.01"
                value={source.clickDamping ?? 0.65}
                onChange={(e) => onUpdate('clickDamping', parseFloat(e.target.value))}
                className="w-full h-1 accent-emerald-400 cursor-pointer"
              />
            </div>
          </div>

          {/* Polarity and MIDI Pitch Track row */}
          <div className="grid grid-cols-2 gap-3 text-xs pt-1">
            {/* Polarity selector */}
            <div className="bg-glitch-dark/70 p-2 rounded border border-glitch-border flex items-center justify-between px-2 py-1.5">
              <span className="text-[10px] text-glitch-dim">IMPULSE POLARITY:</span>
              <div className="flex gap-1">
                {POLARITY_OPTIONS.map((pol, idx) => (
                  <button
                    key={idx}
                    onClick={() => onUpdate('clickPolarity', idx)}
                    className={`py-0.5 px-2 rounded text-[10px] font-bold border transition-all ${
                      (source.clickPolarity ?? 0) === idx
                        ? 'bg-emerald-500 text-glitch-dark border-emerald-500'
                        : 'bg-glitch-surface text-glitch-dim border-glitch-border hover:text-glitch-text'
                    }`}
                  >
                    {pol}
                  </button>
                ))}
              </div>
            </div>

            {/* MIDI Pitch Track toggle */}
            <div className="bg-glitch-dark/70 p-2 rounded border border-glitch-border flex items-center justify-between px-2 py-1.5">
              <span className="text-[10px] text-glitch-dim">MIDI KEY TRACKING:</span>
              <button
                onClick={() => onUpdate('clickPitchTrack', !source.clickPitchTrack)}
                className={`py-0.5 px-3 rounded text-[10px] font-bold border transition-all ${
                  source.clickPitchTrack
                    ? 'bg-emerald-500 text-glitch-dark border-emerald-500 shadow-neon-green'
                    : 'bg-glitch-surface text-glitch-dim border-glitch-border hover:text-glitch-text'
                }`}
              >
                {source.clickPitchTrack ? 'ENABLED (TUNED)' : 'FIXED FREQ'}
              </button>
            </div>
          </div>
        </div>
      )}

      {/* AHDSR Fast Envelope Section */}
      <EnvelopeEditor
        envelope={{
          attackMs: source.attackMs,
          holdMs: source.holdMs,
          decayMs: source.decayMs,
          sustain: source.sustain,
          releaseMs: source.releaseMs,
          curve: source.curve
        }}
        onChange={onUpdate}
      />

      {/* Glitch Multi-FX Section */}
      <GlitchFXPanel
        fx={{
          bitDepth: source.bitDepth,
          bitcrushMix: source.bitcrushMix,
          downsampleHz: source.downsampleHz,
          downsampleMix: source.downsampleMix,
          stutterHz: source.stutterHz,
          stutterDuty: source.stutterDuty,
          stutterMix: source.stutterMix,
          stutterSync: source.stutterSync,
          stutterDivision: source.stutterDivision
        }}
        onChange={onUpdate}
      />
    </div>
  );
};
