# Render Bridge — Draw List Architecture

## Pipeline

```
GScreenClass::Render()
  │
  ├─ Begin_Draw_List()              ← recording ON
  │
  ├─ Draw_It(IsToRedraw)           ← ALL tactical calls deferred:
  │   ├─ Draw_Stamp()              →  CMD_STAMP   (terrain tiles)
  │   ├─ CC_Draw_Shape()           →  CMD_SHAPE   (sprites, shadows)
  │   ├─ Fill_Rect()               →  CMD_FILL_RECT (health bars, shadow rects)
  │   ├─ Draw_Rect()               →  CMD_DRAW_RECT (selection boxes)
  │   ├─ Draw_Line()               →  CMD_DRAW_LINE (selection corners)
  │   └─ Put_Pixel()               →  CMD_PUT_PIXEL (debug markers)
  │
  ├─ End_Draw_List()                ← recording OFF, replay:
  │   │
  │   ├─ zoom == 1.0:
  │   │   replay_world()            terrain + sprites at 1:1
  │   │   replay_overlays()         primitives at 1:1
  │   │   (output identical to legacy)
  │   │
  │   └─ zoom > 1.0:
  │       replay_world()            terrain + sprites at 1:1
  │       zoom_tactical_inplace()   pixel-zoom world (nearest-neighbor)
  │       replay_overlays_zoomed()  primitives at zoomed positions, 1:1 size
  │
  ├─ Buttons->Draw_All()           ← 1:1, after zoom
  ├─ Messages.Draw()               ← 1:1
  ├─ ActionMenu.Draw_It()          ← 1:1
  ├─ Blit_Display()                ← HidPage → SeenBuff
  └─ TD_SDL_Present()              ← SeenBuff → RGBA → SDL → screen
```

## Layer Separation

| Layer | Content | Zoom behavior |
|-------|---------|---------------|
| **World** | Terrain tiles (CMD_STAMP), unit/building sprites (CMD_SHAPE) | Pixel-scaled by zoom factor |
| **Overlay** | Health bars, selection boxes, aim lines, debug markers | Positioned at zoomed coordinates, native pixel size |
| **UI** | Buttons, messages, action menu | Always 1:1, drawn after zoom |
| **Chrome** | Sidebar, tab bar | Always 1:1, outside tactical area |

## Screen Layout

```
┌──────────────────────────────────────────┬──────────┐
│            Tab Bar (always 1:1)          │          │
├──────────────────────────────────────────┤ Sidebar  │
│                                          │ (1:1)    │
│   Tactical Viewport                     │          │
│   ┌──────────────────────────────┐      │ Build    │
│   │ WORLD layer (zoomed):        │      │ queue    │
│   │   terrain, units, buildings  │      │          │
│   │                              │      │ Radar    │
│   │ OVERLAY layer (1:1 size):    │      │          │
│   │   health bars, selection     │      │ Power    │
│   └──────────────────────────────┘      │          │
│   UI layer (1:1): buttons, messages     │          │
│                                          │          │
└──────────────────────────────────────────┴──────────┘
```

## Draw List Commands

| Type | Engine function | Count (typical) | Layer |
|------|----------------|-----------------|-------|
| CMD_STAMP | Draw_Stamp | ~300 | World |
| CMD_SHAPE | CC_Draw_Shape | ~400 | World |
| CMD_FILL_RECT | Fill_Rect | ~50 | Overlay |
| CMD_DRAW_RECT | Draw_Rect | ~10 | Overlay |
| CMD_DRAW_LINE | Draw_Line | ~40 | Overlay |
| CMD_PUT_PIXEL | Put_Pixel | ~5 | Overlay |

## Zoom

- **Scroll up**: zoom in (1.0 → 4.0)
- **Scroll down**: zoom out (→ 1.0 minimum)
- **Anchor**: mouse cursor — point under cursor stays fixed
- **Viewport**: tracks which sub-region of tactical area is visible when zoomed

## Mouse Coordinate Transform

```
SDL events → g_mouse_x/y (raw screen position)
    ↓
TD_SDL_Present() → Apply_Scroll_Zoom() (snapshot raw, apply wheel delta)
    ↓
PumpEvents end → Transform_Mouse()
    maps screen tactical position → source tactical position:
    src = viewport + (screen - tac_origin) / zoom
    ↓
g_mouse_x/y + _Kbd->MouseQX/Y updated with transformed values
    ↓
Game logic reads correct coordinates
```

## Hooked Functions

| File | Function | Hook |
|------|----------|------|
| `modern.src/gscreen.cpp` | `GScreenClass::Render()` | Begin/End draw list around Draw_It |
| `modern.src/conquer.cpp` | `CC_Draw_Shape()` | Record + skip when recording |
| `platform.sdl3/cnc/sdl3_render.cpp` | `Draw_Stamp()` | Record + skip |
| `platform.sdl3/cnc/sdl3_render.cpp` | `Fill_Rect()` | Record + skip |
| `platform.sdl3/cnc/sdl3_render.cpp` | `Draw_Rect()` | Record + skip |
| `platform.sdl3/cnc/sdl3_render.cpp` | `Draw_Line()` | Record + skip |
| `platform.sdl3/cnc/sdl3_render.cpp` | `Put_Pixel()` | Record + skip |
| `platform.sdl3/cnc/sdl3_input.cpp` | `PumpEvents()` | Mouse transform + wheel capture |

All hooks are `#ifdef USE_RENDER_BRIDGE`. Legacy build compiles identically.

## Files

```
render_bridge/
  RENDERING.md           This document
  render_bridge.h/cpp    Bridge init, compositor, passthrough blit
  present.cpp            TD_SDL_Present replacement (passthrough + SDL)
  init.cpp               Game init hook
  zoom.cpp               Scroll input, viewport tracking, mouse transform
  zoom_tactical.cpp      Draw list begin/end, replay, in-place zoom
  draw_list.h/cpp        DrawCommand types and DrawList class
  draw_list_hooks.cpp    Recording hooks (extern functions called from engine)
  CMakeLists.txt         Source lists
```

## Future: GL Rendering Path

The draw list is the foundation for GPU-accelerated rendering:

```
Current:  draw list → replay via CPU (CC_Draw_Shape, Draw_Stamp) → pixel zoom
Future:   draw list → ISpriteProvider → GL sprite batch → GPU zoom + render
```

Steps:
1. ✓ Draw list captures all tactical rendering
2. ✓ World/overlay separation during zoom
3. Route CMD_SHAPE through LegacySpriteProvider → SpriteFrame
4. Pack SpriteFrames into TextureAtlas
5. Replay via GLSpriteBatch (one draw call per atlas page)
6. GL palette shader for terrain (CMD_STAMP → indexed texture)
7. GL primitives for overlays (lines, rects as GL quads)
