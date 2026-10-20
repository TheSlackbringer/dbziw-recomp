# Dragon Ball Z: Infinite World (PAL SLES_553.47), built on PS2Recomp's runtime.
# Generated game code stays local under ../recomp/output.
cmake_minimum_required(VERSION 3.20)
project(dbziw LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DbziwPlatform.cmake")

set(PS2RECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../external/PS2Recomp")
set(DBZIW_RECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../recomp" CACHE PATH "Folder holding the generated game code")
set(DBZIW_GENERATED_DIR "${DBZIW_RECOMP_ROOT}/output")

# Only the runtime (and the IOP module it needs) from PS2Recomp.
set(PS2X_BUILD_RECOMP OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_RUNTIME ON CACHE BOOL "" FORCE)
set(PS2X_BUILD_ANALYZER OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_STUDIO OFF CACHE BOOL "" FORCE)
set(PS2X_MSVC_FAST_FLOAT OFF CACHE BOOL "" FORCE)
add_subdirectory("${PS2RECOMP_DIR}" ps2recomp EXCLUDE_FROM_ALL)
target_compile_options(ps2_runtime PRIVATE ${DBZIW_STRICT_FP_FLAGS})

# Setup wizard core: reads the player's disc image, checks and extracts it. Needs no game data.
add_library(dbziw_setup_core STATIC
    setup/iso9660.cpp
    setup/disc_setup.cpp
    setup/run_process.cpp
    setup/codegen.cpp
    setup/build_game.cpp
    setup/install_state.cpp
    setup/setup_layout.cpp
    setup/setup_pipeline.cpp
)
target_include_directories(dbziw_setup_core PUBLIC setup)
target_link_libraries(dbziw_setup_core PUBLIC ffmpeg)

# The wizard's window (raylib + imgui) and no-window run.
add_library(dbziw_setup_ui STATIC
    setup/wizard_ui.cpp
    setup/file_dialog.cpp
)
target_link_libraries(dbziw_setup_ui PUBLIC rlImGui imgui raylib dbziw_setup_core)
if(WIN32)
    target_link_libraries(dbziw_setup_ui PUBLIC comdlg32)
endif()

# Developer & first-launch setup executable:
add_executable(dbziw_setup WIN32 setup/wizard_main.cpp setup/wizard_console.cpp)
target_link_libraries(dbziw_setup PRIVATE dbziw_setup_ui)
dbziw_windows_target_options(dbziw_setup)

# Testing
enable_testing()
add_executable(dbziw_setup_disc_test tests/setup_disc_test.cpp)
target_link_libraries(dbziw_setup_disc_test PRIVATE dbziw_setup_core)
set_target_properties(dbziw_setup_disc_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
add_test(NAME setup_disc COMMAND dbziw_setup_disc_test)

# Game executable (when generated sources exist)
option(DBZIW_BUILD_GAME "Build the game (needs locally generated code)" OFF)
if(DBZIW_BUILD_GAME)
    file(GLOB DBZIW_GENERATED_SOURCES CONFIGURE_DEPENDS "${DBZIW_GENERATED_DIR}/*.cpp")
    if(DBZIW_GENERATED_SOURCES)
        add_executable(dbziw
            ${DBZIW_GENERATED_SOURCES}
            "${PS2RECOMP_DIR}/ps2xRuntime/src/main.cpp"
            "${PS2RECOMP_DIR}/ps2xRuntime/src/lib/ps2_debug_panel.cpp"
            setup/wizard_console.cpp
        )
        target_include_directories(dbziw PRIVATE "${DBZIW_GENERATED_DIR}" "${PS2RECOMP_DIR}/ps2xRuntime/src/lib")
        target_link_libraries(dbziw PRIVATE ps2_runtime rlImGui imgui dbziw_setup_ui)
        target_compile_definitions(dbziw PRIVATE PS2X_ENABLE_DEBUG_UI=1 DBZIW_INSTALLED_MODE=1)
        target_compile_options(dbziw PRIVATE ${DBZIW_ARCH_FLAGS} ${DBZIW_STRICT_FP_FLAGS})
        set_target_properties(dbziw PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 64)
        set_source_files_properties(setup/wizard_console.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    endif()
endif()

# Windows: /bigobj, stack reserve, DLL staging
get_property(DBZIW_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(dbziw_target IN LISTS DBZIW_TARGETS)
    dbziw_windows_target_options(${dbziw_target})
endforeach()
