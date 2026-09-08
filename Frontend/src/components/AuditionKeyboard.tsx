import React, { useState } from 'react';
import { NativeBridge } from '../services/NativeBridge';
import { ChevronLeft, ChevronRight, Music } from 'lucide-react';

const WHITE_KEYS = [0, 2, 4, 5, 7, 9, 11]; // C, D, E, F, G, A, B
const BLACK_KEYS: { [key: number]: number } = {
  0: 1,  // C#
  2: 3,  // D#
  5: 6,  // F#
  7: 8,  // G#
  9: 10  // A#
};

export const AuditionKeyboard: React.FC = () => {
  const [octaveOffset, setOctaveOffset] = useState<number>(0); // Middle C = 60
  const [activeNotes, setActiveNotes] = useState<Set<number>>(new Set());

  const baseNote = 48 + octaveOffset * 12; // C3 base

  const handleNoteOn = (note: number) => {
    setActiveNotes((prev) => new Set(prev).add(note));
    NativeBridge.noteOn(note, 0.85);
  };

  const handleNoteOff = (note: number) => {
    setActiveNotes((prev) => {
      const next = new Set(prev);
      next.delete(note);
      return next;
    });
    NativeBridge.noteOff(note);
  };

  // Render 2 octaves (24 semitones, 14 white keys)
  const renderKeys = () => {
    const keys = [];

    for (let oct = 0; oct < 2; oct++) {
      for (let i = 0; i < 7; i++) {
        const whiteSemis = WHITE_KEYS[i];
        const currentMidi = baseNote + oct * 12 + whiteSemis;
        const isWhiteActive = activeNotes.has(currentMidi);

        const blackSemis = BLACK_KEYS[whiteSemis];
        const blackMidi = blackSemis !== undefined ? baseNote + oct * 12 + blackSemis : null;
        const isBlackActive = blackMidi !== null && activeNotes.has(blackMidi);

        keys.push(
          <div key={`key-${oct}-${i}`} className="relative inline-block h-12 flex-1 min-w-[24px]">
            {/* White Key */}
            <button
              onMouseDown={() => handleNoteOn(currentMidi)}
              onMouseUp={() => handleNoteOff(currentMidi)}
              onMouseLeave={() => activeNotes.has(currentMidi) && handleNoteOff(currentMidi)}
              className={`w-full h-full rounded-b border border-glitch-border/80 transition-colors ${
                isWhiteActive
                  ? 'bg-glitch-cyan shadow-neon-cyan'
                  : 'bg-glitch-surface hover:bg-glitch-panel'
              }`}
            >
              {whiteSemis === 0 && (
                <span className="absolute bottom-1 left-1 text-[8px] font-bold text-glitch-dim">
                  C{Math.floor(currentMidi / 12) - 1}
                </span>
              )}
            </button>

            {/* Black Key */}
            {blackMidi !== null && (
              <button
                onMouseDown={(e) => {
                  e.stopPropagation();
                  handleNoteOn(blackMidi);
                }}
                onMouseUp={(e) => {
                  e.stopPropagation();
                  handleNoteOff(blackMidi);
                }}
                onMouseLeave={() => activeNotes.has(blackMidi) && handleNoteOff(blackMidi)}
                style={{ left: '60%', width: '70%', height: '62%' }}
                className={`absolute top-0 z-10 rounded-b border border-glitch-border transition-colors ${
                  isBlackActive
                    ? 'bg-glitch-pink shadow-neon-pink'
                    : 'bg-glitch-dark hover:bg-glitch-card'
                }`}
              />
            )}
          </div>
        );
      }
    }
    return keys;
  };

  return (
    <div className="h-14 bg-glitch-surface/90 border-t border-glitch-border px-4 flex items-center justify-between gap-4 select-none">
      {/* Octave Controls */}
      <div className="flex items-center gap-2">
        <div className="flex items-center gap-1 text-[11px] text-glitch-dim">
          <Music className="w-3.5 h-3.5 text-glitch-cyan" />
          <span>OCT:</span>
        </div>

        <button
          onClick={() => setOctaveOffset((o) => Math.max(-2, o - 1))}
          className="p-1 rounded bg-glitch-panel border border-glitch-border hover:text-glitch-cyan"
        >
          <ChevronLeft className="w-3.5 h-3.5" />
        </button>

        <span className="font-bold text-xs text-glitch-cyan w-6 text-center">
          {octaveOffset >= 0 ? `+${octaveOffset}` : octaveOffset}
        </span>

        <button
          onClick={() => setOctaveOffset((o) => Math.min(2, o + 1))}
          className="p-1 rounded bg-glitch-panel border border-glitch-border hover:text-glitch-cyan"
        >
          <ChevronRight className="w-3.5 h-3.5" />
        </button>
      </div>

      {/* Keys Row */}
      <div className="flex-1 max-w-2xl h-10 flex items-stretch">
        {renderKeys()}
      </div>

      {/* Audition hint */}
      <span className="text-[10px] text-glitch-dim font-mono hidden md:inline">
        AUDITION STRIP (CLICK OR PLAY MIDI)
      </span>
    </div>
  );
};
