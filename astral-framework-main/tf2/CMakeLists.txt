add_library(astral_tf2 SHARED
    entry.cpp
    global.cpp
    hooks.inl
    menu.cpp
    menu.hpp
    preview.cpp
    config.cpp
    config_storage.cpp
    config.hpp
    script_api.cpp
    sdk.cpp
    entities.cpp
    netvars.cpp

    features/anti_aim.cpp
    features/anticheat_profile.cpp
    features/anti_backstab.cpp
    features/strafe-prediction.cpp
    features/air_blast.cpp
    features/anims.cpp
    features/auto_det.cpp
    features/auto_automation.cpp
    features/auto_rocketjmp.cpp
    features/auto_vacc.cpp
    features/bone_setup.cpp
    features/lagcomp.cpp
    features/extrapolate.cpp
    features/fakelag.cpp
    features/fake_latency.cpp
    features/fast_queue.cpp
    features/prediction.cpp
    features/datamaps.cpp
    features/doors.cpp
    features/discord_rpc.cpp
    features/fall_damage.cpp
    features/misc.cpp
    features/player_data.cpp
    features/player_cloud.cpp
    features/player_perf.cpp
    features/player_card_image.cpp
    features/party_network.cpp
    features/entity_cache.cpp
    features/enhanced_spectate.cpp
    features/esp.cpp
    features/esp_builder.cpp
    features/chams.cpp
    features/model_render.cpp
    features/glow.cpp
    features/hitmarker.cpp
    features/compression.cpp
    features/event_notifiers.cpp
    features/crits.cpp
    features/crash_reporter.cpp
    features/events.cpp
    features/external_media.cpp
    features/loading.cpp
    features/menu_mascot.cpp
    features/networking.cpp
    features/tickbase.cpp
    features/nospread.cpp
    features/proj_camera.cpp
    features/proj_sim.cpp
    features/projectile_warning.cpp
    features/splash.cpp
    features/move_sim.cpp
    features/movement_recorder.cpp
    features/radio.cpp
    features/radar.cpp
    features/ragebot.cpp
    features/resolver.cpp
    features/melee.cpp
    features/holdaim_mgr.cpp
    features/interp_mgr.cpp
    features/skybox.cpp
    features/soundboard.cpp
    features/session.cpp
    features/server_hitboxes.cpp
    features/skin_share.cpp
    features/skins.cpp
    features/streamer_mode.cpp
    features/telemetry_blocker.cpp
    features/spectator_list.cpp
    features/tracers.cpp
    features/trajectory.cpp
features/triggerbot.cpp
    features/viewmodel.cpp
    features/behavior_scanner.cpp
    features/astral_sense.cpp
    features/binds_list.cpp
    features/watchdog.cpp
    features/weather.cpp
    features/world_modulation.cpp
    features/post_process.cpp
    features/medbot.cpp

    $<TARGET_OBJECTS:astral_se>
)

target_include_directories(astral_tf2 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set(ASTRAL_MUSIC_FRAMEWORK "${PROJECT_SOURCE_DIR}/../externals/music-framework" CACHE PATH
    "Path to the Nihon's Music Framework source tree")
set(ASTRAL_MUSIC_FRAMEWORK_LIB_DIR "${ASTRAL_MUSIC_FRAMEWORK}/build-x64-nocrt/Release")

foreach(required_path
    "${ASTRAL_MUSIC_FRAMEWORK}/include/MusicOverlay/Integration/MusicOverlay.h"
    "${ASTRAL_MUSIC_FRAMEWORK_LIB_DIR}/musicoverlay.lib"
    "${ASTRAL_MUSIC_FRAMEWORK_LIB_DIR}/musicoverlay_nocrt_runtime.lib")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR
            "Missing Music Framework dependency: ${required_path}\n"
            "Build it with build.ps1 -Configuration Release -Architecture x64 -NoCRT, "
            "or configure ASTRAL_MUSIC_FRAMEWORK explicitly.")
    endif()
endforeach()

