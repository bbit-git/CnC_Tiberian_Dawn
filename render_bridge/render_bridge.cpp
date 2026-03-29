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

/// Copy a rectangular region from an 8-bit indexed buffer into an RGBA layer.
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

void Render_Bridge_Blit_Display()
{
    if (!g_bridge_active) return;

    // Source: SeenBuff (8-bit indexed, already blitted from HidPage by legacy Blit_Display)
    const uint8_t* seen = static_cast<const uint8_t*>(SeenBuff.Get_Buffer());
    if (!seen) return;
    int seen_pitch = SeenBuff.Get_Full_Pitch();

    // Update palette LUT
    const uint8_t* pal = static_cast<const uint8_t*>((void*)Get_Palette());
    if (!pal) pal = GamePalette;
    if (!pal) return;
    update_palette_lut(pal);

    // Copy tactical area → WORLD_TERRAIN layer
    {
        uint8_t* layer = static_cast<uint8_t*>(
            g_compositor.Get_Layer_Buffer(RenderLayerID::WORLD_TERRAIN));
        int lw = g_compositor.Get_Layer_Width(RenderLayerID::WORLD_TERRAIN);
        int lh = g_compositor.Get_Layer_Height(RenderLayerID::WORLD_TERRAIN);
        if (layer && lw > 0 && lh > 0) {
            copy_indexed_region(seen, seen_pitch,
                                Map.TacPixelX, Map.TacPixelY,
                                layer, lw, lh);
            g_compositor.Invalidate_Layer(RenderLayerID::WORLD_TERRAIN);
        }
    }

    // Copy sidebar → UI_SIDEBAR layer
    {
        uint8_t* layer = static_cast<uint8_t*>(
            g_compositor.Get_Layer_Buffer(RenderLayerID::UI_SIDEBAR));
        int lw = g_compositor.Get_Layer_Width(RenderLayerID::UI_SIDEBAR);
        int lh = g_compositor.Get_Layer_Height(RenderLayerID::UI_SIDEBAR);
        if (layer && lw > 0 && lh > 0) {
            copy_indexed_region(seen, seen_pitch,
                                Map.SideX, 0,
                                layer, lw, lh);
            g_compositor.Invalidate_Layer(RenderLayerID::UI_SIDEBAR);
        }
    }

    // Copy tab bar → UI_TAB layer
    {
        uint8_t* layer = static_cast<uint8_t*>(
            g_compositor.Get_Layer_Buffer(RenderLayerID::UI_TAB));
        int lw = g_compositor.Get_Layer_Width(RenderLayerID::UI_TAB);
        int lh = g_compositor.Get_Layer_Height(RenderLayerID::UI_TAB);
        if (layer && lw > 0 && lh > 0) {
            copy_indexed_region(seen, seen_pitch,
                                0, 0,
                                layer, lw, lh);
            g_compositor.Invalidate_Layer(RenderLayerID::UI_TAB);
        }
    }

    // Composite all layers
    g_compositor.Composite();
}

void Render_Bridge_Set_Zoom(float zoom)
{
    g_compositor.Set_World_Zoom(zoom);
}

float Render_Bridge_Get_Zoom()
{
    return g_compositor.Get_World_Zoom();
}

RenderCompositor* Render_Bridge_Get_Compositor()
{
    return &g_compositor;
}
