cmake_minimum_required(VERSION 3.21)
project(cnc_td_modern LANGUAGES CXX)

find_package(SDL3 QUIET CONFIG)
if(NOT SDL3_FOUND)
  message(STATUS "SDL3 not found — cnc_td_modern will be unavailable.")
  return()
endif()

# Collect all passing .cpp files (exclude the 6 that don't compile yet)
set(TD_EXCLUDED_SOURCES
  debug.cpp
  ioobj.cpp
  mplayer.cpp
  queue.cpp
  score.cpp
  stats.cpp
)

file(GLOB TD_ALL_SOURCES "*.cpp")
set(TD_ENGINE_SOURCES "")
foreach(src IN LISTS TD_ALL_SOURCES)
  get_filename_component(name "${src}" NAME)
  if(NOT name IN_LIST TD_EXCLUDED_SOURCES
     AND NOT name STREQUAL "td_sdl_entry.cpp"
     AND NOT name STREQUAL "td_stubs.cpp"
     AND NOT name STREQUAL "check.sh")
    list(APPEND TD_ENGINE_SOURCES "${src}")
  endif()
endforeach()

# Build the engine as a static library
add_library(cnc_td_engine STATIC
  ${TD_ENGINE_SOURCES}
  td_stubs.cpp
)

target_compile_features(cnc_td_engine PRIVATE cxx_std_20)
target_include_directories(cnc_td_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(cnc_td_engine PRIVATE
  TRUE_FALSE_DEFINED=1
  NOMINMAX=1
)
target_compile_options(cnc_td_engine PRIVATE
  -fpermissive
  -Wno-unknown-pragmas
  -Wno-deprecated
  -Wno-write-strings
  -Wno-narrowing
  -Wno-register
  -Wno-return-type
  -Wno-sign-compare
  -Wno-unused-variable
  -Wno-address
  -Wno-unused-but-set-variable
  -Wno-conversion-null
  -Wno-deprecated-enum-enum-conversion
)

# Build the SDL3 entry point executable
add_executable(cnc_td_sdl
  td_sdl_entry.cpp
)

target_compile_features(cnc_td_sdl PRIVATE cxx_std_20)
target_include_directories(cnc_td_sdl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(cnc_td_sdl PRIVATE
  TRUE_FALSE_DEFINED=1
  NOMINMAX=1
)
target_compile_options(cnc_td_sdl PRIVATE
  -fpermissive
  -Wno-unknown-pragmas
  -Wno-deprecated
  -Wno-write-strings
  -Wno-narrowing
)
target_link_libraries(cnc_td_sdl PRIVATE
  cnc_td_engine
  SDL3::SDL3
)
