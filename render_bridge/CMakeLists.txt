# engine/cnc/render_bridge — Compositor-based rendering integration for TD.
#
# When USE_RENDER_BRIDGE=ON:
#   - Adds bridge sources to cnc_td
#   - Replaces sdl3_present.cpp with render_bridge/present.cpp
#   - Links render_core library
#   - Defines USE_RENDER_BRIDGE preprocessor symbol
#
# The legacy modern.src/ code is fully preserved — only the SDL present
# function is swapped out. Blit_Display() and Draw_It() chain are unchanged.

set(RENDER_BRIDGE_DIR ${CMAKE_SOURCE_DIR}/Engine/EA/cnc/render_bridge)

set(CNC_RENDER_BRIDGE_SRCS
    ${RENDER_BRIDGE_DIR}/render_bridge.cpp
    ${RENDER_BRIDGE_DIR}/init.cpp
    ${RENDER_BRIDGE_DIR}/zoom.cpp
    ${RENDER_BRIDGE_DIR}/zoom_tactical.cpp
    ${RENDER_BRIDGE_DIR}/draw_list.cpp
    ${RENDER_BRIDGE_DIR}/draw_list_hooks.cpp
    ${RENDER_BRIDGE_DIR}/gl_present.cpp
    ${RENDER_BRIDGE_DIR}/gl_present_regions.cpp
    ${RENDER_BRIDGE_DIR}/gl_present_debug.cpp
    ${RENDER_BRIDGE_DIR}/gl_present_cursor.cpp
    ${RENDER_BRIDGE_DIR}/gl_primitives.cpp
    ${RENDER_BRIDGE_DIR}/gl_sprites.cpp
    ${RENDER_BRIDGE_DIR}/sprite_render.cpp
    ${RENDER_BRIDGE_DIR}/stamp_render.cpp
    ${RENDER_BRIDGE_DIR}/ui_overlay.cpp
    ${RENDER_BRIDGE_DIR}/debug_hud.cpp
    ${RENDER_BRIDGE_DIR}/ui/gl_ui.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_controls.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_dialog.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_game_options.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_load_dialog.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_load_dialog_state.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_game_controls.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_visual_controls.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_sound_controls.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_special_dialog.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_expansion_dialog.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_com_scenario.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_main_menu.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_main_menu_options.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_tooltip.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_text.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_text_sdf.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_messages.cpp
    ${RENDER_BRIDGE_DIR}/ui/commandbar_atlas.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_sidebar.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_radar.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_draw_list.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_header.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_help.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_action_menu.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_layout.cpp
    ${RENDER_BRIDGE_DIR}/ui/ui_input.cpp
    ${RENDER_BRIDGE_DIR}/ui/hd_sidebar_layout.cpp
)

# Embed sidebar-layout.json into the binary as kSidebarLayoutJson.
set(SIDEBAR_LAYOUT_JSON ${RENDER_BRIDGE_DIR}/assets/sidebar-layout.json)
set(SIDEBAR_LAYOUT_GENERATED
    ${CMAKE_CURRENT_BINARY_DIR}/generated/sidebar_layout_json.cpp)
add_custom_command(
    OUTPUT  ${SIDEBAR_LAYOUT_GENERATED}
    COMMAND ${CMAKE_COMMAND}
            -DIN=${SIDEBAR_LAYOUT_JSON}
            -DOUT=${SIDEBAR_LAYOUT_GENERATED}
            -DSYMBOL=kSidebarLayoutJson
            -P ${CMAKE_SOURCE_DIR}/cmake/embed_text.cmake
    DEPENDS ${SIDEBAR_LAYOUT_JSON}
            ${CMAKE_SOURCE_DIR}/cmake/embed_text.cmake
    COMMENT "Embedding sidebar-layout.json as kSidebarLayoutJson")
list(APPEND CNC_RENDER_BRIDGE_SRCS ${SIDEBAR_LAYOUT_GENERATED})

# This file provides TD_SDL_Present() and Shake_Screen() — replaces sdl3_present.cpp
set(CNC_RENDER_BRIDGE_PRESENT_SRC
    ${RENDER_BRIDGE_DIR}/present.cpp
)

list(APPEND CNC_RENDER_BRIDGE_SRCS ${RENDER_LEGACY_PROVIDER_SRC})
