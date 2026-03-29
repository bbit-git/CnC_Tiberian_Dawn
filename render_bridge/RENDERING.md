# Render Bridge — Layer Separation & Draw List Architecture

## Overview

The render bridge intercepts the C&C TD rendering pipeline to enable tactical map zoom while keeping all UI elements at native resolution.

## Rendering Pipeline (Legacy)

```
GScreenClass::Render()
  ├─ Set_Logic_Page(HidPage)           ← 8-bit indexed off-screen buffer
  ├─ Draw_It(IsToRedraw)               ← EVERYTHING: terrain, sprites, health bars, selection
  ├─ Buttons->Draw_All()               ← Dialog buttons (gadgets)
  ├─ Messages.Draw()                   ← Chat/event messages
  ├─ ActionMenu.Draw_It()              ← Right-click action menu
  ├─ Blit_Display()                    ← HidPage → SeenBuff (VisiblePage)
  └─ TD_SDL_Present()                  ← SeenBuff → palette LUT → RGBA → SDL texture → screen
```

All draws go to a single 8-bit indexed buffer. No layer separation exists.

## Rendering Pipeline (Bridge)

```
GScreenClass::Render()
  ├─ Set_Logic_Page(HidPage)
  │
  ├─ Render_Bridge_Begin_Draw_List()   ← Start recording CC_Draw_Shape calls
  ├─ Draw_It(IsToRedraw)               ← World renders normally + draw list captures shapes
  ├─ Render_Bridge_End_Draw_List()     ← Apply zoom in-place to tactical area
  │
  ├─ Buttons->Draw_All()               ← Drawn AFTER zoom, at 1:1
  ├─ Messages.Draw()                   ← At 1:1
  ├─ ActionMenu.Draw_It()              ← At 1:1
  ├─ Blit_Display()                    ← HidPage → SeenBuff
  └─ TD_SDL_Present()                  ← Passthrough → SDL present
```

The `#ifdef USE_RENDER_BRIDGE` hooks in `gscreen.cpp` (2 lines) and `conquer.cpp` (1 block) are the only changes to `modern.src/`.

## Screen Regions

```
┌────────────────────────────────────────────┬──────────┐
│              Tab Bar (UI_TAB)              │          │
│           credits, EVA, options            │          │
├────────────────────────────────────────────┤ Sidebar  │
│                                            │ (UI)     │
│          Tactical Viewport                 │          │
│                                            │ Build    │
│   ┌─────────────────────────────────┐      │ queue    │
│   │     World (zoomed)              │      │          │
│   │   terrain tiles, units,         │      │ Radar    │
│   │   buildings, animations,        │      │          │
│   │   health bars, selection        │      │ Power    │
│   └─────────────────────────────────┘      │ bar      │
│                                            │          │
└────────────────────────────────────────────┴──────────┘
```

| Region | Source | Zoom | Position |
|--------|--------|------|----------|
| Tab bar | SeenBuff row 0..TacPixelY | 1:1 always | Fixed top |
| Sidebar | SeenBuff col SideX..end | 1:1 always | Fixed right |
| Tactical viewport | HidPage tactical rect | Zoomed | Fixed position, content scrolls |
| Buttons/Messages | Drawn after zoom | 1:1 always | Over tactical area |

## Draw List

The draw list captures rendering commands during `Draw_It()` for analysis and future GPU replay.

### Captured Commands

| Command | Source Function | Layer | Notes |
|---------|----------------|-------|-------|
| `CMD_SHAPE` | `CC_Draw_Shape()` | LAYER_SPRITE | Units, buildings, animations, overlays, shadows |
| `CMD_STAMP` | `Draw_Stamp()` | LAYER_TERRAIN | Terrain template tiles (future) |
| `CMD_FILL_RECT` | `Fill_Rect()` | LAYER_OVERLAY | Health bars, shadow rects (future) |
| `CMD_DRAW_RECT` | `Draw_Rect()` | LAYER_OVERLAY | Selection boxes (future) |
| `CMD_DRAW_LINE` | `Draw_Line()` | LAYER_OVERLAY | Selection corners, aim lines (future) |
| `CMD_PUT_PIXEL` | `Put_Pixel()` | LAYER_OVERLAY | Debug markers (future) |

