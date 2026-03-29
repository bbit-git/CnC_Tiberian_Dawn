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

set(RENDER_BRIDGE_DIR ${CMAKE_SOURCE_DIR}/engine/cnc/render_bridge)

set(CNC_RENDER_BRIDGE_SRCS
    ${RENDER_BRIDGE_DIR}/render_bridge.cpp
    ${RENDER_BRIDGE_DIR}/init.cpp
)

# This file provides TD_SDL_Present() and Shake_Screen() — replaces sdl3_present.cpp
set(CNC_RENDER_BRIDGE_PRESENT_SRC
    ${RENDER_BRIDGE_DIR}/present.cpp
)
