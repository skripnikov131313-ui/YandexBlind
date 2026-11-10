"use strict";

// The browser owns the side-panel layout: Sk/Ai is docked beside the page,
// never rendered as a modal overlay over website content.
chrome.sidePanel.setPanelBehavior({ openPanelOnActionClick: true }).catch((error) => {
  console.error("Sk/Ai side panel setup failed:", error);
});

// A keyboard shortcut is an explicit browser action and also grants the
// temporary activeTab permission used only when the user asks to read a page.
chrome.commands.onCommand.addListener(async (command) => {
  if (command !== "toggle-skai-panel") return;
  try {
    const [tab] = await chrome.tabs.query({ active: true, lastFocusedWindow: true });
    if (tab?.windowId != null) await chrome.sidePanel.open({ windowId: tab.windowId });
  } catch (error) {
    console.error("Sk/Ai side panel could not be opened:", error);
  }
});
