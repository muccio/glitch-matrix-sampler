import React from 'react';
import { SoundSourceData } from '../types/matrix';
import { VolumeX, Headphones, Copy, Trash2, Music, Cpu, Radio, Disc } from 'lucide-react';

interface SourceCardProps {
  source: SoundSourceData;
  isSelected: boolean;
  onSelect: () => void;
  onUpdate: (paramId: string, value: any) => void;
  onClone: () => void;
  onDelete: () => void;
}

const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

function getNoteLabel(note: number): string {
  if (note < 0) return 'ALL';
  const octave = Math.floor(note / 12) - 1;
  const noteIndex = note % 12;
  return `${NOTE_NAMES[noteIndex]}${octave} (${note})`;
}

export const SourceCard: React.FC<SourceCardProps> = ({
  source,
  isSelected,
  onSelect,
  onUpdate,
  onClone,
  onDelete
}) => {
  const getTypeIcon = () => {
    switch (source.type) {
      case 'Oscillator':
        return <Cpu className="w-3.5 h-3.5 text-glitch-cyan" />;
      case 'Noise':
        return <Radio className="w-3.5 h-3.5 text-glitch-amber" />;
      case 'Sample':
        return <Disc className="w-3.5 h-3.5 text-glitch-pink" />;
    }
  };

  const getTypeBadgeClass = () => {
    switch (source.type) {
      case 'Oscillator':
        return 'text-glitch-cyan border-glitch-cyan/30 bg-glitch-cyan/10';
      case 'Noise':
        return 'text-glitch-amber border-glitch-amber/30 bg-glitch-amber/10';
      case 'Sample':
        return 'text-glitch-pink border-glitch-pink/30 bg-glitch-pink/10';
    }
  };

  return (
    <div
      onClick={onSelect}
      className={`p-3 rounded border transition-all cursor-pointer select-none flex flex-col gap-2 ${
        isSelected
          ? 'bg-glitch-panel border-glitch-cyan shadow-neon-cyan'
          : 'bg-glitch-surface hover:bg-glitch-card border-glitch-border'
      }`}
    >
      {/* Top Header: Type, LED, Name, Actions */}
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          {/* Active LED */}
          <span
            className={`w-2 h-2 rounded-full ${
              source.muted
                ? 'bg-glitch-dim'
                : 'bg-glitch-green shadow-neon-green animate-pulse'
            }`}
          />

          <span
            className={`text-[10px] font-bold px-1.5 py-0.5 rounded border flex items-center gap-1 ${getTypeBadgeClass()}`}
          >
            {getTypeIcon()}
            {source.type.toUpperCase()}
          </span>

          <span className="font-bold text-xs text-glitch-text truncate max-w-[140px]">
            {source.name}
          </span>
        </div>

        {/* Quick Toolbar: Mute, Solo, Clone, Delete */}
        <div className="flex items-center gap-1" onClick={(e) => e.stopPropagation()}>
          <button
            onClick={() => onUpdate('muted', !source.muted)}
            title="Mute"
            className={`p-1 rounded text-xs border ${
              source.muted
                ? 'bg-glitch-pink text-glitch-dark border-glitch-pink font-bold'
                : 'bg-glitch-dark text-glitch-dim border-glitch-border hover:text-glitch-pink'
            }`}
          >
            <VolumeX className="w-3 h-3" />
          </button>

          <button
            onClick={() => onUpdate('soloed', !source.soloed)}
            title="Solo"
            className={`p-1 rounded text-xs border ${
              source.soloed
                ? 'bg-glitch-amber text-glitch-dark border-glitch-amber font-bold'
                : 'bg-glitch-dark text-glitch-dim border-glitch-border hover:text-glitch-amber'
            }`}
          >
            <Headphones className="w-3 h-3" />
          </button>

          <button
            onClick={onClone}
            title="Clone Source"
            className="p-1 rounded text-xs bg-glitch-dark text-glitch-dim border border-glitch-border hover:text-glitch-cyan"
          >
            <Copy className="w-3 h-3" />
          </button>

          <button
            onClick={onDelete}
            title="Delete Source"
            className="p-1 rounded text-xs bg-glitch-dark text-glitch-dim border border-glitch-border hover:text-glitch-pink"
          >
            <Trash2 className="w-3 h-3" />
          </button>
        </div>
      </div>

      {/* Middle Row: Assigned MIDI Note & Choke Group */}
      <div
        className="grid grid-cols-2 gap-2 text-[10px]"
        onClick={(e) => e.stopPropagation()}
      >
        {/* MIDI Note selector */}
        <div className="bg-glitch-dark/70 px-2 py-1 rounded border border-glitch-border/60 flex items-center justify-between">
          <span className="text-glitch-dim flex items-center gap-1">
            <Music className="w-3 h-3 text-glitch-cyan" />
            NOTE:
          </span>
          <select
            value={source.assignedNote}
            onChange={(e) => onUpdate('assignedNote', parseInt(e.target.value))}
            className="bg-transparent text-glitch-cyan font-bold outline-none cursor-pointer text-right"
          >
            <option value={-1}>OMNI (ALL)</option>
            {Array.from({ length: 128 }, (_, i) => (
              <option key={i} value={i} className="bg-glitch-panel text-glitch-text">
                {getNoteLabel(i)}
              </option>
            ))}
          </select>
        </div>

        {/* Choke Group Selector */}
        <div className="bg-glitch-dark/70 px-2 py-1 rounded border border-glitch-border/60 flex items-center justify-between">
          <span className="text-glitch-dim">CHOKE:</span>
          <select
            value={source.chokeGroup}
            onChange={(e) => onUpdate('chokeGroup', parseInt(e.target.value))}
            className="bg-transparent text-glitch-pink font-bold outline-none cursor-pointer text-right"
          >
            <option value={0} className="bg-glitch-panel text-glitch-text">OFF</option>
            {[1, 2, 3, 4, 5, 6, 7, 8].map((g) => (
              <option key={g} value={g} className="bg-glitch-panel text-glitch-text">
                GRP {g}
              </option>
            ))}
          </select>
        </div>
      </div>

      {/* Mini Gain / Pan indicators */}
      <div className="flex items-center justify-between text-[9px] text-glitch-dim px-0.5">
        <span>GAIN: {(source.gain * 100).toFixed(0)}%</span>
        <span>PAN: {source.pan < 0 ? `L${Math.abs(source.pan * 100).toFixed(0)}` : source.pan > 0 ? `R${(source.pan * 100).toFixed(0)}` : 'C'}</span>
        <span>A: {source.attackMs.toFixed(1)}ms</span>
        <span>D: {source.decayMs.toFixed(0)}ms</span>
      </div>
    </div>
  );
};
