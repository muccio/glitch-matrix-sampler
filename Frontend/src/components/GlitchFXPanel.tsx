import React from 'react';
import { GlitchFXData } from '../types/matrix';
import { Zap, Radio, Sliders } from 'lucide-react';

interface GlitchFXPanelProps {
  fx: GlitchFXData;
  onChange: (paramId: string, value: any) => void;
}

const DIVISIONS = ['1/4', '1/8', '1/16', '1/32', '1/64', '1/128', '1/8T', '1/16T'];

export const GlitchFXPanel: React.FC<GlitchFXPanelProps> = ({ fx, onChange }) => {
  return (
    <div className="bg-glitch-panel/50 border border-glitch-border rounded p-3 flex flex-col gap-2.5">
      <div className="flex items-center justify-between text-xs">
        <span className="font-bold text-glitch-pink tracking-wider flex items-center gap-1.5">
          <Zap className="w-3.5 h-3.5 text-glitch-pink animate-bounce" />
          GLITCH MULTI-FX PROCESSOR
        </span>
        <span className="text-[10px] text-glitch-dim">PER-SOURCE HARD CUT & DEGRADATION</span>
      </div>

      <div className="grid grid-cols-3 gap-3">
        {/* 1. BITCRUSHER */}
        <div className="bg-glitch-surface/90 border border-glitch-border rounded p-2.5 flex flex-col gap-2">
          <div className="flex items-center justify-between text-[11px]">
            <span className="font-bold text-glitch-cyan flex items-center gap-1">
              <Sliders className="w-3 h-3 text-glitch-cyan" />
              BITCRUSHER
            </span>
            <span className="text-[10px] font-bold text-glitch-cyan">
              {fx.bitDepth.toFixed(1)} BITS
            </span>
          </div>

          <div className="flex flex-col gap-1 text-[10px]">
            <div className="flex justify-between text-glitch-dim">
              <span>QUANTIZE:</span>
              <span className="text-glitch-text">{fx.bitDepth.toFixed(0)} bits</span>
            </div>
            <input
              type="range"
              min="1"
              max="16"
              step="0.5"
              value={fx.bitDepth}
              onChange={(e) => onChange('bitDepth', parseFloat(e.target.value))}
              className="w-full h-1 accent-glitch-cyan cursor-pointer"
            />
          </div>

          <div className="flex flex-col gap-1 text-[10px]">
            <div className="flex justify-between text-glitch-dim">
              <span>MIX:</span>
              <span className="text-glitch-text">{(fx.bitcrushMix * 100).toFixed(0)}%</span>
            </div>
            <input
              type="range"
              min="0"
              max="1"
              step="0.01"
              value={fx.bitcrushMix}
              onChange={(e) => onChange('bitcrushMix', parseFloat(e.target.value))}
              className="w-full h-1 accent-glitch-cyan cursor-pointer"
            />
          </div>
        </div>

        {/* 2. SAMPLE RATE REDUCER */}
        <div className="bg-glitch-surface/90 border border-glitch-border rounded p-2.5 flex flex-col gap-2">
          <div className="flex items-center justify-between text-[11px]">
            <span className="font-bold text-glitch-amber flex items-center gap-1">
              <Radio className="w-3 h-3 text-glitch-amber" />
              RATE REDUCER
            </span>
            <span className="text-[10px] font-bold text-glitch-amber">
              {fx.downsampleHz < 1000
                ? `${Math.round(fx.downsampleHz)} Hz`
                : `${(fx.downsampleHz / 1000).toFixed(1)} kHz`}
            </span>
          </div>

          <div className="flex flex-col gap-1 text-[10px]">
            <div className="flex justify-between text-glitch-dim">
              <span>DOWNSAMPLE:</span>
              <span className="text-glitch-text">{Math.round(fx.downsampleHz)} Hz</span>
            </div>
            <input
              type="range"
              min="50"
              max="44100"
              step="50"
              value={fx.downsampleHz}
              onChange={(e) => onChange('downsampleHz', parseFloat(e.target.value))}
              className="w-full h-1 accent-glitch-amber cursor-pointer"
            />
          </div>

          <div className="flex flex-col gap-1 text-[10px]">
            <div className="flex justify-between text-glitch-dim">
              <span>MIX:</span>
              <span className="text-glitch-text">{(fx.downsampleMix * 100).toFixed(0)}%</span>
            </div>
            <input
              type="range"
              min="0"
              max="1"
              step="0.01"
              value={fx.downsampleMix}
              onChange={(e) => onChange('downsampleMix', parseFloat(e.target.value))}
              className="w-full h-1 accent-glitch-amber cursor-pointer"
            />
          </div>
        </div>

        {/* 3. STUTTER / MICRO-GATER */}
        <div className="bg-glitch-surface/90 border border-glitch-border rounded p-2.5 flex flex-col gap-2">
          <div className="flex items-center justify-between text-[11px]">
            <span className="font-bold text-glitch-pink flex items-center gap-1">
              <Zap className="w-3 h-3 text-glitch-pink" />
              STUTTER GATER
            </span>
            <button
              onClick={() => onChange('stutterSync', !fx.stutterSync)}
              className={`px-1.5 py-0.5 rounded text-[9px] font-mono border ${
                fx.stutterSync
                  ? 'bg-glitch-pink text-glitch-dark border-glitch-pink font-bold'
                  : 'bg-glitch-panel text-glitch-dim border-glitch-border'
              }`}
            >
              {fx.stutterSync ? 'SYNC' : 'FREE HZ'}
            </button>
          </div>

          {fx.stutterSync ? (
            <div className="flex flex-col gap-1 text-[10px]">
              <div className="flex justify-between text-glitch-dim">
                <span>DIVISION:</span>
                <span className="text-glitch-pink font-bold">{DIVISIONS[fx.stutterDivision]}</span>
              </div>
              <select
                value={fx.stutterDivision}
                onChange={(e) => onChange('stutterDivision', parseInt(e.target.value))}
                className="bg-glitch-dark text-glitch-text border border-glitch-border rounded px-1.5 py-1 text-[10px]"
              >
                {DIVISIONS.map((div, i) => (
                  <option key={i} value={i}>
                    {div}
                  </option>
                ))}
              </select>
            </div>
          ) : (
            <div className="flex flex-col gap-1 text-[10px]">
              <div className="flex justify-between text-glitch-dim">
                <span>RATE:</span>
                <span className="text-glitch-pink font-bold">{fx.stutterHz.toFixed(1)} Hz</span>
              </div>
              <input
                type="range"
                min="0.5"
                max="100"
                step="0.5"
                value={fx.stutterHz}
                onChange={(e) => onChange('stutterHz', parseFloat(e.target.value))}
                className="w-full h-1 accent-glitch-pink cursor-pointer"
              />
            </div>
          )}

          <div className="grid grid-cols-2 gap-2 text-[10px]">
            <div className="flex flex-col gap-0.5">
              <span className="text-glitch-dim">DUTY: {(fx.stutterDuty * 100).toFixed(0)}%</span>
              <input
                type="range"
                min="0.05"
                max="0.95"
                step="0.05"
                value={fx.stutterDuty}
                onChange={(e) => onChange('stutterDuty', parseFloat(e.target.value))}
                className="w-full h-1 accent-glitch-pink cursor-pointer"
              />
            </div>
            <div className="flex flex-col gap-0.5">
              <span className="text-glitch-dim">MIX: {(fx.stutterMix * 100).toFixed(0)}%</span>
              <input
                type="range"
                min="0"
                max="1"
                step="0.01"
                value={fx.stutterMix}
                onChange={(e) => onChange('stutterMix', parseFloat(e.target.value))}
                className="w-full h-1 accent-glitch-pink cursor-pointer"
              />
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