target_include_directories(astral_tf2 PRIVATE
    "${ASTRAL_MUSIC_FRAMEWORK}/include"
    "${ASTRAL_MUSIC_FRAMEWORK}/src"
    "${ASTRAL_MUSIC_FRAMEWORK}/src/NoCRT"
)
target_compile_definitions(astral_tf2 PRIVATE
    MUSICOVERLAY_NOCRT=1
    MUSICOVERLAY_MEDIA_ABI=1
    _ITERATOR_DEBUG_LEVEL=0
    _USE_STD_VECTOR_ALGORITHMS=0
)
target_link_libraries(astral_tf2 PRIVATE
    astral_options
    "${ASTRAL_MUSIC_FRAMEWORK_LIB_DIR}/musicoverlay.lib"
    "${ASTRAL_MUSIC_FRAMEWORK_LIB_DIR}/musicoverlay_nocrt_runtime.lib"
    kernel32 user32 runtimeobject shcore uuid ole32
)
# Astral objects are linked first and remain authoritative for shared no-CRT
# primitives; the framework archive supplies its additional STL/WinRT helpers.
target_link_options(astral_tf2 PRIVATE /FORCE:MULTIPLE)
target_compile_definitions(astral_tf2 PUBLIC ASTRAL_TF2=1)

set(ASTRAL_TF2_CHANNEL "Beta" CACHE STRING "Astral TF2 release channel")
set_property(CACHE ASTRAL_TF2_CHANNEL PROPERTY STRINGS Stable Beta)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(astral_tf2 PRIVATE /ENTRY:astral_entry)
    target_sources(astral_tf2 PRIVATE dev_tables.cpp)

    # im doing this so we can see what the performance will actually be on release builds.
    set_property(SOURCE
        features/ragebot.cpp
        features/proj_sim.cpp
        features/splash.cpp
        features/move_sim.cpp
        features/strafe-prediction.cpp
        APPEND PROPERTY COMPILE_OPTIONS /O2 /Ob2
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_link_options(astral_tf2 PRIVATE /ENTRY:astral_entry)
    target_sources(astral_tf2 PRIVATE dev_tables.cpp)
else ()
    target_compile_definitions(astral_tf2 PRIVATE ASTRAL_SHIPPING)
    target_link_options(astral_tf2 PRIVATE /NOENTRY "/MAP:${CMAKE_BINARY_DIR}/astral-tf2.map")

    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    option(ASTRAL_TF2_VMPROTECT "Virtualize TF2 startup and session code before ATLX packing" OFF)
    set(astral_tf2_pack_options)
    set(astral_tf2_pack_outputs "${CMAKE_BINARY_DIR}/astral-tf2.atlx")
    if (ASTRAL_TF2_VMPROTECT)
        find_program(ASTRAL_VMP_CONSOLE
            NAMES VMProtect_Con.exe
            HINTS "$ENV{USERPROFILE}/Desktop/VMProtect Ultimate" "$ENV{PROGRAMFILES}/VMProtect Ultimate"
            REQUIRED
        )
        list(APPEND astral_tf2_pack_options
            --vmprotect "${ASTRAL_VMP_CONSOLE}"
            --map "${CMAKE_BINARY_DIR}/astral-tf2.map"
            --protected-output "${CMAKE_BINARY_DIR}/astral-tf2.vmp.dll"
        )
        list(APPEND astral_tf2_pack_outputs
            "${CMAKE_BINARY_DIR}/astral-tf2.vmp.dll"
            "${CMAKE_BINARY_DIR}/astral-tf2.dll.vmp"
        )
    endif ()
    set_property(
        TARGET astral_tf2
        PROPERTY LINK_DEPENDS "${PROJECT_SOURCE_DIR}/tools/astral_pack.py"
    )

    add_custom_command(
        TARGET astral_tf2
        POST_BUILD
        BYPRODUCTS ${astral_tf2_pack_outputs}
        COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/astral_pack.py"
        --input "$<TARGET_FILE:astral_tf2>"
        --output "${CMAKE_BINARY_DIR}/astral-tf2.atlx"
        ${astral_tf2_pack_options}
        COMMENT "Packing astral_tf2 ATLX v2 payload"
        VERBATIM
    )
endif ()

if (ASTRAL_TF2_CHANNEL STREQUAL "Stable")
    target_compile_definitions(astral_tf2 PRIVATE ASTRAL_CHANNEL_STABLE)
elseif (ASTRAL_TF2_CHANNEL STREQUAL "Beta")
    target_compile_definitions(astral_tf2 PRIVATE ASTRAL_CHANNEL_BETA)
else ()
    message(FATAL_ERROR "ASTRAL_TF2_CHANNEL must be Stable or Beta.")
endif ()

set_target_properties(
    astral_tf2
    PROPERTIES
    CXX_EXTENSIONS OFF
    MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug>:Embedded>"
    OUTPUT_NAME astral-tf2
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
