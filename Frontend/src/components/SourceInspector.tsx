import React from 'react';
import { SoundSourceData } from '../types/matrix';
import { EnvelopeEditor } from './EnvelopeEditor';
import { GlitchFXPanel } from './GlitchFXPanel';
import { WaveformViewer } from './WaveformViewer';
import { getNoteLabel } from './SourceCard';
import { Sliders, Volume2, MoveHorizontal, Disc, Cpu, Radio, Zap, Share2 } from 'lucide-react';

interface SourceInspectorProps {
  source: SoundSourceData | null;
  onUpdate: (paramId: string, value: any) => void;
}

const OSC_WAVEFORMS = ['Sine', 'Square', 'Saw', 'Triangle', 'Glitch Wavetable'];
const NOISE_TYPES = ['White', 'Pink', 'Crackle', 'Bit-Flip Hash'];

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

      {/* Routing & Multi-Out Bar */}
      <div className="bg-glitch-panel/40 border border-glitch-border rounded px-3 py-2 flex items-center justify-between text-xs">
        <div className="flex items-center gap-3">
          <span className="text-[10px] font-bold text-glitch-dim tracking-wider">ROUTING:</span>

          {/* MIDI NOTE */}
          <div className="flex items-center gap-1.5 bg-glitch-dark px-2.5 py-1 rounded border border-glitch-border text-xs">
            <span className="text-[10px] text-glitch-dim">NOTE:</span>
            <select
              value={source.assignedNote}
              onChange={(e) => onUpdate('assignedNote', parseInt(e.target.value))}
              className="bg-transparent text-glitch-cyan font-bold outline-none cursor-pointer"
            >
              <option value={-1} className="bg-glitch-panel text-glitch-text">OMNI (ALL)</option>
              {Array.from({ length: 128 }, (_, i) => (
                <option key={i} value={i} className="bg-glitch-panel text-glitch-text">
                  {getNoteLabel(i)}
                </option>
              ))}
            </select>
          </div>

          {/* CHOKE */}
          <div className="flex items-center gap-1.5 bg-glitch-dark px-2.5 py-1 rounded border border-glitch-border text-xs">
            <span className="text-[10px] text-glitch-dim">CHOKE:</span>
            <select
              value={source.chokeGroup}
              onChange={(e) => onUpdate('chokeGroup', parseInt(e.target.value))}
              className="bg-transparent text-glitch-pink font-bold outline-none cursor-pointer"
            >
              <option value={0} className="bg-glitch-panel text-glitch-text">OFF</option>
              {[1, 2, 3, 4, 5, 6, 7, 8].map((g) => (
                <option key={g} value={g} className="bg-glitch-panel text-glitch-text">
                  GROUP {g}
                </option>
              ))}
            </select>
          </div>
        </div>

        {/* DAW MULTI-OUT BUS ROUTING */}
        <div className="flex items-center gap-2 bg-glitch-dark px-3 py-1 rounded border border-emerald-500/40 text-xs">
          <Share2 className="w-3.5 h-3.5 text-emerald-400" />
          <span className="text-[10px] text-glitch-dim font-bold tracking-wider">AUDIO OUT:</span>
          <select
            value={source.outputBus ?? 0}
            onChange={(e) => onUpdate('outputBus', parseInt(e.target.value))}
            className="bg-transparent text-emerald-400 font-bold outline-none cursor-pointer"
          >
            <option value={0} className="bg-glitch-panel text-glitch-text">MAIN OUT (CH 1-2)</option>
            {Array.from({ length: 15 }, (_, i) => {
              const busIdx = i + 1;
              const chL = busIdx * 2 + 1;
              const chR = busIdx * 2 + 2;
              return (
                <option key={busIdx} value={busIdx} className="bg-glitch-panel text-glitch-text">
                  OUT {busIdx + 1} (CH {chL}-{chR})
                </option>
              );
            })}
          </select>
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

          {/* Frequency & Pitch Track Controls */}
          <div className="grid grid-cols-2 gap-3 text-xs pt-1">
            <div className="bg-glitch-dark/70 p-2.5 rounded border border-glitch-border flex flex-col gap-1.5">
              <div className="flex justify-between text-[10px] items-center">
                <span className="text-glitch-dim">NOTE FREQUENCY (Hz):</span>
                <div className="flex items-center gap-1">
                  <input
                    type="number"
                    min="10"
                    max="20000"
                    step="0.1"
                    value={source.frequency !== undefined ? source.frequency : 440}
                    onChange={(e) => onUpdate('frequency', parseFloat(e.target.value) || 440)}
                    className="w-16 bg-glitch-dark text-glitch-cyan font-bold text-right px-1 py-0.5 rounded border border-glitch-border outline-none focus:border-glitch-cyan text-[11px]"
                  />
                  <span className="text-glitch-cyan font-bold text-[10px]">Hz</span>
                </div>
              </div>
              <input
                type="range"
                min="20"
                max="5000"
                step="1"
                value={source.frequency !== undefined ? source.frequency : 440}
                onChange={(e) => onUpdate('frequency', parseFloat(e.target.value))}
                className="w-full h-1 accent-glitch-cyan cursor-pointer"
              />
              <div className="flex justify-between text-[9px] text-glitch-dim">
                <span>Sub 30Hz</span>
                <span>Bass 120Hz</span>
                <span>A4 440Hz</span>
                <span>Lead 2kHz</span>
              </div>
            </div>

            <div className="bg-glitch-dark/70 p-2.5 rounded border border-glitch-border flex flex-col justify-between">
              <div className="flex justify-between items-center text-[10px]">
                <span className="text-glitch-dim">KEY TRACKING MODE:</span>
                <span className={`font-bold text-[9px] px-1.5 py-0.5 rounded border ${
                  source.pitchTrack !== false
                    ? 'border-glitch-cyan text-glitch-cyan bg-glitch-cyan/10'
                    : 'border-glitch-amber text-glitch-amber bg-glitch-amber/10'
                }`}>
                  {source.pitchTrack !== false ? 'TRACKING (A4=REF)' : 'FIXED FREQUENCY'}
                </span>
              </div>
              <p className="text-[10px] text-glitch-dim leading-tight">
                {source.pitchTrack !== false
                  ? 'Tracks MIDI notes with base frequency as reference tuning.'
                  : 'Generates exact manual frequency regardless of MIDI note.'}
              </p>
              <button
                onClick={() => onUpdate('pitchTrack', source.pitchTrack === false ? true : false)}
                className={`py-1 px-2 rounded text-[10px] font-bold border transition-all ${
                  source.pitchTrack !== false
                    ? 'bg-glitch-surface text-glitch-cyan border-glitch-cyan/60 hover:bg-glitch-panel'
                    : 'bg-glitch-amber text-glitch-dark border-glitch-amber shadow-neon-amber'
                }`}
              >
                {source.pitchTrack !== false ? 'ENABLE MANUAL FIXED FREQ' : 'MANUAL FREQ ACTIVE (FIXED)'}
              </button>
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
        <div className="bg-glitch-panel/40 border border-emerald-500/40 rounded p-3 flex flex-col gap-2.5 shadow-sm">
          <div className="flex items-center justify-between">
            <span className="text-xs font-bold text-emerald-400 tracking-wider flex items-center gap-1.5">
              <Zap className="w-3.5 h-3.5" />
              1-SAMPLE UNIT IMPULSE GENERATOR (DIRAC DELTA)
            </span>
            <span className="text-[9px] px-1.5 py-0.5 rounded bg-emerald-500/20 text-emerald-300 font-bold border border-emerald-500/30">
              FAST-PATTERN OPTIMIZED
            </span>
          </div>

          <div className="bg-glitch-dark/70 p-3 rounded border border-glitch-border flex flex-col gap-2.5">
            <div className="flex items-center justify-between">
              <div className="flex flex-col">
                <span className="text-[11px] font-bold text-glitch-text">IMPULSE POLARITY</span>
                <span className="text-[9px] text-glitch-dim">
                  Unit pulse direction: +1.0 (Positive needle) or -1.0 (Negative needle)
                </span>
              </div>
              <div className="flex gap-1.5">
                {[
                  { label: '+ POSITIVE (+1.0)', val: 0 },
                  { label: '- NEGATIVE (-1.0)', val: 1 },
                  { label: '± BIPOLAR', val: 2 }
                ].map((item) => (
                  <button
                    key={item.val}
                    onClick={() => onUpdate('clickPolarity', item.val)}
                    className={`py-1 px-3 rounded text-[10px] font-bold border transition-all ${
                      (source.clickPolarity ?? 0) === item.val
                        ? 'bg-emerald-500 text-glitch-dark border-emerald-500 shadow-neon-green'
                        : 'bg-glitch-surface text-glitch-dim border-glitch-border hover:text-glitch-text'
                    }`}
                  >
                    {item.label}
                  </button>
                ))}
              </div>
            </div>

            <div className="p-2 rounded bg-emerald-950/20 border border-emerald-800/30 text-[10px] text-emerald-300/80 flex items-center gap-2">
              <span className="font-mono text-emerald-400 font-bold">δ[n]=1.0</span>
              <span>
                Pure 1-sample unit impulse. Free of envelope decay latency or voice stealing delays; renders sample-accurately on ultra-fast patterns, ratchet rolls, and micro-glitches.
              </span>
            </div>
          </div>
        </div>
      )}

      {/* AHDSR Fast Envelope Section (Only for non-Click sources) */}
      {source.type !== 'Click' && (
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
      )}

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
