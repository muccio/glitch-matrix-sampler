import React from 'react';
import { Volume2, AlertOctagon, Activity, Layers, PlaySquare, Save, Dices } from 'lucide-react';
import { NativeBridge } from '../services/NativeBridge';
import { VoiceStats } from '../types/matrix';

interface GlobalBarProps {
  masterVolume: number;
  stats: VoiceStats;
  sourceCount: number;
  onMasterVolumeChange: (vol: number) => void;
}

const PRESETS = [
  "01. Init Glitch Sine",
  "02. Clicks & Cuts IDM",
  "03. Cyberpunk Glitch Bass",
  "04. Micro-Stutter Machine",
  "05. Digital Entropy"
];

export const GlobalBar: React.FC<GlobalBarProps> = ({
  masterVolume,
  stats,
  sourceCount,
  onMasterVolumeChange
}) => {
  return (
    <header className="h-14 bg-glitch-surface border-b border-glitch-border px-4 flex items-center justify-between select-none">
      {/* Brand & Logo */}
      <div className="flex items-center gap-3">
        <div className="flex items-center gap-1.5 bg-glitch-panel px-2.5 py-1 rounded border border-glitch-cyan/40">
          <Activity className="w-4 h-4 text-glitch-cyan animate-pulse" />
          <span className="text-sm font-bold tracking-wider text-glitch-cyan drop-shadow-[0_0_8px_rgba(0,240,255,0.6)]">
            GLITCH_MATRIX
          </span>
          <span className="text-[10px] text-glitch-dim bg-glitch-dark px-1 py-0.5 rounded font-mono border border-glitch-border">
            v1.0.0
          </span>
        </div>

        {/* Live Voice & Source telemetry */}
        <div className="flex items-center gap-3 text-xs ml-2">
          <div className="flex items-center gap-1.5 bg-glitch-panel/70 px-2 py-1 rounded border border-glitch-border">
            <Layers className="w-3.5 h-3.5 text-glitch-amber" />
            <span className="text-glitch-dim">SOURCES:</span>
            <span className="font-bold text-glitch-amber">{sourceCount}</span>
          </div>

          <div className="flex items-center gap-1.5 bg-glitch-panel/70 px-2 py-1 rounded border border-glitch-border">
            <PlaySquare className="w-3.5 h-3.5 text-glitch-green" />
            <span className="text-glitch-dim">ACTIVE VOICES:</span>
            <span className="font-bold text-glitch-green">{stats.activeVoices}</span>
          </div>
        </div>
      </div>

      {/* Preset & Randomize Controls */}
      <div className="flex items-center gap-3">
        {/* Randomize Glitch Set Button */}
        <button
          onClick={() => NativeBridge.randomizeSet()}
          title="Randomize Glitch Set - Generate a randomized set of clicks, textures & glitch sources"
          className="flex items-center gap-1.5 px-3 py-1.5 rounded bg-glitch-cyan/15 border border-glitch-cyan/50 hover:bg-glitch-cyan hover:text-glitch-dark text-glitch-cyan text-xs font-bold transition-all shadow-sm hover:shadow-[0_0_12px_rgba(0,240,255,0.4)] active:scale-95 group"
        >
          <Dices className="w-4 h-4 transition-transform duration-500 group-hover:rotate-180 text-glitch-cyan group-hover:text-glitch-dark" />
          <span className="tracking-wider">RANDOM SET</span>
        </button>

        <div className="h-4 w-[1px] bg-glitch-border" />

        {/* Preset Selector */}
        <div className="flex items-center gap-2">
          <span className="text-xs text-glitch-dim uppercase">Preset:</span>
          <select
            onChange={(e) => NativeBridge.loadPreset(parseInt(e.target.value))}
            className="bg-glitch-panel text-glitch-text text-xs border border-glitch-border rounded px-2 py-1.5 focus:border-glitch-cyan outline-none cursor-pointer hover:border-glitch-cyan/60"
          >
            {PRESETS.map((name, idx) => (
              <option key={idx} value={idx}>
                {name}
              </option>
            ))}
          </select>
          <button
            onClick={() => NativeBridge.savePreset("")}
            title="Save Preset JSON"
            className="p-1.5 rounded bg-glitch-panel hover:bg-glitch-card text-glitch-dim hover:text-glitch-cyan border border-glitch-border"
          >
            <Save className="w-3.5 h-3.5" />
          </button>
        </div>
      </div>

      {/* Output Meter & Master Fader & Panic */}
      <div className="flex items-center gap-4">
        {/* Stereo Peak VU Meter */}
        <div className="flex items-center gap-1 bg-glitch-panel px-2 py-1 rounded border border-glitch-border">
          <div className="flex flex-col gap-0.5">
            <div className="w-12 h-1.5 bg-glitch-dark rounded-sm overflow-hidden flex">
              <div
                className="h-full bg-gradient-to-r from-glitch-green via-glitch-amber to-glitch-pink transition-all duration-75"
                style={{ width: `${Math.min(100, stats.peakL * 100)}%` }}
              />
            </div>
            <div className="w-12 h-1.5 bg-glitch-dark rounded-sm overflow-hidden flex">
              <div
                className="h-full bg-gradient-to-r from-glitch-green via-glitch-amber to-glitch-pink transition-all duration-75"
                style={{ width: `${Math.min(100, stats.peakR * 100)}%` }}
              />
            </div>
          </div>
          <span className="text-[10px] text-glitch-dim w-6 text-right">
            {(stats.peakL > 0.001 ? (20 * Math.log10(stats.peakL)).toFixed(0) : "-inf")}
          </span>
        </div>

        {/* Master Volume Slider */}
        <div className="flex items-center gap-2">
          <Volume2 className="w-4 h-4 text-glitch-cyan" />
          <input
            type="range"
            min="0"
            max="1.5"
            step="0.01"
            value={masterVolume}
            onChange={(e) => onMasterVolumeChange(parseFloat(e.target.value))}
            className="w-24 h-1.5 bg-glitch-panel rounded appearance-none cursor-pointer accent-glitch-cyan"
          />
          <span className="text-xs text-glitch-cyan w-10 font-bold">
            {(masterVolume * 100).toFixed(0)}%
          </span>
        </div>

        {/* Panic Button */}
        <button
          onClick={() => NativeBridge.panic()}
          className="flex items-center gap-1.5 px-3 py-1.5 rounded bg-glitch-pink/10 border border-glitch-pink/40 hover:bg-glitch-pink hover:text-glitch-dark text-glitch-pink text-xs font-bold transition-all shadow-sm hover:shadow-neon-pink"
        >
          <AlertOctagon className="w-3.5 h-3.5" />
          PANIC
        </button>
      </div>
    </header>
  );
};
