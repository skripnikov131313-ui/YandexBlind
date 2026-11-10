"use strict";

chrome.sidePanel.setPanelBehavior({ openPanelOnActionClick: true }).catch((error) => {
  console.error("Sk/Ai side panel setup failed:", error);
});
