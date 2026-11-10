# Sk/Ai Browser UI Prototype

This is the first standalone implementation of the supplied Sk/Ai visual direction in HTML, CSS and JavaScript.

## Run

Open `index.html` in a current desktop browser. No package manager, build tool, framework, server or external asset is required.

## Included

- Browser window frame, tabs, window controls and navigation toolbar.
- Omnibox and search provider selector.
- Left navigation rail and workspace pages.
- Soft Aurora start page and shortcut cards.
- Right-side assistant panel with Local / Cloud mode switch.
- Responsive layout, dark/light themes and reduced-motion support.
- Demonstration chat UI with explicit notices that the model and network navigation are not connected.

## Important boundaries

This is an interactive **UI prototype**, not a compiled browser. It does not render arbitrary websites, persist a real browser profile, contact a search provider, run Qwen or send chat content to a cloud service. The Local / Cloud switch changes only the preview UI; no data is transmitted.

The next implementation phase is to port this visual system into native Chromium Views / browser-owned UI components and connect them to the browser's actual navigation, tab, profile and local AI services. The prototype is intentionally separate from the extension-based legacy proof of concept.
