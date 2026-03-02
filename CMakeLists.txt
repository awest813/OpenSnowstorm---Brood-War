cmake_minimum_required(VERSION 3.1)
project(OpenSnowstorm CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(OPENSNOWSTORM_BUILD_UI
    "Build the SDL2 UI library and gfxtest executable (requires SDL2)" ON)

option(OPENSNOWSTORM_BUILD_BWAPI
    "Build the mini-openbwapi BWAPI compatibility wrapper" ON)

# ---------------------------------------------------------------------------
# SDL2 UI + gfxtest executable
# ---------------------------------------------------------------------------
if(OPENSNOWSTORM_BUILD_UI)
    find_package(SDL2 REQUIRED)

    add_definitions(-DOPENBW_NO_SDL_IMAGE)
    add_definitions(-DOPENBW_NO_SDL_MIXER)
    add_definitions(-DOPENBW_ENABLE_UI)

    # SDL2 backend library (shared between gfxtest and any embedder)
    add_library(openbw_ui ui/sdl2.cpp)
    target_include_directories(openbw_ui PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/ui
        ${SDL2_INCLUDE_DIRS}
    )
    target_link_libraries(openbw_ui ${SDL2_LIBRARIES})
    set_property(TARGET openbw_ui PROPERTY POSITION_INDEPENDENT_CODE ON)

    # Main replay-viewer / benchmark / validate-replay executable
    add_executable(gfxtest ui/gfxtest.cpp)
    target_include_directories(gfxtest PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/ui
        ${SDL2_INCLUDE_DIRS}
    )
    target_link_libraries(gfxtest openbw_ui ${SDL2_LIBRARIES})
endif()

# ---------------------------------------------------------------------------
# mini-openbwapi (BWAPI-compatible C wrapper)
# ---------------------------------------------------------------------------
if(OPENSNOWSTORM_BUILD_BWAPI)
    add_subdirectory(mini-openbwapi)
endif()
