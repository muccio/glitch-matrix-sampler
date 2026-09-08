import React, { useState, useEffect } from 'react';
import { GlobalState, VoiceStats } from './types/matrix';
import { NativeBridge } from './services/NativeBridge';
import { GlobalBar } from './components/GlobalBar';
import { SourceList } from './components/SourceList';
import { SourceInspector } from './components/SourceInspector';
import { AuditionKeyboard } from './components/AuditionKeyboard';

export const App: React.FC = () => {
  const [state, setState] = useState<GlobalState>({
    version: '1.0.0',
    masterVolume: 1.0,
    sources: []
  });

  const [stats, setStats] = useState<VoiceStats>({
    activeVoices: 0,
    peakL: 0,
    peakR: 0
  });

  const [selectedSourceId, setSelectedSourceId] = useState<number | null>(null);

  useEffect(() => {
    const unsubState = NativeBridge.subscribeState((newState) => {
      setState(newState);
      if (newState.sources.length > 0) {
        setSelectedSourceId((prev) => {
          if (prev === null || !newState.sources.some((s) => s.id === prev)) {
            return newState.sources[0].id;
          }
          return prev;
        });
      } else {
        setSelectedSourceId(null);
      }
    });

    const unsubStats = NativeBridge.subscribeVoiceStats((newStats) => {
      setStats(newStats);
    });

    return () => {
      unsubState();
      unsubStats();
    };
  }, []);

  const handleUpdateParameter = (sourceId: number, paramId: string, value: any) => {
    // Optimistic local update
    setState((prev) => {
      const updated = prev.sources.map((s) => {
        if (s.id === sourceId) {
          return { ...s, [paramId]: value };
        }
        return s;
      });
      return { ...prev, sources: updated };
    });

    NativeBridge.updateParameter(sourceId, paramId, value);
  };

  const handleMasterVolumeChange = (vol: number) => {
    setState((prev) => ({ ...prev, masterVolume: vol }));
    NativeBridge.setMasterVolume(vol);
  };

  const selectedSource = state.sources.find((s) => s.id === selectedSourceId) ?? null;

  return (
    <div className="flex flex-col h-screen w-screen bg-glitch-dark text-glitch-text overflow-hidden">
      {/* Top Global Navigation Bar */}
      <GlobalBar
        masterVolume={state.masterVolume}
        stats={stats}
        sourceCount={state.sources.length}
        onMasterVolumeChange={handleMasterVolumeChange}
      />

      {/* Main Rack View */}
      <div className="flex-1 flex overflow-hidden">
        {/* Left Column: Dynamic Sources Matrix */}
        <SourceList
          sources={state.sources}
          selectedSourceId={selectedSourceId}
          onSelectSource={setSelectedSourceId}
          onUpdateSource={handleUpdateParameter}
        />

        {/* Right Column: Deep Inspector Panel */}
        <SourceInspector
          source={selectedSource}
          onUpdate={(paramId, val) => {
            if (selectedSourceId !== null) {
              handleUpdateParameter(selectedSourceId, paramId, val);
            }
          }}
        />
      </div>

      {/* Bottom Audition Strip */}
      <AuditionKeyboard />
    </div>
  );
};

export default App;
