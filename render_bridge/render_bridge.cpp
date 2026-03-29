/**
 * render_bridge.cpp ��� Compositor setup and layer management for TD.
 *
 * Reads game layout (TacPixelX/Y, SideX, SideBarWidth) and configures
 * compositor layers to match. The legacy Draw_It() chain and Blit_Display()
 * are unchanged — they still render into HidPage and copy to SeenBuff.
 *
 * The bridge reads SeenBuff (after Blit_Display), splits it into compositor
 * layers by screen region, and presents via the compositor path.
 */

#include "render_bridge.h"
#include "function.h"

static RenderCompositor g_compositor;
static bool             g_bridge_active = false;
static int              g_output_w = 0;
static int              g_output_h = 0;

// Palette LUT for 8-bit → RGBA conversion
static uint32_t         g_pal_lut[256];
static const uint8_t*   g_pal_lut_src = nullptr;
static uint32_t         g_pal_lut_hash = 0;

static void update_palette_lut(const uint8_t* pal)
{
    uint32_t hash = pal[0] | (pal[3] << 8) | (pal[384] << 16) | (pal[765] << 24);
    if (pal == g_pal_lut_src && hash == g_pal_lut_hash) return;

    for (int i = 0; i < 256; i++) {
        uint8_t r = pal[i * 3 + 0] << 2;
        uint8_t g = pal[i * 3 + 1] << 2;
        uint8_t b = pal[i * 3 + 2] << 2;
        g_pal_lut[i] = (0xFFu << 24) | (b << 16) | (g << 8) | r;
    }
    g_pal_lut_src = pal;
    g_pal_lut_hash = hash;
}

/// Copy a rectangular region from an 8-bit indexed buffer into an RGBA layer (1:1).
static void copy_indexed_region(
    const uint8_t* src, int src_pitch,
    int src_x, int src_y,
    uint8_t* dst_rgba, int dst_w, int dst_h)
{
    for (int row = 0; row < dst_h; row++) {
        const uint8_t* sp = src + (src_y + row) * src_pitch + src_x;
        uint32_t* dp = reinterpret_cast<uint32_t*>(dst_rgba + row * dst_w * 4);
        for (int col = 0; col < dst_w; col++) {
            dp[col] = g_pal_lut[sp[col]];
        }
    }
}

/// Copy a viewport sub-region with nearest-neighbor zoom into an RGBA layer.
/// viewport_x/y are source-space coordinates of the visible region's top-left.
/// zoom < 1.0 = zoomed out (source shrinks, black borders), zoom > 1.0 = zoomed in.
static void copy_indexed_zoomed(
    const uint8_t* src, int src_pitch,
    int src_origin_x, int src_origin_y,   // tactical area origin in SeenBuff
    int src_w, int src_h,                  // tactical area dimensions
    float viewport_x, float viewport_y,   // top-left of visible region in source space
    float zoom,
    uint8_t* dst_rgba, int dst_w, int dst_h)
{
    static constexpr uint32_t BLACK = 0xFF000000u;

    for (int row = 0; row < dst_h; row++) {
        uint32_t* dp = reinterpret_cast<uint32_t*>(dst_rgba + row * dst_w * 4);
        int sy = static_cast<int>(viewport_y + static_cast<float>(row) / zoom);

        if (sy < 0 || sy >= src_h) {
            for (int col = 0; col < dst_w; col++) dp[col] = BLACK;
            continue;
        }

        const uint8_t* sp = src + (src_origin_y + sy) * src_pitch + src_origin_x;

        for (int col = 0; col < dst_w; col++) {
            int sx = static_cast<int>(viewport_x + static_cast<float>(col) / zoom);
            if (sx < 0 || sx >= src_w) {
                dp[col] = BLACK;
            } else {
                dp[col] = g_pal_lut[sp[sx]];
            }
        }
    }
}

bool Render_Bridge_Init(int output_width, int output_height)
{
    g_output_w = output_width;
    g_output_h = output_height;

    g_compositor.Init(output_width, output_height);
    Render_Bridge_Update_Layout();

    g_bridge_active = true;
    return true;
}

void Render_Bridge_Shutdown()
{
    g_bridge_active = false;
}

