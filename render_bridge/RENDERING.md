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

## Debug HUD And Overlay

The GL bridge renders a debug HUD in the top-left corner and a screen-space
debug rectangle overlay on top of the frame. `F8` dumps the same values to the
debug log.

### HUD fields

| Field | Meaning |
|-------|---------|
| `NAT WxH` | Native tactical texture size in pixels. This is the offscreen GL world buffer. |
| `TAC WxH` | Legacy tactical viewport size in game-buffer pixels after tab/sidebar layout. |
| `Z n.nn` | Current GL world zoom. `1.00` is the outermost world view. Higher values zoom in. |
| `VIS WxH` | Visible source rectangle size inside `NAT` after zoom/aspect fitting. |
| `VP X-Y` | Top-left of the visible source rectangle inside `NAT`, in native texture pixels. |
| `VP MAX X-Y` | Maximum legal viewport travel inside `NAT`. `0-0` means the whole native texture is visible. |
| `TC X-Y` | Tactical world origin relative to map origin, in pixels, from `Map.TacticalCoord`. |
| `RNG X-Y` | Maximum tactical-coordinate travel range from the legacy engine viewport model. |
| `WORLD X-Y` | Effective world top-left shown by the GL bridge. Roughly `TC + VP`. |
| `BND LRTB` | World clamp flags. `L/R/T/B` show that the map edge is reached on that side. `.` means unclamped. |
| `MSE LRTB` | Mouse-edge flags inside the tactical viewport after mouse transform. |
| `DL N` | Total draw-list commands recorded this frame. |
| `S N` | `CMD_SHAPE` count in the draw list. |
| `T N` | `CMD_STAMP` count in the draw list. |
| `P N` | Primitive overlay command count in the draw list. |
| `ATL NF NP` | Atlas cache totals: cached frames and atlas pages. |
| `ATL NEW N` | New atlas entries built this frame. |
| `GL S N` | Sprites rendered by the experimental GL sprite overlay. |
| `D N` | GL sprite batch draw calls. |
| `CPU S N` | Sprite draws that fell back to legacy CPU replay. |
| `GL P N` | Overlay primitives rendered by the GL primitive pass. |
| `MAP WxH` | Map dimensions in cells. |
| `SCR WxH` | Output screen resolution in pixels. |
| `MSE X-Y` | Current transformed in-game mouse position in game-buffer coordinates. |

### Flag details

- `BND LRTB`: scroll clamp at map boundaries.
- `MSE LRTB`: transformed mouse touching tactical edges used for edge scrolling.
- `WORLD`: useful when `TC` and `VP` disagree and the bridge is showing the wrong world origin.

### Overlay color legend

| Label | Color | Meaning |
|-------|-------|---------|
| `TAC` | Green | Tactical presentation area on screen. |
| `VP` | Cyan | Current visible viewport within the native tactical texture. Rendered in the minimap box. |
| `HDR` | Yellow | Header/tab strip. |
| `SID` | Blue | Sidebar region. |
| `MSE` | White | Mouse clamp region and transformed gameplay mouse marker. |
| `SCRL` | Red | Edge-scroll bands. Bright red indicates a currently clamped map edge. |
| `RAW` | Orange | Raw mouse marker before render-bridge transform. |

### Overlay shapes

- Green rectangle: tactical screen region.
- Yellow rectangle: tab/header strip.
- Blue rectangle: sidebar region.
- White inset rectangle: tactical mouse clamp/input region.
- Red translucent bands: edge-scroll trigger zones.
- Minimap box: native tactical texture extents.
- Cyan box inside minimap: current visible viewport sampled from the native texture.
- Orange square: raw SDL mouse position mapped into screen space.
- White square: transformed gameplay mouse position mapped into screen space.

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
3. ✓ Experimental: CMD_SHAPE can be routed through `LegacySpriteProvider` behind `USE_RENDER_BRIDGE_GL_SPRITES`
4. ✓ Experimental: SpriteFrames can be packed into `TextureAtlas` behind `USE_RENDER_BRIDGE_GL_SPRITES`
5. ✓ Experimental: GL sprite replay can overlay atlas-backed quads behind `USE_RENDER_BRIDGE_GL_SPRITES`
6. ✓ GL palette shader for terrain (CMD_STAMP → indexed texture)
7. ✓ GL primitives for overlays (lines, rects, pixels) in the GL present pass

Notes:
- Unsupported or unclassified sprite effects still fall back to the legacy CPU replay path.
- `USE_RENDER_BRIDGE_GL_SPRITES` remains experimental until runtime visual parity is verified.
