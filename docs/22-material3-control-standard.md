# OneUI Material 3 Control Standard

OneUI control styling follows Material 3 principles, adapted for native desktop
shells and Win7-compatible renderers. Product UI must use tokens first, then
component rules. Do not tune per-control shadows, borders, or hover colors by
eye in product code.

## Source Of Truth

- Material 3 color roles: surface, surface container, primary, outline, error.
- Material 3 state layers: hover, focus, pressed, dragged, disabled.
- Flutter-style state resolution: a control resolves visual properties from
  normal, hovered, focused, pressed, and disabled states before painting.

## Dark Scheme Baseline

Use `Material3DarkColorScheme` from `oneui/material3_tokens.h`.

- Window background: `surface`
- Sidebar and cards: `surfaceContainerLow`
- Inputs: `surfaceContainer`
- Raised buttons and selected navigation: `surfaceContainerHigh`
- Borders: `outlineVariant`
- Primary action: `primaryContainer`
- Primary text: `onSurface`
- Muted text: `onSurfaceVariant`

## State Layer Opacity

State layers are blended over the container color:

- Hover: 0.08
- Focus: 0.10
- Pressed: 0.10
- Dragged: 0.16
- Disabled content: 0.38
- Disabled container: 0.12

The helper `material3StateLayer(base, stateColor, state)` exists so native and
canvas renderers can use the same math.

## Elevation

Prefer fewer elevation levels:

- Level 0: flat surfaces, text fields at rest, window background
- Level 1: cards, search boxes, subtle toolbar chips
- Level 2: hovered elevated controls
- Level 3: active overlays and focus-emphasized controls

The helper `material3ElevationShadow(level)` maps these levels to consistent
Win32-friendly soft shadows. Product code should not invent new blur/offset
pairs unless OneUI adds a new token.

## Component Rules

- Buttons use one container color, one border token, and state-layer overlays.
  Hover should not change hue family; pressed should reduce elevation and darken
  through state resolution.
- Text fields use `surfaceContainer`, `outlineVariant`, and a one-pixel focus
  outline with primary color. Inner shadows are allowed only to express inset
  depth, not as decoration.
- Navigation selected state uses a low-contrast container plus primary text/icon.
  It should not look like a separate card.
- Cards use Level 1 elevation and a single outline. Gradients are allowed for
  marketing/recent-device cards only.
- Chrome buttons use transparent rest state, state-layer hover/pressed, and
  destructive red only for close hover/pressed.

Remote and future products should document any deviation in the product CSS
block with the token name being adapted.
