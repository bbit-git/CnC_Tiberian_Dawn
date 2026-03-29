# Render Bridge — Buffer Architecture

## Buffer Inventory

### Game Engine Buffers (always present)

| Buffer | Size | Format | Allocated |
|--------|------|--------|-----------|
| **HidPage** | 712×400 | 8-bit indexed | Startup (sdl3_entry.cpp) |
| **SeenBuff** | 712×400 | 8-bit indexed | Startup (viewport into VisiblePage) |
| **HiddenPage** | 712×400 | 8-bit indexed | Startup (backing store for HidPage) |
| **VisiblePage** | 712×400 | 8-bit indexed | Startup (backing store for SeenBuff) |
| **SysMemPage** | 320×200 | 8-bit indexed | Startup (legacy system memory page) |

### Render Bridge Buffers (bridge build only)

| Buffer | Size | Format | Allocated |
|--------|------|--------|-----------|
| **Native tactical** | min(screen,map) per axis | 8-bit indexed | Per scenario, grown as needed |
| **GL indexed texture** | 712×400 | GL_LUMINANCE | GL context init |
| **GL palette texture** | 256×1 | GL_RGB | GL context init |
| **GL native tactical texture** | Variable | GL_LUMINANCE | When zoom < default |
| **GL HUD texture** | 128×64 | GL_RGBA | First debug frame |
| **GL atlas page textures** | 2048×2048 × N | GL_RGBA | First gameplay frame |
| **GL VQA texture** | VQA resolution | GL_RGBA | During VQA playback |

## Buffer Purposes

### HidPage (712×400, 8-bit indexed)

Engine's off-screen rendering target. `Set_Logic_Page(HidPage)` directs all drawing here during `GScreenClass::Render()`.

**Written by:**
- Draw_It() chain: sidebar, tab bar, radar, power bar (direct rendering)
- Draw list replay: terrain stamps + sprites (replayed after Draw_It)
- Buttons, Messages, ActionMenu (drawn after draw list replay)

**Read by:**
- `Blit_Display()` copies entire buffer to SeenBuff

### SeenBuff (712×400, 8-bit indexed)

The "visible" frame buffer. Contains the final composited 8-bit frame ready for presentation.

**Written by:**
- `Blit_Display()` copies from HidPage

**Read by:**
- Legacy: CPU palette LUT → RGBA → SDL_UpdateTexture
- Bridge GL: uploaded as GL indexed texture each frame
- Bridge SDL fallback: CPU palette LUT → RGBA → SDL_UpdateTexture

### Native Tactical Buffer (variable, 8-bit indexed)

High-resolution tactical area for zoom-out. Sized per scenario as `min(screen_resolution, map_pixel_size)` per axis.

**Example sizes:**
- 58×49 cell map on 1920×1080: buffer = 1392×1080 (58 cells × 24px, capped by screen height)
- 20×20 cell map on 1920×1080: buffer = 480×480 (entire map fits)
- 128×128 cell map on 1920×1080: buffer = 1920×1080 (screen-limited)

**Written by:**
- Draw list replay to temporary GraphicBufferClass viewport
- Only filled when zoom < default (more cells needed than 712×400 provides)

**Read by:**
- `GL_Present_Upload_Tactical()` uploads to GL native tactical texture

### GL Indexed Texture (712×400, GL_LUMINANCE)

SeenBuff on the GPU. Each byte = palette index (0-255). Palette shader converts to RGB.

**Written by:**
- `glTexSubImage2D` from SeenBuff pixels each frame

**Read by:**
- Palette fragment shader for sidebar, tab bar, and menu quads
- At default zoom: also used for tactical quad (no native texture)

### GL Palette Texture (256×1, GL_RGB)

VGA palette converted from 6-bit (0-63) to 8-bit (0-252) on the GPU.

**Written by:**
- `glTexSubImage2D` every frame (palette fades modify data in-place)

**Read by:**
- Palette fragment shader: `idx = texture2D(indexed).r → color = texture2D(palette, idx)`

### GL Native Tactical Texture (variable, GL_LUMINANCE)

Native-resolution tactical area on the GPU. Same format as the indexed texture but larger.

**Written by:**
- `GL_Present_Upload_Tactical()` from native tactical buffer

**Read by:**
- Tactical GL quad when zoom < default (replaces indexed texture for that quad)

**Active:** Only when zoom < default zoom (zoomed out, more cells visible)

### GL HUD Texture (128×64, GL_RGBA)

Debug overlay bitmap rendered at screen resolution. Semi-transparent black background with colored text.

**Written by:**
- `debug_hud.cpp` renders stats to RGBA pixel array each frame

**Read by:**
- RGBA shader quad in bottom-left corner of screen

**Active:** Debug builds only. Rendered as final GL overlay before SwapWindow.

### GL Atlas Page Textures (2048×2048 × N, GL_RGBA)

Sprite atlas for future GPU sprite rendering. Sprites decoded via ISpriteProvider, converted to RGBA with current palette, packed by TextureAtlas.

**Written by:**
- `GL_Sprites_Build_Atlas()` on first gameplay frame and palette changes

**Read by:**
- Currently unused (GL sprite overlay disabled — CPU replay handles sprites correctly with house colors and all rendering modes)

**Rebuild triggers:** Palette content change (768-byte memcmp), new shapes appearing

### GL VQA Texture (variable, GL_RGBA)

VQA video frame on the GPU. Used when SDL_Renderer is unavailable (bridge build).

**Written by:**
- `SDL3VideoRenderer::present_frame_gl()` converts indexed VQA frame to RGBA

**Read by:**
- Simple RGBA shader quad, letterboxed to maintain aspect ratio

**Active:** Only during VQA cutscene playback

## Mode Comparison

