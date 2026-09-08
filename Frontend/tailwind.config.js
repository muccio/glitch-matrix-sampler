/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        glitch: {
          dark: '#07090e',
          surface: '#0d1117',
          card: '#141a24',
          panel: '#1b2330',
          border: '#2a364a',
          cyan: '#00f0ff',
          pink: '#ff0055',
          amber: '#ffaa00',
          green: '#00ff66',
          purple: '#b026ff',
          text: '#e2e8f0',
          dim: '#718096'
        }
      },
      fontFamily: {
        mono: ['JetBrains Mono', 'Fira Code', 'ui-monospace', 'SFMono-Regular', 'Menlo', 'monospace']
      },
      boxShadow: {
        'neon-cyan': '0 0 10px rgba(0, 240, 255, 0.4), 0 0 20px rgba(0, 240, 255, 0.2)',
        'neon-pink': '0 0 10px rgba(255, 0, 85, 0.4), 0 0 20px rgba(255, 0, 85, 0.2)',
        'neon-green': '0 0 10px rgba(0, 255, 102, 0.4)',
        'neon-amber': '0 0 10px rgba(255, 170, 0, 0.4)'
      }
    },
  },
  plugins: [],
}
