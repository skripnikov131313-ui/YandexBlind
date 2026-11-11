# Sk/Ai Native Shell — UI Prototype

This directory starts the independent Sk/Ai browser shell.

## Scope of this milestone

- Original Sk/Ai window chrome, tab strip, omnibox, navigation rail and workspace.
- Docked assistant on the right; it is not a modal overlay over the page.
- Assistant can switch between docked and floating presentation states, collapse, and expand.
- Local / Cloud selector is part of the prompt composer. The selected mode is visible at all times.
- Theme selector supports Soft Aurora, Quiet Mist and Deep Space.
- Responsive layout for compact desktop windows.

## Important implementation boundary

This is a front-end interaction prototype. The central page is a demonstration surface, not a live Chromium tab. The assistant responses are local demo responses, not a connected LLM. The next engineering milestone is to host this UI inside the native Sk/Ai shell and connect browser tabs/navigation to Chromium's content layer and the assistant to the local inference service.

The long-term browser engine is Chromium. Sk/Ai owns the window chrome, navigation UX, tabs, settings, privacy controls and AI workspace. We are not writing a new HTML/CSS/JavaScript rendering engine.

## Run

Open `ui/index.html` in a modern desktop browser. No build step or external runtime dependency is required.