### Legacy Mode (`make linux-cnc`)

```
Draw_It() → HidPage (8-bit)
Blit_Display() → SeenBuff (8-bit)
TD_SDL_Present():
  SeenBuff → CPU palette LUT → RGBA buffer
  → SDL_UpdateTexture → SDL_RenderPresent

Buffers used: HidPage, SeenBuff, VisiblePage, HiddenPage
GL buffers: none
```

### Bridge GL Mode (`make bridge-cnc`, default zoom)

```
Begin_Draw_List() → recording ON
Draw_It():
  Sidebar/tab/radar → HidPage directly
  CC_Draw_Shape(WINDOW_TACTICAL) → draw list (deferred)
  Draw_Stamp(WINDOW_TACTICAL) → draw list (deferred)
End_Draw_List():
  Replay draw list → HidPage (terrain + sprites at 712×400)
Buttons/Messages → HidPage
Blit_Display() → SeenBuff

TD_SDL_Present() → GL_Present_Frame():
  SeenBuff → GL indexed texture (glTexSubImage2D)
  Palette → GL palette texture (glTexSubImage2D)
  Tab quad:      indexed texture → palette shader → screen
  Tactical quad: indexed texture → palette shader → screen
  Sidebar quad:  indexed texture → palette shader → screen
  Debug HUD:     HUD texture → RGBA shader → screen
  SDL_GL_SwapWindow

Buffers used: HidPage, SeenBuff, GL indexed, GL palette, GL HUD
```

### Bridge GL Mode (zoomed out, more cells)

```
Begin_Draw_List():
  Expand TacLeptonWidth/Height to native size
  Recording ON
Draw_It():
  Game iterates more cells (expanded tactical area)
  CC_Draw_Shape/Draw_Stamp → draw list
End_Draw_List():
  Restore tactical dimensions
  Replay to native tactical buffer (e.g. 1392×1080)
  Upload to GL native tactical texture
  Replay to HidPage (712×400 for sidebar/tab)
Buttons/Messages → HidPage
Blit_Display() → SeenBuff

GL_Present_Frame():
  SeenBuff → GL indexed texture
  Tab quad:      indexed texture → palette shader
  Tactical quad: NATIVE texture → palette shader (more cells visible!)
  Sidebar quad:  indexed texture → palette shader
  Debug HUD → RGBA overlay
  SDL_GL_SwapWindow

Buffers used: HidPage, SeenBuff, native tactical, GL indexed,
              GL palette, GL native tactical, GL HUD
```

### Bridge SDL Fallback (GL unavailable)

```
Same as Legacy but with draw list recording/replay.
SDL_Renderer created as fallback when GL_Present_Init fails.

Draw_It() → draw list → replay to HidPage
Blit_Display() → SeenBuff
TD_SDL_Present():
  SeenBuff → CPU palette LUT → RGBA
  → SDL_UpdateTexture → SDL_RenderPresent

Buffers used: HidPage, SeenBuff
GL buffers: none
```

### VQA Playback (bridge build)

```
VQA decoder → indexed frame + palette
present_frame_gl():
  Indexed → RGBA conversion (CPU)
  → GL VQA texture (glTexSubImage2D)
  → RGBA shader quad, letterboxed
  → SDL_GL_SwapWindow

Buffers used: GL VQA texture
```

## Data Flow Diagram

```
                    ┌─────────────────────────────────────────────────┐
                    │              GScreenClass::Render()              │
                    └────────────────────┬────────────────────────────┘
                                         │
                    ┌────────────────────▼────────────────────────────┐
                    │  Begin_Draw_List (expand tac if zoom < default) │
                    └────────────────────┬────────────────────────────┘
                                         │
              ┌──────────────────────────▼──────────────────────────┐
              │                     Draw_It()                       │
              │  ┌─────────────┐  ┌──────────────┐  ┌───────────┐  │
              │  │ Sidebar/Tab │  │ CC_Draw_Shape │  │Draw_Stamp │  │
              │  │  → HidPage  │  │  → draw list  │  │→ draw list│  │
              │  └─────────────┘  └──────────────┘  └───────────┘  │
              └────────────────────────┬────────────────────────────┘
                                       │
              ┌────────────────────────▼────────────────────────────┐
              │              End_Draw_List (restore tac)            │
              │                                                     │
              │  ┌─ if zoom < default ────────────────────────────┐ │
              │  │ Replay → native buffer → GL_Present_Upload     │ │
              │  └────────────────────────────────────────────────┘ │
              │  Replay → HidPage (712×400)                        │
              └────────────────────────┬────────────────────────────┘
                                       │
              ┌────────────────────────▼────────────────────────────┐
              │  Buttons / Messages / ActionMenu → HidPage          │
              └────────────────────────┬────────────────────────────┘
                                       │
              ┌────────────────────────▼────────────────────────────┐
              │  Blit_Display(): HidPage → SeenBuff                 │
              └────────────────────────┬────────────────────────────┘
                                       │
              ┌────────────────────────▼────────────────────────────┐
              │  GL_Present_Frame()                                 │
              │  ┌──────────┐  ┌───────────┐  ┌──────────┐         │
              │  │ Tab quad │  │  Tactical  │  │ Sidebar  │         │
              │  │ SeenBuff │  │  quad      │  │ SeenBuff │         │
              │  │ palette  │  │  native OR │  │ palette  │         │
              │  │ shader   │  │  SeenBuff  │  │ shader   │         │
              │  └──────────┘  └───────────┘  └──────────┘         │
              │  ┌──────────┐                                       │
              │  │ HUD quad │  (debug overlay, RGBA)                │
              │  └──────────┘                                       │
              │  SDL_GL_SwapWindow → screen                         │
              └─────────────────────────────────────────────────────┘
```
