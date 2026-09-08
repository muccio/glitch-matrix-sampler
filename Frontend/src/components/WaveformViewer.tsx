import React, { useRef, useEffect } from 'react';
import { WaveformPeak } from '../types/matrix';
import { FolderOpen, Repeat, ArrowLeftRight } from 'lucide-react';
import { NativeBridge } from '../services/NativeBridge';

interface WaveformViewerProps {
  sourceId: number;
  filePath?: string;
  startPoint: number;
  endPoint: number;
  reverse: boolean;
  speed: number;
  microLoop: boolean;
  loopStart: number;
  loopLengthMs: number;
  crossfadeMs: number;
  peaks?: WaveformPeak[];
  onChange: (paramId: string, value: any) => void;
}

export const WaveformViewer: React.FC<WaveformViewerProps> = ({
  sourceId,
  filePath,
  startPoint,
  endPoint,
  reverse,
  speed,
  microLoop,
  loopStart,
  loopLengthMs,
  crossfadeMs,
  peaks = [],
  onChange
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const w = canvas.width;
    const h = canvas.height;
    const centerY = h / 2;

    // Clear background
    ctx.fillStyle = '#07090e';
    ctx.fillRect(0, 0, w, h);

    // Draw grid
    ctx.strokeStyle = 'rgba(42, 54, 74, 0.4)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, centerY);
    ctx.lineTo(w, centerY);
    for (let x = 0; x < w; x += 40) {
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
    }
    ctx.stroke();

    // Draw dummy peaks if none loaded yet
    const displayPeaks = (peaks && peaks.length > 0)
      ? peaks
      : Array.from({ length: 200 }, (_, i) => {
          const t = i / 200;
          const env = Math.exp(-t * 3) * Math.sin(t * 40);
          return { min: -Math.abs(env), max: Math.abs(env) };
        });

    const step = w / displayPeaks.length;

    // Inactive waveform background
    ctx.fillStyle = 'rgba(100, 116, 139, 0.3)';
    displayPeaks.forEach((p, idx) => {
      const x = idx * step;
      const yMin = centerY - p.min * centerY * 0.85;
      const yMax = centerY - p.max * centerY * 0.85;
      ctx.fillRect(x, yMin, Math.max(1, step - 0.5), yMax - yMin);
    });

    // Active Trim Region
    const startX = startPoint * w;
    const endX = endPoint * w;
    const activeW = Math.max(0, endX - startX);

    ctx.save();
    ctx.beginPath();
    ctx.rect(startX, 0, activeW, h);
    ctx.clip();

    // Active Waveform highlight
    ctx.fillStyle = '#00f0ff';
    displayPeaks.forEach((p, idx) => {
      const x = idx * step;
      const yMin = centerY - p.min * centerY * 0.85;
      const yMax = centerY - p.max * centerY * 0.85;
      ctx.fillRect(x, yMin, Math.max(1, step - 0.5), yMax - yMin);
    });

    ctx.restore();

    // Micro-loop region highlight
    if (microLoop) {
      const loopStartX = loopStart * w;
      // Approximate visual width based on loop length (assume sample length ~2 sec)
      const loopWidthX = Math.min(w - loopStartX, Math.max(10, (loopLengthMs / 2000) * w));

      ctx.fillStyle = 'rgba(255, 170, 0, 0.25)';
      ctx.fillRect(loopStartX, 0, loopWidthX, h);

      ctx.strokeStyle = '#ffaa00';
      ctx.lineWidth = 1.5;
      ctx.strokeRect(loopStartX, 0, loopWidthX, h);
    }

    // Trim markers
    ctx.strokeStyle = '#00ff66';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(startX, 0);
    ctx.lineTo(startX, h);
    ctx.stroke();

    ctx.strokeStyle = '#ff0055';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(endX, 0);
    ctx.lineTo(endX, h);
    ctx.stroke();

  }, [startPoint, endPoint, microLoop, loopStart, loopLengthMs, peaks]);

  return (
    <div className="bg-glitch-panel/50 border border-glitch-border rounded p-3 flex flex-col gap-2.5">
      {/* File Header & Actions */}
      <div className="flex items-center justify-between text-xs">
        <div className="flex items-center gap-2 truncate">
          <span className="font-bold text-glitch-amber tracking-wide flex items-center gap-1.5">
            <span className="w-1.5 h-1.5 rounded-full bg-glitch-amber shadow-neon-amber animate-pulse" />
            MICRO-SAMPLER & TRANSIENT SLICING
          </span>
          <span className="text-[10px] text-glitch-dim truncate max-w-[200px]" title={filePath}>
            {filePath ? filePath.split('/').pop() : 'No sample loaded'}
          </span>
        </div>

        <button
          onClick={() => NativeBridge.openFileDialog(sourceId)}
          className="flex items-center gap-1.5 px-2.5 py-1 bg-glitch-surface hover:bg-glitch-card text-glitch-cyan text-[11px] rounded border border-glitch-cyan/40 hover:border-glitch-cyan transition-all"
        >
          <FolderOpen className="w-3.5 h-3.5" />
          LOAD AUDIO FILE
        </button>
      </div>

      {/* Canvas Waveform */}
      <div className="relative rounded overflow-hidden border border-glitch-border/80">
        <canvas
          ref={canvasRef}
          width={600}
          height={100}
          className="w-full h-24 block"
        />

        {/* Start / End Trim overlays */}
        <div className="absolute top-1 left-2 text-[9px] font-mono text-glitch-green bg-glitch-dark/80 px-1 rounded border border-glitch-green/30">
          START: {(startPoint * 100).toFixed(1)}%
        </div>
        <div className="absolute top-1 right-2 text-[9px] font-mono text-glitch-pink bg-glitch-dark/80 px-1 rounded border border-glitch-pink/30">
          END: {(endPoint * 100).toFixed(1)}%
        </div>
      </div>

      {/* Slicing & Playback Controls */}
      <div className="grid grid-cols-4 gap-2 text-[10px]">
        {/* START TRIM */}
        <div className="bg-glitch-surface p-1.5 rounded border border-glitch-border flex flex-col items-center">
          <span className="text-glitch-dim">START POINT</span>
          <span className="font-bold text-glitch-green my-0.5">{(startPoint * 100).toFixed(0)}%</span>
          <input
            type="range"
            min="0"
            max="0.99"
            step="0.005"
            value={startPoint}
            onChange={(e) => onChange('startPoint', parseFloat(e.target.value))}
            className="w-full h-1 accent-glitch-green cursor-pointer"
          />
        </div>

        {/* END TRIM */}
        <div className="bg-glitch-surface p-1.5 rounded border border-glitch-border flex flex-col items-center">
          <span className="text-glitch-dim">END POINT</span>
          <span className="font-bold text-glitch-pink my-0.5">{(endPoint * 100).toFixed(0)}%</span>
          <input
            type="range"
            min="0.01"
            max="1"
            step="0.005"
            value={endPoint}
            onChange={(e) => onChange('endPoint', parseFloat(e.target.value))}
            className="w-full h-1 accent-glitch-pink cursor-pointer"
          />
        </div>

        {/* SPEED / RATE */}
        <div className="bg-glitch-surface p-1.5 rounded border border-glitch-border flex flex-col items-center">
          <span className="text-glitch-dim">PLAY SPEED</span>
          <span className="font-bold text-glitch-cyan my-0.5">{speed.toFixed(2)}x</span>
          <input
            type="range"
            min="0.1"
            max="4.0"
            step="0.05"
            value={speed}
            onChange={(e) => onChange('speed', parseFloat(e.target.value))}
            className="w-full h-1 accent-glitch-cyan cursor-pointer"
          />
        </div>

        {/* REVERSE TOGGLE */}
        <div className="bg-glitch-surface p-1.5 rounded border border-glitch-border flex flex-col items-center justify-between">
          <span className="text-glitch-dim">REVERSE</span>
          <button
            onClick={() => onChange('reverse', !reverse)}
            className={`flex items-center gap-1 px-3 py-1 rounded text-[10px] font-bold transition-all border ${
              reverse
                ? 'bg-glitch-purple text-glitch-dark border-glitch-purple shadow-sm'
                : 'bg-glitch-dark text-glitch-dim border-glitch-border'
            }`}
          >
            <ArrowLeftRight className="w-3 h-3" />
            {reverse ? 'ON' : 'OFF'}
          </button>
        </div>
      </div>

      {/* Micro-Looping Mode Controls */}
      <div className="bg-glitch-surface/70 border border-glitch-border/60 rounded p-2 flex flex-col gap-1.5">
        <div className="flex items-center justify-between text-[11px]">
          <div className="flex items-center gap-2">
            <Repeat className="w-3.5 h-3.5 text-glitch-amber" />
            <span className="font-bold text-glitch-amber">MICRO-LOOPING GRAIN</span>
          </div>

          <button
            onClick={() => onChange('microLoop', !microLoop)}
            className={`px-2 py-0.5 rounded text-[10px] font-bold border transition-all ${
              microLoop
                ? 'bg-glitch-amber text-glitch-dark border-glitch-amber shadow-neon-amber'
                : 'bg-glitch-panel text-glitch-dim border-glitch-border'
            }`}
          >
            {microLoop ? 'LOOP ACTIVE' : 'LOOP BYPASS'}
          </button>
        </div>

        {microLoop && (
          <div className="grid grid-cols-3 gap-2 text-[10px] pt-1">
            <div className="flex flex-col items-center">
              <span className="text-glitch-dim">LOOP START: {(loopStart * 100).toFixed(0)}%</span>
              <input
                type="range"
                min="0"
                max="0.95"
                step="0.01"
                value={loopStart}
                onChange={(e) => onChange('loopStart', parseFloat(e.target.value))}
                className="w-full h-1 accent-glitch-amber cursor-pointer"
              />
            </div>

            <div className="flex flex-col items-center">
              <span className="text-glitch-dim">LOOP LEN: {loopLengthMs.toFixed(1)}ms</span>
              <input
                type="range"
                min="1.0"
                max="300.0"
                step="1.0"
                value={loopLengthMs}
                onChange={(e) => onChange('loopLengthMs', parseFloat(e.target.value))}
                className="w-full h-1 accent-glitch-amber cursor-pointer"
              />
            </div>

            <div className="flex flex-col items-center">
              <span className="text-glitch-dim">CROSSFADE: {crossfadeMs.toFixed(1)}ms</span>
              <input
                type="range"
                min="0.5"
                max="30.0"
                step="0.5"
                value={crossfadeMs}
                onChange={(e) => onChange('crossfadeMs', parseFloat(e.target.value))}
                className="w-full h-1 accent-glitch-amber cursor-pointer"
              />
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