void Render_Bridge_Update_Layout()
{
    int screen_w = g_output_w;
    int screen_h = g_output_h;

    // Tactical area (world view)
    int tac_x = Map.TacPixelX;
    int tac_y = Map.TacPixelY;
    int tac_w = Lepton_To_Pixel(Map.TacLeptonWidth);
    int tac_h = Lepton_To_Pixel(Map.TacLeptonHeight);

    // Clamp to screen bounds
    if (tac_w > screen_w - tac_x) tac_w = screen_w - tac_x;
    if (tac_h > screen_h - tac_y) tac_h = screen_h - tac_y;

    // Sidebar area
    int side_x = Map.SideX;
    int side_w = Map.SideBarWidth;
    int side_h = screen_h;

    // World terrain layer — tactical area
    if (tac_w > 0 && tac_h > 0) {
        RenderLayerDesc world = {};
        world.id       = RenderLayerID::WORLD_TERRAIN;
        world.width    = tac_w;
        world.height   = tac_h;
        world.zoom     = 1.0f;
        world.offset_x = tac_x;
        world.offset_y = tac_y;
        world.dirty    = true;
        g_compositor.Configure_Layer(world);
    }

    // UI sidebar layer
    if (side_w > 0) {
        RenderLayerDesc sidebar = {};
        sidebar.id       = RenderLayerID::UI_SIDEBAR;
        sidebar.width    = side_w;
        sidebar.height   = side_h;
        sidebar.zoom     = 1.0f;
        sidebar.offset_x = side_x;
        sidebar.offset_y = 0;
        sidebar.dirty    = true;
        g_compositor.Configure_Layer(sidebar);
    }

    // Tab bar — top strip above tactical area
    if (tac_y > 0) {
        RenderLayerDesc tab = {};
        tab.id       = RenderLayerID::UI_TAB;
        tab.width    = screen_w;
        tab.height   = tac_y;
        tab.zoom     = 1.0f;
        tab.offset_x = 0;
        tab.offset_y = 0;
        tab.dirty    = true;
        g_compositor.Configure_Layer(tab);
    }
}

/// Passthrough: convert full SeenBuff to RGBA without compositor (menus, dialogs).
static void blit_passthrough()
{
    const uint8_t* seen = static_cast<const uint8_t*>(SeenBuff.Get_Buffer());
    if (!seen) return;
    int w = SeenBuff.Get_Width();
    int h = SeenBuff.Get_Height();
    int pitch = SeenBuff.Get_Full_Pitch();

    const uint8_t* pal = static_cast<const uint8_t*>((void*)Get_Palette());
    if (!pal) pal = GamePalette;
    if (!pal) return;
    update_palette_lut(pal);

    // Write directly to compositor output (full screen, 1:1)
    uint8_t* out = static_cast<uint8_t*>(
        const_cast<void*>(g_compositor.Get_Output()));
    int ow = g_compositor.Get_Output_Width();
    int oh = g_compositor.Get_Output_Height();
    if (!out || ow <= 0 || oh <= 0) return;

    int rows = (h < oh) ? h : oh;
    int cols = (w < ow) ? w : ow;
    for (int row = 0; row < rows; row++) {
        const uint8_t* sp = seen + row * pitch;
        uint32_t* dp = reinterpret_cast<uint32_t*>(out + row * ow * 4);
        for (int col = 0; col < cols; col++) {
            dp[col] = g_pal_lut[sp[col]];
        }
    }
}

void Render_Bridge_Blit_Display()
{
    if (!g_bridge_active) return;

    // Zoom is now applied in-place to HidPage by Render_Bridge_Zoom_Tactical()
    // (called from GScreenClass::Render between Draw_It and Buttons).
    // By the time we get here, SeenBuff already has the zoomed world + 1:1 UI.
    // Just passthrough.
    blit_passthrough();
}

void Render_Bridge_Set_Zoom(float zoom)
{
    // Zoom is applied during the SeenBuff → layer copy (copy_indexed_zoomed),
    // not by the compositor. The compositor always blits layers 1:1.
    (void)zoom;
}

float Render_Bridge_Get_Zoom()
{
    return g_compositor.Get_World_Zoom();
}

RenderCompositor* Render_Bridge_Get_Compositor()
{
    return &g_compositor;
}
