cmake_minimum_required(VERSION 3.20)

project(cppGameEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Dependencies)

set(ENGINE_BUILD_TESTS ON)
if(ENGINE_WINDOWS_CROSS OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(ENGINE_BUILD_TESTS OFF CACHE BOOL "Disable tests for cross/web builds" FORCE)
endif()

if(ENGINE_BUILD_TESTS)
    include(FetchContent)
    set(CATCH2_VERSION "v3.5.4")
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG ${CATCH2_VERSION}
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)
endif()

set(ENGINE_GAMEPLAY_SOURCES
    src/gameplay/GameTypes.cpp
    src/gameplay/Entity.cpp
    src/gameplay/GameStateManager.cpp
    src/gameplay/ZoneManager.cpp
    src/gameplay/ProceduralZoneGenerator.cpp
    src/gameplay/MobController.cpp
    src/gameplay/IsometricCamera.cpp
    src/systems/ItemTypes.cpp
    src/systems/Inventory.cpp
    src/systems/Equipment.cpp
    src/systems/LootEngine.cpp
    src/systems/TradeSystem.cpp
    src/systems/CharacterCombat.cpp
    src/systems/ItemStats.cpp
    src/systems/ItemGenerator.cpp
    src/systems/RunProgression.cpp
    src/systems/WeaponMastery.cpp
    src/systems/SoulProgression.cpp
    src/systems/Blacksmith.cpp
    src/game/SaveGame.cpp
    src/game/SaveGamePlatform.cpp
    src/ui/OverlayState.cpp
    src/ui/MinimapSystem.cpp
    src/ui/UiHitTest.cpp
    src/ui/UiScale.cpp
    src/ui/UiLayout.cpp
    src/ui/UiTextLayout.cpp
    src/ui/UiInteraction.cpp
)

add_library(EngineCore STATIC
    src/Window.cpp
    src/Shader.cpp
    src/render/Mesh.cpp
    src/render/Texture.cpp
    src/render/UiAssets.cpp
    src/render/MobAssets.cpp
    src/render/WorldPropAssets.cpp
    src/render/SpriteSheet.cpp
    src/render/SpriteRenderer.cpp
    src/render/UiRenderer.cpp
    src/render/TextRenderer.cpp
)

target_include_directories(EngineCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party>
        $<INSTALL_INTERFACE:include>
)

engine_apply_warnings(EngineCore)
engine_apply_debug_sanitizers(EngineCore)
engine_apply_release_opts(EngineCore)
engine_link_graphics(EngineCore)
engine_link_glm(EngineCore)

add_library(EngineGameplay STATIC ${ENGINE_GAMEPLAY_SOURCES})

