import React from 'react';
import { SoundSourceData, SourceType } from '../types/matrix';
import { SourceCard } from './SourceCard';
import { Plus, Cpu, Radio, Disc } from 'lucide-react';
import { NativeBridge } from '../services/NativeBridge';

interface SourceListProps {
  sources: SoundSourceData[];
  selectedSourceId: number | null;
  onSelectSource: (id: number) => void;
  onUpdateSource: (sourceId: number, paramId: string, value: any) => void;
}

export const SourceList: React.FC<SourceListProps> = ({
  sources,
  selectedSourceId,
  onSelectSource,
  onUpdateSource
}) => {
  const handleAdd = (type: SourceType) => {
    NativeBridge.addSource(type);
  };

  return (
    <div className="flex flex-col h-full bg-glitch-surface/60 border-r border-glitch-border w-80 min-w-[300px] select-none">
      {/* List Header & Add Actions */}
      <div className="p-3 border-b border-glitch-border bg-glitch-panel/40 flex flex-col gap-2">
        <div className="flex items-center justify-between">
          <span className="text-xs font-bold text-glitch-text tracking-wider uppercase flex items-center gap-1.5">
            <span className="w-1.5 h-1.5 rounded-sm bg-glitch-cyan" />
            SOUND SOURCES ({sources.length})
          </span>
          <span className="text-[10px] text-glitch-dim">UNLIMITED DYNAMIC</span>
        </div>

        {/* 3 Quick Add Buttons */}
        <div className="grid grid-cols-3 gap-1.5">
          <button
            onClick={() => handleAdd('Oscillator')}
            className="flex items-center justify-center gap-1 py-1.5 px-2 rounded bg-glitch-cyan/10 hover:bg-glitch-cyan/20 border border-glitch-cyan/40 hover:border-glitch-cyan text-glitch-cyan text-[10px] font-bold transition-all shadow-sm"
          >
            <Plus className="w-3 h-3" />
            <Cpu className="w-3 h-3" />
            OSC
          </button>

          <button
            onClick={() => handleAdd('Noise')}
            className="flex items-center justify-center gap-1 py-1.5 px-2 rounded bg-glitch-amber/10 hover:bg-glitch-amber/20 border border-glitch-amber/40 hover:border-glitch-amber text-glitch-amber text-[10px] font-bold transition-all shadow-sm"
          >
            <Plus className="w-3 h-3" />
            <Radio className="w-3 h-3" />
            NOISE
          </button>

          <button
            onClick={() => handleAdd('Sample')}
            className="flex items-center justify-center gap-1 py-1.5 px-2 rounded bg-glitch-pink/10 hover:bg-glitch-pink/20 border border-glitch-pink/40 hover:border-glitch-pink text-glitch-pink text-[10px] font-bold transition-all shadow-sm"
          >
            <Plus className="w-3 h-3" />
            <Disc className="w-3 h-3" />
            SMPL
          </button>
        </div>
      </div>

      {/* Scrollable list of source cards */}
      <div className="flex-1 overflow-y-auto p-3 flex flex-col gap-2.5 custom-scrollbar">
        {sources.length === 0 ? (
          <div className="h-48 border border-dashed border-glitch-border rounded flex flex-col items-center justify-center text-glitch-dim text-xs gap-2 p-4 text-center">
            <span>NO ACTIVE SOURCES</span>
            <span className="text-[10px]">Click OSC, NOISE, or SMPL above to create your first glitch generator.</span>
          </div>
        ) : (
          sources.map((src) => (
            <SourceCard
              key={src.id}
              source={src}
              isSelected={selectedSourceId === src.id}
              onSelect={() => onSelectSource(src.id)}
              onUpdate={(paramId, val) => onUpdateSource(src.id, paramId, val)}
              onClone={() => NativeBridge.cloneSource(src.id)}
              onDelete={() => NativeBridge.removeSource(src.id)}
            />
          ))
        )}
      </div>
    </div>
  );
};
