# Sk/Ai Design System — Aurora Glass

The supplied Sk/Ai reference screenshot is the visual source of truth. New screens and components should extend this system instead of introducing another visual language.

## Composition
- Windows desktop application frame with compact tabs and navigation/omnibox toolbar.
- Narrow vertical navigation rail on the left.
- Main start-page canvas occupies the center-left.
- Persistent AI workspace docked on the right; it must not cover websites with a full-screen modal.
- The assistant can collapse or detach into a separate native Windows popup. Native topmost behavior is implemented by Win32, not CSS.

## Visual language
- Deep graphite/navy glass surfaces, restrained blue-violet borders and subtle backdrop blur.
- Aurora atmosphere: violet, electric blue, cyan and teal highlights.
- Start page with northern-lights / mountain-lake atmosphere and centered Sk/Ai wordmark above search.
- Translucent blue glass cards, clear icon colors and fine low-contrast borders.
- Assistant surface in dark navy; Local identity is mint/teal, Cloud identity is violet.
- Avoid generic gray SaaS styling, excessive pill shapes, plastic gloss and heavy bloom.

## Palette tokens
- Base navy: #080D22
- Window navy: #0C132C
- Panel navy: #141D3E
- Glass card: rgba(83, 99, 161, .58)
- Violet: #9868FF
- Violet highlight: #C38AFF
- Electric blue: #547BFF
- Cyan: #52D9FF
- Teal: #54E9C0
- Primary text: #F1F2FF
- Secondary text: #AAB1D0
- Hairline border: rgba(190, 205, 255, .16)

## Type and spacing
- Manrope for brand/display typography; DM Sans for interface copy.
- Large central wordmark, generous search field, compact shortcut labels and concise assistant copy.
- Desktop should feel spacious without wasting the center canvas.
- Keep the left rail narrow and assistant panel around 25–28% of wide desktop width.

## Interaction states
- Selected navigation item: violet-blue gradient with restrained glow.
- Local AI: mint indicator; never display it as connected while the model is unavailable.
- Cloud AI: violet indicator; never imply transmission while no provider is connected.
- Unavailable feature states are explicit and neutral, not green success badges.
- Hover: slight elevation / border illumination, no large movement.
- Reduced-motion preference disables decorative motion.
- Light theme is coordinated pale blue/lavender glass, not a simple color inversion.

## Implementation status

The HTML/CSS/JS UI is integrated with a native CEF/Win32 controller in source: tabs, navigation, loading state, search routing, docked assistant and a floating native assistant window. It has not yet passed Windows CI or a Windows 11 runtime test. Local Qwen inference, cloud-provider connections and privacy filtering are not implemented in the native shell. UI labels must not imply unavailable model/security functionality is active.