Currently only `CC_Draw_Shape` is hooked. Primitive hooks are defined in the draw list API for future use.

### Recording Flow

```
Render_Bridge_Begin_Draw_List()     ← g_draw_list.Clear(), SetRecording(true)
  │
  └─ Draw_It()
       ├─ CellClass::Draw_It()     ← Draw_Stamp (terrain) — not yet recorded
       ├─ obj->Render()
       │    ├─ Techno_Draw_Object() ← CC_Draw_Shape → recorded to draw list
       │    ├─ Draw health bar      ← Fill_Rect, Draw_Rect — direct to buffer
       │    └─ Draw selection       ← Draw_Line — direct to buffer
       └─ Redraw_Shadow()           ← CC_Draw_Shape → recorded to draw list

Render_Bridge_End_Draw_List()       ← SetRecording(false), apply zoom
```

### Zoom Application (Current)

When zoom ≠ 1.0, `Render_Bridge_End_Draw_List()` applies nearest-neighbor scaling to the tactical rectangle in HidPage in-place. This zooms everything Draw_It produced (world + in-game overlays). Buttons/Messages draw after at 1:1.

### Zoom Application (Future — Draw List Replay)

When the draw list captures ALL commands (including primitives):

1. Clear tactical area in HidPage
2. Replay LAYER_TERRAIN and LAYER_SPRITE with zoomed coordinates and scaled shapes
3. Replay LAYER_OVERLAY with zoomed coordinates but 1:1 sizes
4. Result: world scales, overlay text/bars stay readable

This requires intercepting Draw_Stamp and GraphicViewPortClass primitive methods (future work).

## Files

```
render_bridge/
  render_bridge.h          Public API
  render_bridge.cpp        Compositor init, passthrough blit
  present.cpp              TD_SDL_Present replacement
  init.cpp                 Game init hook
  zoom.cpp                 Scroll wheel input, viewport tracking, mouse transform
  zoom_tactical.cpp        Draw list begin/end, in-place tactical zoom
  draw_list.h              Draw command types and DrawList class
  draw_list.cpp            DrawList implementation
  draw_list_hooks.cpp      Recording hooks called from modern.src
  CMakeLists.txt           Source lists
  RENDERING.md             This document
```

## modern.src Changes (ifdef USE_RENDER_BRIDGE)

| File | Change | Lines |
|------|--------|-------|
| `gscreen.cpp` | Begin/End draw list hooks around Draw_It() | +8 |
| `conquer.cpp` | CC_Draw_Shape records to draw list | +5 |

Legacy build (`USE_RENDER_BRIDGE` not defined): zero change, code compiles identically.

## Build

```bash
make linux-cnc          # Legacy (no zoom, no draw list)
make bridge-cnc         # Bridge (zoom + draw list recording)
```

## Zoom Behavior

- **Scroll up**: zoom in (magnify tactical area, see less map)
- **Scroll down**: zoom out (return to 1:0)
- **Min zoom**: 1.0 (normal view)
- **Max zoom**: 4.0
- **Anchor**: mouse cursor position stays fixed during zoom
- **Sidebar/tab/buttons**: always 1:1, unaffected
- **Menu screens**: passthrough, no zoom

## Known Limitations

1. **In-game overlays zoom with world** — health bars, selection boxes, pips scale with zoom. This matches standard RTS behavior (SC2, AoE2 DE). Full separation requires intercepting primitive draw calls (future).

2. **Mouse accuracy** — coordinate transform from screen→game space needs refinement. Will improve with draw list replay approach.

3. **Zoom out not supported** — the game renders a fixed number of cells. Showing more map requires changing the tactical viewport dimensions (Set_View_Dimensions), which cascades into sidebar/window recalculation.