target_include_directories(EngineGameplay
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

engine_link_glm(EngineGameplay)

engine_apply_warnings(EngineGameplay)
engine_apply_debug_sanitizers(EngineGameplay)
engine_apply_release_opts(EngineGameplay)

find_program(CLANG_TIDY_EXE NAMES clang-tidy)

if(CLANG_TIDY_EXE)
    set_target_properties(EngineCore EngineGameplay PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
else()
    message(STATUS "clang-tidy not found; static analysis disabled")
endif()

add_executable(GameEngine
    src/main.cpp
    src/game/GameApplication.cpp
    src/game/GameDebug.cpp
    src/game/AppFlow.cpp
    src/game/GameSettings.cpp
    src/game/EntityPicker.cpp
    src/game/CombatSystem.cpp
)
if(ENGINE_WINDOWS_CROSS)
    target_compile_definitions(GameEngine PRIVATE ENGINE_ASSETS_RELATIVE="assets")
    enable_language(RC)
    target_sources(GameEngine PRIVATE "${CMAKE_SOURCE_DIR}/cmake/windows/GameEngine.rc")
    set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/cmake/windows/GameEngine.rc"
        PROPERTIES COMPILE_FLAGS "-I\"${CMAKE_SOURCE_DIR}/cmake/windows\""
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    target_compile_definitions(GameEngine PRIVATE ENGINE_ASSETS_RELATIVE="/assets")
    target_compile_options(GameEngine PRIVATE -fexceptions)
else()
    target_compile_definitions(GameEngine PRIVATE
        ENGINE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
    )
endif()
engine_apply_warnings(GameEngine)
engine_apply_debug_sanitizers(GameEngine)
engine_apply_release_opts(GameEngine)
target_link_libraries(GameEngine PRIVATE EngineCore EngineGameplay)
if(TARGET glfw)
    target_link_libraries(GameEngine PRIVATE glfw)
elseif(TARGET PkgConfig::GLFW3)
    target_link_libraries(GameEngine PRIVATE PkgConfig::GLFW3)
endif()

if(ENGINE_WINDOWS_CROSS)
    target_link_options(GameEngine PRIVATE
        -static-libgcc
        -static-libstdc++
        "LINKER:--nxcompat"
        "LINKER:--dynamicbase"
        "LINKER:--high-entropy-va"
    )
    add_custom_command(
        TARGET GameEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:GameEngine>/assets"
        COMMENT "Staging assets beside GameEngine.exe"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    target_link_options(GameEngine PRIVATE
        "SHELL:-sUSE_GLFW=3"
        "SHELL:-sMAX_WEBGL_VERSION=2"
        "SHELL:-sFULL_ES3=1"
        "SHELL:-sALLOW_MEMORY_GROWTH=1"
        "SHELL:-sENVIRONMENT=web"
        "SHELL:-fexceptions"
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/assets@/assets"
        "$<$<CONFIG:Release>:SHELL:-sASSERTIONS=0>"
        "$<$<NOT:$<CONFIG:Release>>:SHELL:-sASSERTIONS=1>"
    )
    add_custom_command(
        TARGET GameEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/cmake/web/shell.html"
            "$<TARGET_FILE_DIR:GameEngine>/index.html"
        COMMENT "Copying WebGL shell page"
    )
endif()

if(CLANG_TIDY_EXE)
    set_target_properties(GameEngine PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

if(ENGINE_BUILD_TESTS)
    add_executable(EngineTests
        tests/test_math.cpp
        tests/test_gameplay_systems.cpp
        tests/test_ui.cpp
        tests/test_combat.cpp
        tests/test_souls.cpp
        tests/test_items.cpp
        tests/test_equipment.cpp
        tests/test_replayability.cpp
        tests/test_save_game.cpp
        tests/test_weapon_mastery.cpp
        tests/test_mobs.cpp
        tests/test_blacksmith.cpp
    )
    engine_apply_warnings(EngineTests)
    engine_apply_debug_sanitizers(EngineTests)
    engine_apply_release_opts(EngineTests)
    target_link_libraries(EngineTests PRIVATE
        Catch2::Catch2WithMain
        EngineGameplay
        EngineCore
    )
    target_compile_definitions(EngineTests PRIVATE
        ENGINE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
    )
    target_sources(EngineTests PRIVATE
        src/game/CombatSystem.cpp
    )
    engine_link_glm(EngineTests)

    if(CLANG_TIDY_EXE)
        set_target_properties(EngineTests PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    endif()

    enable_testing()
    add_test(NAME engine_math_suite COMMAND EngineTests "[math]")
    add_test(NAME engine_camera_suite COMMAND EngineTests "[camera]")
    add_test(NAME engine_zone_suite COMMAND EngineTests "[zone]")
    add_test(NAME engine_loot_suite COMMAND EngineTests "[loot]")
    add_test(NAME engine_inventory_suite COMMAND EngineTests "[inventory]")
    add_test(NAME engine_trade_suite COMMAND EngineTests "[trade]")
    add_test(NAME engine_minimap_suite COMMAND EngineTests "[minimap]")
    add_test(NAME engine_ui_suite COMMAND EngineTests "[ui]")
    add_test(NAME engine_combat_suite COMMAND EngineTests "[combat]")
    add_test(NAME engine_souls_suite COMMAND EngineTests "[souls]")
    add_test(NAME engine_items_suite COMMAND EngineTests "[items]")
    add_test(NAME engine_replayability_suite COMMAND EngineTests "[replayability]")
    add_test(NAME engine_save_suite COMMAND EngineTests "[save]")
    add_test(NAME engine_mastery_suite COMMAND EngineTests "[mastery]")
    add_test(NAME engine_mobs_suite COMMAND EngineTests "[mobs]")
    add_test(NAME engine_world_props_suite COMMAND EngineTests "[world]")

    if(NOT ENGINE_WINDOWS_CROSS)
        target_sources(EngineTests PRIVATE
            tests/test_boot.cpp
            tests/test_playthrough.cpp
            tests/test_performance.cpp
            src/game/GameApplication.cpp
            src/game/GameDebug.cpp
            src/game/AppFlow.cpp
            src/game/GameSettings.cpp
            src/game/EntityPicker.cpp
        )
        target_link_libraries(EngineTests PRIVATE EngineCore)
        engine_link_graphics(EngineTests)
        if(TARGET glfw)
            target_link_libraries(EngineTests PRIVATE glfw)
        elseif(TARGET PkgConfig::GLFW3)
            target_link_libraries(EngineTests PRIVATE PkgConfig::GLFW3)
        endif()
        target_include_directories(EngineTests PRIVATE "${CMAKE_SOURCE_DIR}/tests")
        target_compile_definitions(EngineTests PRIVATE
            ENGINE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
        )
        add_test(NAME engine_boot_suite COMMAND EngineTests "[boot]")
        add_test(NAME engine_playthrough_suite COMMAND EngineTests "[playthrough]")
        add_test(NAME engine_performance_suite COMMAND EngineTests "[performance]")
    endif()
endif()
