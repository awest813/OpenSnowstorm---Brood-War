cmake_minimum_required(VERSION 3.10)
project(OpenSnowstorm CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Common Compile Options & Strictness
# ---------------------------------------------------------------------------
function(opensnowstorm_apply_common_compile_options target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE 
            /bigobj 
            /W4       # Warning level 4
            /MP       # Multi-processor build
            /utf-8    # UTF-8 source/execution charset
            /D_CRT_SECURE_NO_WARNINGS # Silence fopen/scanf warnings
        )
    else()
        target_compile_options(${target_name} PRIVATE 
            -Wall 
            -Wextra 
            -Wno-unused-parameter
            -fvisibility=hidden
        )
    endif()
endfunction()

function(opensnowstorm_append_target_flags target_name property_name flags)
    get_target_property(existing_flags ${target_name} ${property_name})
    if(NOT existing_flags OR existing_flags STREQUAL "existing_flags-NOTFOUND")
        set(existing_flags "")
    endif()
    set_target_properties(${target_name} PROPERTIES ${property_name} "${existing_flags} ${flags}")
endfunction()

function(opensnowstorm_apply_emscripten_sdl_flags target_name)
    opensnowstorm_append_target_flags(${target_name} COMPILE_FLAGS
        " -sUSE_SDL=2 -sUSE_SDL_MIXER=2")
    opensnowstorm_append_target_flags(${target_name} LINK_FLAGS
        " -sUSE_SDL=2 -sUSE_SDL_MIXER=2")
endfunction()

function(opensnowstorm_apply_emscripten_executable_defaults target_name)
    opensnowstorm_apply_emscripten_sdl_flags(${target_name})
    opensnowstorm_append_target_flags(${target_name} LINK_FLAGS
        " --bind -sALLOW_MEMORY_GROWTH=1 -sFORCE_FILESYSTEM=1 -sWASM=1 -sASSERTIONS=1")
endfunction()

if(MSVC)
    add_compile_options(/bigobj)
endif()

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(OPENSNOWSTORM_BUILD_UI
    "Build the SDL2 UI library and gfxtest executable (requires SDL2)" ON)

option(OPENSNOWSTORM_BUILD_BWAPI
    "Build the mini-openbwapi BWAPI compatibility wrapper" ON)

enable_testing()
add_test(NAME DeterminismCheck COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} python scripts/regression_check.py)

# ---------------------------------------------------------------------------
# SDL2 UI + gfxtest executable
# ---------------------------------------------------------------------------
if(OPENSNOWSTORM_BUILD_UI)
    if(NOT EMSCRIPTEN)
        find_package(SDL2 REQUIRED)
        find_package(SDL2_mixer REQUIRED)

        # Normalize SDL2 link targets across config-package versions.
        # Modern SDL2 / SDL2_mixer config packages only export the imported
        # targets (SDL2::SDL2, SDL2_mixer::SDL2_mixer) and do NOT set the
        # ${SDL2_LIBRARY} / ${SDL2_MIXER_LIBRARIES} variables this project used
        # to reference, which left SDL2_mixer unlinked (undefined Mix_* refs).
        # Prefer the imported targets when present, falling back to variables.
        if(TARGET SDL2::SDL2)
            set(OPENSNOWSTORM_SDL2_LIB SDL2::SDL2)
        else()
            set(OPENSNOWSTORM_SDL2_LIB ${SDL2_LIBRARIES})
        endif()
        if(TARGET SDL2_mixer::SDL2_mixer)
            set(OPENSNOWSTORM_SDL2_MIXER_LIB SDL2_mixer::SDL2_mixer)
        else()
            set(OPENSNOWSTORM_SDL2_MIXER_LIB ${SDL2_MIXER_LIBRARIES})
        endif()
    endif()

    add_definitions(-DOPENBW_NO_SDL_IMAGE)
    add_definitions(-DOPENBW_ENABLE_UI)

    # Rendering backend selection (SDL2 vs Headless)
    if(EMSCRIPTEN OR 1 OR SDL2_FOUND)
        add_library(openbw_ui ui/sdl2.cpp)
        if(NOT EMSCRIPTEN)
            target_link_libraries(openbw_ui PUBLIC ${OPENSNOWSTORM_SDL2_LIB} ${OPENSNOWSTORM_SDL2_MIXER_LIB})
            target_include_directories(openbw_ui PUBLIC "${SDL2_INCLUDE_DIR}" "${SDL2_MIXER_INCLUDE_DIR}")
        else()
            opensnowstorm_apply_emscripten_sdl_flags(openbw_ui)
        endif()
        add_definitions(-DOPENBW_ENABLE_SDL)
    else()
        add_library(openbw_ui ui/headless_backend.cpp)
        add_definitions(-DOPENBW_ENABLE_HEADLESS)
    endif()

    target_include_directories(openbw_ui PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/ui
    )
    opensnowstorm_apply_common_compile_options(openbw_ui)
    set_property(TARGET openbw_ui PROPERTY POSITION_INDEPENDENT_CODE ON)

    # Main replay-viewer / benchmark / validate-replay executable
    add_executable(gfxtest ui/gfxtest.cpp)
    target_include_directories(gfxtest PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/ui
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_MIXER_INCLUDE_DIRS}
    )
    if(NOT EMSCRIPTEN)
        target_link_libraries(gfxtest openbw_ui ${OPENSNOWSTORM_SDL2_LIB} ${OPENSNOWSTORM_SDL2_MIXER_LIB})
    else()
        target_link_libraries(gfxtest openbw_ui)
        opensnowstorm_apply_emscripten_executable_defaults(gfxtest)
        opensnowstorm_append_target_flags(gfxtest LINK_FLAGS
            " -sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_ui_resize','_replay_get_value','_replay_set_value','_load_map_data','_player_get_value','_load_replay'] -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS']")
        set_target_properties(gfxtest PROPERTIES SUFFIX ".html")
    endif()
    opensnowstorm_apply_common_compile_options(gfxtest)
endif()

# ---------------------------------------------------------------------------
# mini-openbwapi (BWAPI-compatible C wrapper)
# ---------------------------------------------------------------------------
if(OPENSNOWSTORM_BUILD_BWAPI)
    add_subdirectory(mini-openbwapi)
endif()

add_executable(test_ui test_ui.cpp)

add_executable(test_client_settings tests/test_client_settings.cpp)
target_include_directories(test_client_settings PRIVATE ${CMAKE_SOURCE_DIR})
opensnowstorm_apply_common_compile_options(test_client_settings)
add_test(NAME ClientSettings COMMAND test_client_settings)
