import React, { useRef, useState, useEffect } from 'react';
import { EnvelopeData } from '../types/matrix';

interface EnvelopeEditorProps {
  envelope: EnvelopeData;
  onChange: (paramId: string, value: number) => void;
}

export const EnvelopeEditor: React.FC<EnvelopeEditorProps> = ({ envelope, onChange }) => {
  const svgRef = useRef<SVGSVGElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);

  const [editingParam, setEditingParam] = useState<string | null>(null);
  const [editValue, setEditValue] = useState<string>('');

  const { attackMs, holdMs, decayMs, sustain, releaseMs, curve } = envelope;

  useEffect(() => {
    if (editingParam && inputRef.current) {
      inputRef.current.focus();
      inputRef.current.select();
    }
  }, [editingParam]);

  const startEdit = (paramId: string, initialVal: number | string) => {
    setEditingParam(paramId);
    setEditValue(String(initialVal));
  };

  const cancelEdit = () => {
    setEditingParam(null);
  };

  const commitEdit = (paramId: string) => {
    if (!editingParam) return;
    const cleanStr = editValue.trim().toLowerCase().replace('ms', '').replace('%', '');
    let parsed = parseFloat(cleanStr);

    if (paramId === 'curve') {
      const upper = editValue.trim().toUpperCase();
      if (upper === 'EXP') parsed = -0.6;
      else if (upper === 'LIN') parsed = 0.0;
      else if (upper === 'LOG') parsed = 0.6;
    }

    if (!isNaN(parsed)) {
      let finalVal = parsed;
      if (paramId === 'attackMs') finalVal = Math.max(0.05, Math.min(5000, parsed));
      else if (paramId === 'holdMs') finalVal = Math.max(0, Math.min(2000, parsed));
      else if (paramId === 'decayMs') finalVal = Math.max(0.1, Math.min(10000, parsed));
      else if (paramId === 'sustain') {
        if (parsed > 1.0 || editValue.includes('%')) finalVal = parsed / 100.0;
        finalVal = Math.max(0, Math.min(1, finalVal));
      }
      else if (paramId === 'releaseMs') finalVal = Math.max(0.1, Math.min(10000, parsed));
      else if (paramId === 'curve') finalVal = Math.max(-1.0, Math.min(1.0, parsed));

      onChange(paramId, finalVal);
    }
    setEditingParam(null);
  };

  // Coordinate mapping
  // Normalized visual time proportions: Attack (20%), Hold (15%), Decay (25%), Sustain (20%), Release (20%)
  const width = 360;
  const height = 140;
  const padding = 15;
  const drawWidth = width - padding * 2;
  const drawHeight = height - padding * 2;

  // Maximum time scales for visualization
  const normA = Math.min(1.0, Math.log10(attackMs + 0.1) / Math.log10(5000)) * 0.25;
  const normH = Math.min(1.0, holdMs / 1000.0) * 0.15;
  const normD = Math.min(1.0, Math.log10(decayMs + 0.1) / Math.log10(5000)) * 0.3;
  const normR = Math.min(1.0, Math.log10(releaseMs + 0.1) / Math.log10(5000)) * 0.3;

  const totalNorm = normA + normH + normD + 0.2 + normR;
  const scale = drawWidth / totalNorm;

  const x0 = padding;
  const y0 = height - padding;

  // Attack peak
  const xA = x0 + Math.max(10, normA * scale);
  const yA = padding;

  // Hold end
  const xH = xA + normH * scale;
  const yH = padding;

  // Decay / Sustain point
  const xD = xH + Math.max(15, normD * scale);
  const yD = y0 - sustain * drawHeight;

  // Sustain end (fixed visual width)
  const xS = xD + 0.2 * scale;
  const yS = yD;

  // Release end
  const xR = xS + Math.max(15, normR * scale);
  const yR = y0;

  // Construct SVG Path with curve shaping
  const pathD = `M ${x0} ${y0} 
    Q ${x0 + (xA - x0) * (0.5 - curve * 0.3)} ${y0 - (y0 - yA) * (0.5 + curve * 0.3)}, ${xA} ${yA}
    L ${xH} ${yH}
    Q ${xH + (xD - xH) * (0.5 + curve * 0.3)} ${yH + (yD - yH) * (0.5 - curve * 0.3)}, ${xD} ${yD}
    L ${xS} ${yS}
    Q ${xS + (xR - xS) * (0.5 + curve * 0.3)} ${yS + (yR - yS) * (0.5 - curve * 0.3)}, ${xR} ${yR}`;

  const areaPathD = `${pathD} L ${xR} ${y0} L ${x0} ${y0} Z`;

  return (
    <div className="bg-glitch-panel/50 border border-glitch-border rounded p-3 flex flex-col gap-2">
      <div className="flex items-center justify-between text-xs">
        <span className="font-bold text-glitch-cyan tracking-wider flex items-center gap-1.5">
          <span className="w-1.5 h-1.5 rounded-full bg-glitch-cyan shadow-neon-cyan animate-pulse" />
          FAST AHDSR ENVELOPE (SUB-MS CLICK READY)
        </span>
        <div className="flex items-center gap-2">
          <span className="text-[9px] text-glitch-cyan/60 hidden sm:inline">
            (Doppio click per inserire i valori)
          </span>
          <span className="text-[10px] text-glitch-dim">
            Attack: {attackMs < 1 ? `${(attackMs).toFixed(2)}ms` : `${Math.round(attackMs)}ms`} | Decay: {Math.round(decayMs)}ms
          </span>
        </div>
      </div>

      {/* SVG Canvas visualizer */}
      <div className="relative bg-glitch-dark/80 rounded border border-glitch-border/60 overflow-hidden flex items-center justify-center">
        {/* Subtle grid lines */}
        <div className="absolute inset-0 bg-[linear-gradient(to_right,#1b2330_1px,transparent_1px),linear-gradient(to_bottom,#1b2330_1px,transparent_1px)] bg-[size:20px_20px] opacity-40 pointer-events-none" />

        <svg
          ref={svgRef}
          viewBox={`0 0 ${width} ${height}`}
          className="w-full h-32 select-none"
        >
          {/* Filled gradient area */}
          <defs>
            <linearGradient id="envGradient" x1="0" y1="0" x2="0" y2="1">
              <stop offset="0%" stopColor="#00f0ff" stopOpacity="0.45" />
              <stop offset="100%" stopColor="#00f0ff" stopOpacity="0.02" />
            </linearGradient>
          </defs>
          <path d={areaPathD} fill="url(#envGradient)" />

          {/* Stroke outline */}
          <path d={pathD} fill="none" stroke="#00f0ff" strokeWidth="2" strokeLinecap="round" />

          {/* Draggable control points */}
          <circle cx={xA} cy={yA} r="5" fill="#00f0ff" className="cursor-ew-resize hover:scale-125 transition-transform" />
          <circle cx={xH} cy={yH} r="4" fill="#ffaa00" className="cursor-ew-resize hover:scale-125 transition-transform" />
          <circle cx={xD} cy={yD} r="5" fill="#00f0ff" className="cursor-move hover:scale-125 transition-transform" />
          <circle cx={xS} cy={yS} r="4" fill="#ff0055" className="cursor-ns-resize hover:scale-125 transition-transform" />
          <circle cx={xR} cy={yR} r="5" fill="#00f0ff" className="cursor-ew-resize hover:scale-125 transition-transform" />
        </svg>
      </div>

      {/* Numeric Parameter Dials / Sliders with Double-Click Text Input */}
      <div className="grid grid-cols-6 gap-2 text-center text-[10px]">
        {/* ATTACK */}
        <div
          className="bg-glitch-surface p-1.5 rounded border border-glitch-border flex flex-col items-center group hover:border-glitch-cyan/60 transition-colors"
          title="Doppio click per inserire il valore manualmente"
        >
          <span className="text-glitch-dim select-none">ATTACK</span>
          {editingParam === 'attackMs' ? (
            <input
              ref={inputRef}
              type="text"
              value={editValue}
              onChange={(e) => setEditValue(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter') commitEdit('attackMs');
                else if (e.key === 'Escape') cancelEdit();
              }}
              onBlur={() => commitEdit('attackMs')}
              className="w-14 h-4 my-0.5 text-center text-[10px] bg-glitch-dark border border-glitch-cyan text-glitch-cyan font-bold rounded outline-none shadow-neon-cyan/40 focus:ring-1 focus:ring-glitch-cyan"
            />
          ) : (
            <span
              onDoubleClick={() => startEdit('attackMs', attackMs < 1.0 ? attackMs.toFixed(2) : attackMs.toFixed(0))}
              className="font-bold text-glitch-cyan my-0.5 cursor-pointer hover:underline hover:text-white transition-colors select-none"
            >
              {attackMs < 1.0 ? `${attackMs.toFixed(2)}ms` : `${attackMs.toFixed(0)}ms`}
            </span>
          )}
          <input
            type="range"
            min="0.05"
            max="1000"
            step="0.05"
            value={attackMs}
            onChange={(e) => onChange('attackMs', parseFloat(e.target.value))}
            className="w-full h-1 accent-glitch-cyan cursor-pointer"
          />
        </div>

        {/* HOLD */}
        <div
          className="bg-glitch-surface p-1.5 rounded border border-glitch-border flex flex-col items-center group hover:border-glitch-amber/60 transition-colors"
          title="Doppio click per inserire il valore manualmente"
        >
          <span className="text-glitch-dim select-none">HOLD</span>
          {editingParam === 'holdMs' ? (
            <input
              ref={inputRef}
              type="text"
              value={editValue}
              onChange={(e) => setEditValue(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter') commitEdit('holdMs');
                else if (e.key === 'Escape') cancelEdit();
              }}
              onBlur={() => commitEdit('holdMs')}
              className="w-14 h-4 my-0.5 text-center text-[10px] bg-glitch-dark border border-glitch-amber text-glitch-amber font-bold rounded outline-none shadow-neon-amber/40 focus:ring-1 focus:ring-glitch-amber"
            />
          ) : (
            <span
              onDoubleClick={() => startEdit('holdMs', holdMs.toFixed(0))}
              className="font-bold text-glitch-amber my-0.5 cursor-pointer hover:underline hover:text-white transition-colors select-none"
            >
              {holdMs.toFixed(0)}ms
            </span>
          )}
          <input
            type="range"
            min="0"
            max="500"
            step="1"
            value={holdMs}
            onChange={(e) => onChange('holdMs', parseFloat(e.target.value))}
            className="w-full h-1 accent-glitch-amber cursor-pointer"
          />
        </div>

        {/* DECAY */}
        <div
          className="bg-glitch-surface p-1.5 rounded border border-glitch-border flex flex-col items-center group hover:border-glitch-cyan/60 transition-colors"
          title="Doppio click per inserire il valore manualmente"
        >
          <span className="text-glitch-dim select-none">DECAY</span>
          {editingParam === 'decayMs' ? (
            <input
              ref={inputRef}
              type="text"
              value={editValue}
              onChange={(e) => setEditValue(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter') commitEdit('decayMs');
                else if (e.key === 'Escape') cancelEdit();
              }}
              onBlur={() => commitEdit('decayMs')}
              className="w-14 h-4 my-0.5 text-center text-[10px] bg-glitch-dark border border-glitch-cyan text-glitch-cyan font-bold rounded outline-none shadow-neon-cyan/40 focus:ring-1 focus:ring-glitch-cyan"
            />
          ) : (
            <span
              onDoubleClick={() => startEdit('decayMs', decayMs.toFixed(0))}
              className="font-bold text-glitch-cyan my-0.5 cursor-pointer hover:underline hover:text-white transition-colors select-none"
            >
              {decayMs.toFixed(0)}ms
            </span>
          )}
          <input
            type="range"
            min="1"
            max="2000"
            step="1"
            value={decayMs}
            onChange={(e) => onChange('decayMs', parseFloat(e.target.value))}
            className="w-full h-1 accent-glitch-cyan cursor-pointer"
          />
        </div>

        {/* SUSTAIN */}
        <div
          className="bg-glitch-surface p-1.5 rounded border border-glitch-border flex flex-col items-center group hover:border-glitch-pink/60 transition-colors"
          title="Doppio click per inserire il valore manualmente"
        >
          <span className="text-glitch-dim select-none">SUSTAIN</span>
          {editingParam === 'sustain' ? (
            <input
              ref={inputRef}
              type="text"
              value={editValue}
              onChange={(e) => setEditValue(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter') commitEdit('sustain');
                else if (e.key === 'Escape') cancelEdit();
              }}
              onBlur={() => commitEdit('sustain')}
              className="w-14 h-4 my-0.5 text-center text-[10px] bg-glitch-dark border border-glitch-pink text-glitch-pink font-bold rounded outline-none shadow-neon-pink/40 focus:ring-1 focus:ring-glitch-pink"
            />
          ) : (
            <span
              onDoubleClick={() => startEdit('sustain', (sustain * 100).toFixed(0))}
              className="font-bold text-glitch-pink my-0.5 cursor-pointer hover:underline hover:text-white transition-colors select-none"
            >
              {(sustain * 100).toFixed(0)}%
            </span>
          )}
          <input
            type="range"
            min="0"
            max="1"
            step="0.01"
            value={sustain}
            onChange={(e) => onChange('sustain', parseFloat(e.target.value))}
            className="w-full h-1 accent-glitch-pink cursor-pointer"
          />
        </div>

        {/* RELEASE */}
        <div
          className="bg-glitch-surface p-1.5 rounded border border-glitch-border flex flex-col items-center group hover:border-glitch-cyan/60 transition-colors"
          title="Doppio click per inserire il valore manualmente"
        >
          <span className="text-glitch-dim select-none">RELEASE</span>
          {editingParam === 'releaseMs' ? (
            <input
              ref={inputRef}
              type="text"
              value={editValue}
              onChange={(e) => setEditValue(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter') commitEdit('releaseMs');
                else if (e.key === 'Escape') cancelEdit();
              }}
              onBlur={() => commitEdit('releaseMs')}
              className="w-14 h-4 my-0.5 text-center text-[10px] bg-glitch-dark border border-glitch-cyan text-glitch-cyan font-bold rounded outline-none shadow-neon-cyan/40 focus:ring-1 focus:ring-glitch-cyan"
            />
          ) : (
            <span
              onDoubleClick={() => startEdit('releaseMs', releaseMs.toFixed(0))}
              className="font-bold text-glitch-cyan my-0.5 cursor-pointer hover:underline hover:text-white transition-colors select-none"
            >
              {releaseMs.toFixed(0)}ms
            </span>
          )}
          <input
            type="range"
            min="1"
            max="2000"
            step="1"
            value={releaseMs}
            onChange={(e) => onChange('releaseMs', parseFloat(e.target.value))}
            className="w-full h-1 accent-glitch-cyan cursor-pointer"
          />
        </div>

        {/* CURVE SHAPE */}
        <div
          className="bg-glitch-surface p-1.5 rounded border border-glitch-border flex flex-col items-center group hover:border-glitch-green/60 transition-colors"
          title="Doppio click per inserire il valore manualmente (-1 a +1, o EXP / LIN / LOG)"
        >
          <span className="text-glitch-dim select-none">CURVE</span>
          {editingParam === 'curve' ? (
            <input
              ref={inputRef}
              type="text"
              value={editValue}
              onChange={(e) => setEditValue(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter') commitEdit('curve');
                else if (e.key === 'Escape') cancelEdit();
              }}
              onBlur={() => commitEdit('curve')}
              className="w-14 h-4 my-0.5 text-center text-[10px] bg-glitch-dark border border-glitch-green text-glitch-green font-bold rounded outline-none shadow-neon-green/40 focus:ring-1 focus:ring-glitch-green"
            />
          ) : (
            <span
              onDoubleClick={() => startEdit('curve', curve.toFixed(2))}
              className="font-bold text-glitch-green my-0.5 cursor-pointer hover:underline hover:text-white transition-colors select-none"
            >
              {curve < -0.1 ? 'EXP' : curve > 0.1 ? 'LOG' : 'LIN'}
            </span>
          )}
          <input
            type="range"
            min="-1.0"
            max="1.0"
            step="0.05"
            value={curve}
            onChange={(e) => onChange('curve', parseFloat(e.target.value))}
            className="w-full h-1 accent-glitch-green cursor-pointer"
          />
        </div>
      </div>
    </div>
  );
};
