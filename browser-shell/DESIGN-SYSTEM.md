# Sk/Ai Design System — Aurora Glass

The supplied Sk/Ai screenshot is the visual source of truth for the browser shell. New screens and components must follow this system rather than introducing a separate visual language.

## Composition
- Windows desktop application frame with a compact tab strip and navigation/omnibox toolbar.
- Narrow vertical navigation rail on the left.
- Main start-page canvas occupies the center-left.
- Persistent AI workspace docked on the right; never cover the website with a full-screen AI modal.
- AI panel may collapse or enter a floating presentation state. Native always-on-top behavior belongs to the Windows shell, not CSS.

## Visual language
- Deep navy / indigo glass surfaces, subtle blue-violet borders and restrained backdrop blur.
- Aurora atmosphere: violet, electric blue, cyan and teal light; accents glow locally, not across every surface.
- Start page has a dramatic northern-lights / mountain-lake visual atmosphere, with the Sk/Ai wordmark centered above search.
- Cards are translucent blue glass with clear icon colors and low-contrast borders.
- Assistant uses a separate dark navy surface; Local mode is mint/teal, Cloud mode is violet.
- Avoid generic flat gray SaaS UI, excessive rounded pills, plastic gloss and heavy bloom.

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
- Manrope for brand / display typography; DM Sans for interface copy.
- Strong hierarchy: large centered wordmark, medium search field, compact shortcut labels, concise assistant copy.
- Desktop spacing should feel spacious without wasting the center canvas.
- Keep the left rail narrow and AI panel around 25% of window width at wide desktop sizes.

## Interaction states
- Selected navigation item: violet-blue gradient with soft glow.
- Local AI: mint indicator and teal selection outline.
- Cloud AI: violet indicator and violet selection outline.
- Hover: slight elevation / border illumination, no large movement.
- Reduced-motion preference disables decorative motion.
- Light theme is a coordinated pale blue/lavender glass treatment, not a simple color inversion.

## Implementation status
The current HTML/CSS/JS is a visual and interaction prototype. Browser navigation, real tabs, Windows window controls, local inference and security scanning require native/core integration. UI labels must not imply those features are active until connected and verified.
