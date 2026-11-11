# Dragon Ball Z: Infinite World (PAL SLES_553.47), built on PS2Recomp's runtime.
# Generated game code stays local under ../recomp/output.
cmake_minimum_required(VERSION 3.20)
project(dbziw LANGUAGES C CXX)

# DBZIW 011: every target's compile commands, the game's included, go into compile_commands.json (the preflight
# checks the game's own flags there; PS2Recomp's CMakeLists exported only the targets below it).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DbziwPlatform.cmake")

set(PS2RECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../external/PS2Recomp")
set(DBZIW_RECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../recomp" CACHE PATH "Folder holding the generated game code")
set(DBZIW_GENERATED_DIR "${DBZIW_RECOMP_ROOT}/output")
include(cmake/generated_sources.cmake)

# Vulkan compute GS renderer (paraLLEl-GS)
set(PS2X_ENABLE_VULKAN_GS ON CACHE BOOL "" FORCE)
if(NOT PS2X_PARALLEL_GS_SOURCE_DIR)
    set(PS2X_PARALLEL_GS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/parallel-gs" CACHE PATH "" FORCE)
endif()

# Only the runtime (and the IOP module it needs) from PS2Recomp.
set(PS2X_BUILD_RECOMP OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_RUNTIME ON CACHE BOOL "" FORCE)
set(PS2X_BUILD_ANALYZER OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_STUDIO OFF CACHE BOOL "" FORCE)
set(PS2X_ENABLE_AGRESSIVE_LOGS OFF CACHE BOOL "" FORCE)
set(PS2X_MSVC_FAST_FLOAT OFF CACHE BOOL "" FORCE)
# Issue 010 (DBZIW-DEBUG, DW5 025 b8d976a): profiling markers, debug info and frame pointers for OS recorders (WPR,
# perf). Use a separate build directory (app/build-diagnostics); never for timing acceptance or releases.
option(DBZIW_DEVELOPER_DIAGNOSTICS "Developer diagnostics build: profiling markers (issue 010)" OFF)
if(DBZIW_DEVELOPER_DIAGNOSTICS)
    add_compile_options(-g -fno-omit-frame-pointer)
    if(WIN32)
        add_compile_options(-gcodeview)
        add_link_options(-Wl,--pdb=)
    endif()
endif()
add_subdirectory("${PS2RECOMP_DIR}" ps2recomp EXCLUDE_FROM_ALL)
target_compile_options(ps2_runtime PRIVATE ${DBZIW_STRICT_FP_FLAGS})
if(DBZIW_DEVELOPER_DIAGNOSTICS)
    target_sources(ps2_runtime PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/src/lib/ps2_profile_markers.cpp")
    target_compile_definitions(ps2_runtime PUBLIC DBZIW_DEVELOPER_DIAGNOSTICS=1)
endif()

# Setup wizard core: reads the player's disc image, checks and extracts it. Needs no game data.
find_package(Threads REQUIRED)
add_library(dbziw_vu_generator OBJECT setup/vu1lift/vu1_timing.cpp setup/vu1lift/vu1_translator.cpp)
target_link_libraries(dbziw_vu_generator PRIVATE ffmpeg Threads::Threads)
set(DBZIW_SETUP_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/setup-generated")
file(GENERATE OUTPUT "${DBZIW_SETUP_GENERATED}/vu-objects-$<CONFIG>.txt"
    CONTENT "$<JOIN:$<TARGET_OBJECTS:dbziw_vu_generator>,\n>\n")
add_custom_command(OUTPUT "${DBZIW_SETUP_GENERATED}/vu_producer.h"
    COMMAND "${CMAKE_COMMAND}" "-DOBJECT_LIST=${DBZIW_SETUP_GENERATED}/vu-objects-$<CONFIG>.txt"
        "-DOUT=${DBZIW_SETUP_GENERATED}/vu_producer.h" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/vu_producer.cmake"
    DEPENDS dbziw_vu_generator $<TARGET_OBJECTS:dbziw_vu_generator> "${CMAKE_CURRENT_SOURCE_DIR}/cmake/vu_producer.cmake"
    VERBATIM)
add_custom_target(dbziw_vu_identity DEPENDS "${DBZIW_SETUP_GENERATED}/vu_producer.h")
# DW5 009 v4.5 (015): the IOP module translator (used by dbziw_ioplift and the setup pipeline).
add_library(dbziw_iop_translator OBJECT setup/ioplift/iop_translator.cpp setup/ioplift/iop_generate.cpp)
target_include_directories(dbziw_iop_translator PUBLIC setup/ioplift "${PS2RECOMP_DIR}/ps2xIOP/include"
    PRIVATE "${PS2RECOMP_DIR}/ps2xIOP/src")
target_link_libraries(dbziw_iop_translator PRIVATE ps2_iop)
add_library(dbziw_setup_core STATIC
    setup/iso9660.cpp
    setup/disc_setup.cpp
    setup/run_process.cpp
    setup/codegen.cpp
    setup/build_game.cpp
    setup/install_state.cpp
    setup/setup_layout.cpp
    setup/setup_pipeline.cpp
    setup/setup_cache.cpp
    setup/install_location.cpp
    setup/installation.cpp
    setup/uninstall_process.cpp
    $<TARGET_OBJECTS:dbziw_vu_generator>
    $<TARGET_OBJECTS:dbziw_iop_translator>
)
add_dependencies(dbziw_setup_core dbziw_vu_identity)
target_include_directories(dbziw_setup_core PRIVATE "${DBZIW_SETUP_GENERATED}")
target_include_directories(dbziw_setup_core PUBLIC setup)
target_link_libraries(dbziw_setup_core PUBLIC ffmpeg Threads::Threads ps2_iop)
# VU1 translator (issue 012; DW5 025 Step 6, 5442b4a): dbziw_vu1lift <SLES_553.47> <output.cpp> translates the game's
# VU1 microprograms; dbziw_vu1lift --find-range <image> <pc> <name> [disc files] prints the vu1_inputs.h entries for
# a program seen in a PS2X_VU1_CENSUS run.
add_executable(dbziw_vu1lift setup/vu1lift/vu1lift_main.cpp)
target_link_libraries(dbziw_vu1lift PRIVATE dbziw_setup_core)
# DW5 009 v4.5 (015): IOP module translator: dbziw_ioplift <MODULES folder> <output folder> (game-derived output);
# dbziw_ioplift --raw <code.bin> <base hex> <name> <output folder> for the translator test.
add_executable(dbziw_ioplift setup/ioplift/ioplift_main.cpp $<TARGET_OBJECTS:dbziw_iop_translator>)
target_include_directories(dbziw_ioplift PRIVATE setup/ioplift "${PS2RECOMP_DIR}/ps2xIOP/include")
target_link_libraries(dbziw_ioplift PRIVATE ps2_iop)

# The wizard's window (raylib + imgui) and no-window run.
add_library(dbziw_setup_ui STATIC
    setup/wizard_ui.cpp
    setup/wizard_storage.cpp
    setup/file_dialog.cpp
)
if(TARGET imgui)
    # A developer build finds the launcher's font here; a package reads it from app/deps/imgui-src.
    get_target_property(DBZIW_IMGUI_INCLUDES imgui INTERFACE_INCLUDE_DIRECTORIES)
    list(GET DBZIW_IMGUI_INCLUDES 0 DBZIW_IMGUI_SRC)
    string(REGEX REPLACE "\\$<BUILD_INTERFACE:(.*)>" "\\1" DBZIW_IMGUI_SRC "${DBZIW_IMGUI_SRC}")
    target_compile_definitions(dbziw_setup_ui PRIVATE DBZIW_IMGUI_FONT="${DBZIW_IMGUI_SRC}/misc/fonts/Roboto-Medium.ttf")
endif()
target_link_libraries(dbziw_setup_ui PUBLIC rlImGui imgui raylib dbziw_setup_core)
if(WIN32)
    target_link_libraries(dbziw_setup_ui PUBLIC comdlg32 ole32 uuid)
endif()

# Developer & first-launch setup executable:
add_executable(dbziw_setup WIN32 setup/wizard_main.cpp setup/wizard_console.cpp)
target_link_libraries(dbziw_setup PRIVATE dbziw_setup_ui)
if(WIN32)
    target_link_libraries(dbziw_setup_core PUBLIC shell32)
    # What the player starts on Windows: needs only system DLLs and starts the real launcher in dbziw-data/package/launcher.
    add_executable(dbziw_bootstrap WIN32 setup/bootstrap_win.c)
    set_target_properties(dbziw_bootstrap PROPERTIES OUTPUT_NAME DBZIW-bootstrap)
    target_link_options(dbziw_bootstrap PRIVATE -municode -static)
endif()
# Finishes an uninstall after the launcher has closed, from outside the installation.
add_executable(dbziw_uninstall setup/uninstall_main.cpp setup/installation.cpp setup/install_location.cpp setup/install_state.cpp)
target_include_directories(dbziw_uninstall PRIVATE setup)
target_link_libraries(dbziw_uninstall PRIVATE Threads::Threads)
set_target_properties(dbziw_uninstall PROPERTIES OUTPUT_NAME dbziw-uninstall)
if(WIN32)
    target_link_libraries(dbziw_uninstall PRIVATE shell32 user32)
    set_target_properties(dbziw_uninstall PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_options(dbziw_uninstall PRIVATE -static)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # DW5-E a71ccaa3: no C++ ABI boundary with the wizard; glibc stays dynamic to avoid clashing static libraries in the SDK image.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(dbziw_uninstall PRIVATE -stdlib=libstdc++)
        target_link_options(dbziw_uninstall PRIVATE -stdlib=libstdc++ -nostdlib++ "LINKER:--as-needed")
        target_link_libraries(dbziw_uninstall PRIVATE "-l:libstdc++.a")
    else()
        target_link_options(dbziw_uninstall PRIVATE -static-libstdc++)
    endif()
    target_link_options(dbziw_uninstall PRIVATE -static-libgcc)
endif()
add_dependencies(dbziw_setup dbziw_uninstall)
if(WIN32)
    add_dependencies(dbziw_setup dbziw_bootstrap)
endif()
dbziw_windows_target_options(dbziw_setup)

# Testing
enable_testing()
add_executable(dbziw_setup_disc_test tests/setup_disc_test.cpp)
target_link_libraries(dbziw_setup_disc_test PRIVATE dbziw_setup_core)
set_target_properties(dbziw_setup_disc_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
add_test(NAME setup_disc COMMAND dbziw_setup_disc_test)
# DBZIW 019: which video mode a run starts in (test, player's setting, launcher default, disc).
add_executable(dbziw_video_mode_choice_test tests/video_mode_choice_test.cpp)
target_include_directories(dbziw_video_mode_choice_test PRIVATE src)
set_target_properties(dbziw_video_mode_choice_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
add_test(NAME video_mode_choice COMMAND dbziw_video_mode_choice_test)
# DBZIW 007: the setup CTests from DW5, disc-free.
foreach(name setup_build setup_codegen setup_pipeline setup_install setup_cache setup_sync)
    add_executable(dbziw_${name}_test tests/${name}_test.cpp)
    target_link_libraries(dbziw_${name}_test PRIVATE dbziw_setup_core)
    # Beside the DLLs: these import FFmpeg's avutil, and a missing DLL stops a Windows program with a dialog.
    add_test(NAME ${name} COMMAND dbziw_${name}_test)
endforeach()
add_executable(dbziw_setup_timing_test tests/setup_timing_test.cpp)
target_include_directories(dbziw_setup_timing_test PRIVATE setup)
add_test(NAME setup_timing COMMAND dbziw_setup_timing_test)

# Launcher tests (007, DW5-E's): the real main, storage and pipeline on a made-up disc, with a stand-in window and setup steps.
set(DBZIW_FIXTURE_DISC DBZIW_TEST_DISC_MANIFEST="${CMAKE_CURRENT_SOURCE_DIR}/tests/launcher_fixture_disc.h")
add_executable(dbziw_launcher_fixture WIN32 tests/launcher_fixture.cpp setup/wizard_main.cpp setup/wizard_console.cpp setup/wizard_storage.cpp
    setup/iso9660.cpp setup/disc_setup.cpp setup/run_process.cpp setup/codegen.cpp setup/build_game.cpp setup/install_state.cpp setup/setup_layout.cpp
    setup/setup_pipeline.cpp setup/setup_cache.cpp setup/install_location.cpp setup/installation.cpp setup/uninstall_process.cpp
    $<TARGET_OBJECTS:dbziw_vu_generator> $<TARGET_OBJECTS:dbziw_iop_translator>)
add_dependencies(dbziw_launcher_fixture dbziw_vu_identity)
target_compile_definitions(dbziw_launcher_fixture PRIVATE ${DBZIW_FIXTURE_DISC})
target_include_directories(dbziw_launcher_fixture PRIVATE setup "${DBZIW_SETUP_GENERATED}")
target_link_libraries(dbziw_launcher_fixture PRIVATE ffmpeg Threads::Threads ps2_iop)
add_executable(dbziw_install_fake_game WIN32 tests/install_fake_game.cpp setup/install_location.cpp)
target_include_directories(dbziw_install_fake_game PRIVATE setup)
add_executable(dbziw_install_location_test tests/install_location_test.cpp)
target_link_libraries(dbziw_install_location_test PRIVATE dbziw_setup_core)
add_test(NAME install_location COMMAND dbziw_install_location_test)
add_executable(dbziw_install_folder_test tests/install_folder_test.cpp setup/install_location.cpp)
target_include_directories(dbziw_install_folder_test PRIVATE setup)
add_test(NAME install_folder_default COMMAND dbziw_install_folder_test $<TARGET_FILE_DIR:dbziw_install_folder_test>)
set_tests_properties(install_folder_default PROPERTIES ENVIRONMENT "DBZIW_DATA_DIR=;APPIMAGE=")
add_executable(dbziw_storage_default_test tests/storage_default_test.cpp setup/install_location.cpp)
target_include_directories(dbziw_storage_default_test PRIVATE setup)
add_test(NAME storage_default COMMAND dbziw_storage_default_test)
set_tests_properties(storage_default PROPERTIES ENVIRONMENT "DBZIW_DATA_DIR=")
add_executable(dbziw_storage_policy_test tests/storage_policy_test.cpp setup/installation.cpp setup/install_location.cpp setup/install_state.cpp)
target_compile_definitions(dbziw_storage_policy_test PRIVATE ${DBZIW_FIXTURE_DISC})
target_include_directories(dbziw_storage_policy_test PRIVATE setup)
target_link_libraries(dbziw_storage_policy_test PRIVATE Threads::Threads)
add_test(NAME storage_policy COMMAND dbziw_storage_policy_test)
if(WIN32)
    target_link_options(dbziw_install_fake_game PRIVATE -static)
    foreach(target dbziw_launcher_fixture dbziw_install_fake_game dbziw_install_folder_test dbziw_storage_default_test dbziw_storage_policy_test)
        target_link_libraries(${target} PRIVATE shell32 user32)
    endforeach()
endif()
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND CMAKE_HOST_SYSTEM_NAME STREQUAL CMAKE_SYSTEM_NAME)
    set(DBZIW_BOOTSTRAP_ARGS)
    if(WIN32)
        set(DBZIW_BOOTSTRAP_ARGS --bootstrap $<TARGET_FILE:dbziw_bootstrap>)
        add_test(NAME launcher_bootstrap COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/bootstrap_test.py
            --bootstrap $<TARGET_FILE:dbziw_bootstrap> --child $<TARGET_FILE:dbziw_install_fake_game> --out ${CMAKE_CURRENT_BINARY_DIR}/bootstrap-test)
    endif()
    add_test(NAME launcher_install COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/launcher_install_test.py
        --launcher $<TARGET_FILE:dbziw_launcher_fixture> --game $<TARGET_FILE:dbziw_install_fake_game>
        --uninstaller $<TARGET_FILE:dbziw_uninstall> ${DBZIW_BOOTSTRAP_ARGS} --out ${CMAKE_CURRENT_BINARY_DIR}/install-launcher-test)
    set_tests_properties(launcher_install PROPERTIES TIMEOUT 600)
endif()
add_executable(dbziw_setup_measure tests/setup_measure.cpp)
target_link_libraries(dbziw_setup_measure PRIVATE dbziw_setup_core)

# Sofdec movie video input without the IPU (issue 020), shared by the game and its test
add_library(dbziw_movie_input STATIC src/dbziw_movie_input.cpp)
target_include_directories(dbziw_movie_input PUBLIC src)
target_link_libraries(dbziw_movie_input PUBLIC ps2_runtime)
add_executable(dbziw_movie_input_test tests/movie_input_test.cpp)
target_link_libraries(dbziw_movie_input_test PRIVATE dbziw_movie_input)
set_target_properties(dbziw_movie_input_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
add_test(NAME movie_input COMMAND dbziw_movie_input_test)
# The libmpeg replacement itself on the runtime's EE scheduler: callbacks in Sofdec's order (issue 020, criterion 4)
add_executable(dbziw_movie_replacement_test tests/movie_replacement_test.cpp src/dbziw_movies.cpp)
target_link_libraries(dbziw_movie_replacement_test PRIVATE dbziw_movie_input)
set_target_properties(dbziw_movie_replacement_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
add_test(NAME movie_replacement COMMAND dbziw_movie_replacement_test)

# DBZIW-TEST: GS recording tools (tools/gsreplay, issue 017, from DW5 35701a6): inspect and compare PS2X_GS_RECORD
# recordings, render their frames offline in either renderer, replay a VIF1 capture without the game.
set(DBZIW_GSREPLAY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../tools/gsreplay")
if(EXISTS "${DBZIW_GSREPLAY_DIR}") # DBZIW 007: developer tools, not in a release package
add_executable(dbziw_gsrec "${DBZIW_GSREPLAY_DIR}/gsrec.cpp")
target_include_directories(dbziw_gsrec PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/src/lib" "${PS2RECOMP_DIR}/ps2xRuntime/include")
add_executable(dbziw_gsrender "${DBZIW_GSREPLAY_DIR}/gsrender.cpp")
target_link_libraries(dbziw_gsrender PRIVATE ps2_runtime)
target_include_directories(dbziw_gsrender PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/src/lib" "${PS2RECOMP_DIR}/ps2xRuntime/include")
add_executable(dbziw_vif1_replay "${DBZIW_GSREPLAY_DIR}/vif1_replay.cpp")
target_link_libraries(dbziw_vif1_replay PRIVATE ps2_runtime)
target_include_directories(dbziw_vif1_replay PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/src/lib" "${DBZIW_GSREPLAY_DIR}")
target_compile_options(dbziw_vif1_replay PRIVATE ${DBZIW_ARCH_FLAGS})
endif()

# DBZIW-TEST: DW5's runtime regression tests at the fork (issue 018, DW5 438a3de app/tests). They guard runtime
# fixes DBZIW took over at the fork, with authored inputs and small fake function tables: no game data.
function(dbziw_runtime_test name)
    add_executable(dbziw_${name}_test tests/${name}_test.cpp ${ARGN})
    target_link_libraries(dbziw_${name}_test PRIVATE ps2_runtime)
    target_compile_options(dbziw_${name}_test PRIVATE ${DBZIW_ARCH_FLAGS})
    set_target_properties(dbziw_${name}_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
    add_test(NAME ${name} COMMAND dbziw_${name}_test)
endfunction()
foreach(name vu_minmax vif_unpack_fast vif_unpack_boundary ee_shared_vu0 cd_iop_read cd_stream_rate presentation_stride
        intc_status gs_csr mc_dir_listing cd_read_unknown sif_register_rpc iop_heap_free deci2_call) # DBZIW 036, 033, 034, 035, 037
    dbziw_runtime_test(${name})
endforeach()
dbziw_runtime_test(rom_profile src/dbziw_rom_profile.cpp) # DBZIW 037
dbziw_runtime_test(keyboard_stick) # DBZIW 038
dbziw_runtime_test(settings_file) # DBZIW 007
dbziw_runtime_test(crt_mode_switch) # DBZIW 019
dbziw_runtime_test(host_input_field_sample) # DBZIW 013 (DW5 023 5c): one controller sample per field
dbziw_runtime_test(ee_timer_lazy) # DBZIW 039 (v3 stage 3): EE timers advanced only when observable
# DBZIW 013 (DW5 eaa68f3, 08d30e8): the function keys come from one table; README.md lists exactly those keys.
add_executable(dbziw_hotkeys_test tests/hotkeys_test.cpp)
target_link_libraries(dbziw_hotkeys_test PRIVATE ps2_runtime)
target_compile_definitions(dbziw_hotkeys_test PRIVATE DBZIW_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/..")
set_target_properties(dbziw_hotkeys_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
add_test(NAME hotkeys COMMAND dbziw_hotkeys_test)
set_tests_properties(keyboard_stick PROPERTIES ENVIRONMENT "PS2X_HOST_INPUT=100:W:1,200:S:1,300:A:1,400:D:1,500:W:1,500:A:1,600:W:1,600:S:1,700:A:1,700:D:1,800:UP:1,900:DOWN:1,1000:LEFT:1,1100:RIGHT:1,1200:W:1,1200:DOWN:1")
# DBZIW 011: DW5 025's guards for the runtime speed fixes (A, B, C2, C3a, C3b, D) and DW5 023 7T.
foreach(name dma_chain_snapshot vu1_xgkick_tail gif_arbiter_order)
    dbziw_runtime_test(${name})
endforeach()
set_tests_properties(dma_chain_snapshot gif_arbiter_order PROPERTIES TIMEOUT 20)
dbziw_runtime_test(vu1_rounding_scope)
target_compile_options(dbziw_vu1_rounding_scope_test PRIVATE ${DBZIW_STRICT_FP_FLAGS})
add_executable(dbziw_iop_ascii_compare_test tests/iop_ascii_compare_test.cpp)
target_link_libraries(dbziw_iop_ascii_compare_test PRIVATE ps2_iop)
set_target_properties(dbziw_iop_ascii_compare_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
add_test(NAME iop_ascii_compare COMMAND dbziw_iop_ascii_compare_test)
foreach(name stdio_pump log_lines_whole stdio_pump_exit deferred_log) # the pump on its own, as in DW5; exit paths: DBZIW 011; deferred debug lines: 013
    add_executable(dbziw_${name}_test tests/${name}_test.cpp "${PS2RECOMP_DIR}/ps2xRuntime/src/lib/ps2_stdio_pump.cpp")
    target_include_directories(dbziw_${name}_test PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/include")
    set_target_properties(dbziw_${name}_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
    add_test(NAME ${name} COMMAND dbziw_${name}_test)
endforeach()
set_tests_properties(stdio_pump PROPERTIES TIMEOUT 20)
set_tests_properties(log_lines_whole PROPERTIES TIMEOUT 30)
set_tests_properties(deferred_log PROPERTIES TIMEOUT 20)
# DBZIW 013 (DW5 023 fix 2): the window shows each field once; it gives up waiting only when the game stalls.
add_executable(dbziw_presentation_wait_test tests/presentation_wait_test.cpp)
target_include_directories(dbziw_presentation_wait_test PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/include")
set_target_properties(dbziw_presentation_wait_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
add_test(NAME presentation_wait COMMAND dbziw_presentation_wait_test)
set_tests_properties(presentation_wait PROPERTIES TIMEOUT 20)
foreach(name vu0_macro vu1_fmac_fast ee_rounding) # bit-exact float results
    dbziw_runtime_test(${name})
    target_compile_options(dbziw_${name}_test PRIVATE ${DBZIW_STRICT_FP_FLAGS})
endforeach()
add_executable(dbziw_field_history_test tests/field_history_test.cpp)
target_include_directories(dbziw_field_history_test PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/include")
set_target_properties(dbziw_field_history_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
add_test(NAME field_history COMMAND dbziw_field_history_test)

dbziw_runtime_test(vu1_async_queue)
set_tests_properties(vu1_async_queue PROPERTIES TIMEOUT 20)
# DW5 ran these two from its lifted-code test; here a synthetic microprogram stands alone, one process per path.
add_executable(dbziw_vu1_dma_reset_test tests/vu1_dma_reset_test.cpp)
target_link_libraries(dbziw_vu1_dma_reset_test PRIVATE ps2_runtime)
target_include_directories(dbziw_vu1_dma_reset_test PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/src/lib" "${PS2RECOMP_DIR}/ps2xRuntime/include")
target_compile_options(dbziw_vu1_dma_reset_test PRIVATE ${DBZIW_ARCH_FLAGS} ${DBZIW_STRICT_FP_FLAGS})
set_target_properties(dbziw_vu1_dma_reset_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
add_test(NAME vu1_dma_reset_interpreter COMMAND dbziw_vu1_dma_reset_test interpreter)
add_test(NAME vu1_dma_reset_lift COMMAND dbziw_vu1_dma_reset_test lift)
set_tests_properties(vu1_dma_reset_interpreter vu1_dma_reset_lift PROPERTIES TIMEOUT 20)

# Issue 010 profiling markers (a debugging tool, DW5 0b91b62: not part of CI or normal test runs): built and run only
# in diagnostic builds. The first test compiles them in itself; the _off test checks that a build without
# DBZIW_DEVELOPER_DIAGNOSTICS only gets empty inline functions.
if(DBZIW_DEVELOPER_DIAGNOSTICS)
    find_package(Threads REQUIRED)
    add_executable(dbziw_profile_markers_test tests/profile_markers_test.cpp
        "${PS2RECOMP_DIR}/ps2xRuntime/src/lib/ps2_profile_markers.cpp")
    target_include_directories(dbziw_profile_markers_test PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/include")
    target_compile_definitions(dbziw_profile_markers_test PRIVATE DBZIW_DEVELOPER_DIAGNOSTICS=1)
    target_link_libraries(dbziw_profile_markers_test PRIVATE Threads::Threads)
    set_target_properties(dbziw_profile_markers_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
    add_test(NAME profile_markers COMMAND dbziw_profile_markers_test)
    add_executable(dbziw_profile_markers_off_test tests/profile_markers_off_test.cpp)
    target_include_directories(dbziw_profile_markers_off_test PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/include")
    set_target_properties(dbziw_profile_markers_off_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
    add_test(NAME profile_markers_off COMMAND dbziw_profile_markers_off_test)
endif()
# ThreadSanitizer variant: the queue, PS2Memory, VIF1 parser and GIF arbiter are recompiled instrumented; their
# archive copies are then never pulled in.
option(DBZIW_TSAN_TESTS "Build the ThreadSanitizer variant of the VU1 async queue test" ON)
if(DBZIW_TSAN_TESTS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT WIN32)
    set(DBZIW_RUNTIME_SRC "${PS2RECOMP_DIR}/ps2xRuntime/src/lib")
    add_executable(dbziw_vu1_async_queue_tsan_test
        tests/vu1_async_queue_test.cpp
        "${DBZIW_RUNTIME_SRC}/ps2_device_queue.cpp"
        "${DBZIW_RUNTIME_SRC}/ps2_memory.cpp"
        "${DBZIW_RUNTIME_SRC}/ps2_vif1_interpreter.cpp"
        "${DBZIW_RUNTIME_SRC}/gs/ps2_gif_arbiter.cpp")
    target_link_libraries(dbziw_vu1_async_queue_tsan_test PRIVATE ps2_runtime)
    target_compile_options(dbziw_vu1_async_queue_tsan_test PRIVATE ${DBZIW_ARCH_FLAGS} -fsanitize=thread -g -O1)
    target_link_options(dbziw_vu1_async_queue_tsan_test PRIVATE -fsanitize=thread)
    set_target_properties(dbziw_vu1_async_queue_tsan_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
    add_test(NAME vu1_async_queue_tsan COMMAND dbziw_vu1_async_queue_tsan_test)
    set_tests_properties(vu1_async_queue_tsan PROPERTIES TIMEOUT 20 ENVIRONMENT "TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1")
endif()

if(NOT WIN32 AND TARGET dbziw_gsrec) # drives the tools through a POSIX shell (env -u, quoting, wait status)
    add_executable(dbziw_gsreplay_test tests/gsreplay_test.cpp)
    target_link_libraries(dbziw_gsreplay_test PRIVATE ps2_runtime)
    target_include_directories(dbziw_gsreplay_test PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/src/lib" "${DBZIW_GSREPLAY_DIR}")
    target_compile_options(dbziw_gsreplay_test PRIVATE ${DBZIW_ARCH_FLAGS})
    set_target_properties(dbziw_gsreplay_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
    add_test(NAME gsreplay_synthetic COMMAND dbziw_gsreplay_test $<TARGET_FILE:dbziw_vif1_replay>
             $<TARGET_FILE:dbziw_gsrec> "${CMAKE_CURRENT_BINARY_DIR}/gsreplay_test")
endif()

# Game executable (when generated sources exist). ON by default, as in DW5 (6ee9b0b): normal builds and the BUILDING.md
# steps get the game; OFF builds only what needs no game data (CI).
option(DBZIW_BUILD_GAME "Build the game (needs locally generated code)" ON)
if(DBZIW_BUILD_GAME)
    # DW5 042: generated functions may come in indexed batches, keeping the same ordered unity groups of 64.
    dbziw_generated_sources(dbziw "${DBZIW_GENERATED_DIR}"
        "${PS2RECOMP_DIR}/ps2xRuntime/src/main.cpp;${PS2RECOMP_DIR}/ps2xRuntime/src/lib/ps2_debug_panel.cpp"
        DBZIW_GENERATED_SOURCES DBZIW_BATCHED)
    if(DBZIW_GENERATED_SOURCES)
        add_executable(dbziw
            ${DBZIW_GENERATED_SOURCES}
            setup/wizard_console.cpp
            src/dbziw_movies.cpp
            src/dbziw_rom_profile.cpp
            src/dbziw_video_mode.cpp
        )
        target_include_directories(dbziw PRIVATE "${DBZIW_GENERATED_DIR}" "${PS2RECOMP_DIR}/ps2xRuntime/src/lib")
        target_link_libraries(dbziw PRIVATE ps2_runtime rlImGui imgui dbziw_setup_ui dbziw_movie_input)
        target_compile_definitions(dbziw PRIVATE PS2X_ENABLE_DEBUG_UI=1 DBZIW_INSTALLED_MODE=1)
        target_compile_options(dbziw PRIVATE ${DBZIW_ARCH_FLAGS} ${DBZIW_STRICT_FP_FLAGS})
        set_target_properties(dbziw PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 64)
        if(DBZIW_BATCHED)
            set_target_properties(dbziw PROPERTIES UNITY_BUILD OFF)
        endif()
        set_source_files_properties(setup/wizard_console.cpp src/dbziw_movies.cpp src/dbziw_rom_profile.cpp
            src/dbziw_video_mode.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
        # Translated VU1 microprograms (issue 012): written by dbziw_vu1lift (tools/regen.sh, the setup) from the
        # player's own executable into the ignored recomp/vu1/ folder, several files that compile in parallel. Game-derived: never committed. Without it every
        # VU1 program is interpreted.
        file(GLOB DBZIW_VU1_LIFTED_SOURCES CONFIGURE_DEPENDS "${DBZIW_RECOMP_ROOT}/vu1/dbziw_vu1_lifted*.cpp")
        if(DBZIW_VU1_LIFTED_SOURCES)
            target_sources(dbziw PRIVATE ${DBZIW_VU1_LIFTED_SOURCES})
            set_source_files_properties(${DBZIW_VU1_LIFTED_SOURCES} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
                SKIP_PRECOMPILE_HEADERS ON)
        else()
            message(STATUS "No translated VU1 code (run tools/regen.sh); VU1 microcode will be interpreted")
        endif()
        # DW5 009 v4.5 (015): the game's IOP modules translated to C++ (dbziw_ioplift: tools/regen.sh, the setup) into the
        # ignored recomp/iop/ folder. Game-derived: never committed. Without it, IOP code runs in the interpreter.
        file(GLOB DBZIW_IOP_NATIVE_SOURCES CONFIGURE_DEPENDS "${DBZIW_RECOMP_ROOT}/iop/*.cpp")
        if(DBZIW_IOP_NATIVE_SOURCES)
            target_sources(dbziw PRIVATE ${DBZIW_IOP_NATIVE_SOURCES})
            set_source_files_properties(${DBZIW_IOP_NATIVE_SOURCES} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
                SKIP_PRECOMPILE_HEADERS ON)
        else()
            message(STATUS "No translated IOP modules (run tools/regen.sh); IOP code will be interpreted")
        endif()
    endif()
    # DW5 009 v4.5 (015): the IOP translator against the interpreter, on our own test program (translated at build time).
    add_executable(dbziw_ioplift_testgen tests/ioplift_testgen.cpp)
    set(DBZIW_IOPLIFT_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/ioplift-test")
    # DW5 054 (015): a build machine of another system (Windows built on Linux: CI's windows-build) cannot run the
    # programs built here; it builds app/host_tools with its own compiler and runs those copies.
    if(CMAKE_CROSSCOMPILING AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL CMAKE_SYSTEM_NAME)
        include(ExternalProject)
        set(DBZIW_HOST_TOOLS_DIR "${CMAKE_CURRENT_BINARY_DIR}/host-tools")
        set(DBZIW_IOPLIFT_TESTGEN "${DBZIW_HOST_TOOLS_DIR}/dbziw_ioplift_testgen${CMAKE_HOST_EXECUTABLE_SUFFIX}")
        set(DBZIW_IOPLIFT "${DBZIW_HOST_TOOLS_DIR}/dbziw_ioplift${CMAKE_HOST_EXECUTABLE_SUFFIX}")
        ExternalProject_Add(dbziw_host_tools
            SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/host_tools"
            BINARY_DIR "${DBZIW_HOST_TOOLS_DIR}"
            CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
            BUILD_ALWAYS ON # rebuilt (usually a no-op) on every build, so source changes reach the host copies
            INSTALL_COMMAND ""
            BUILD_BYPRODUCTS "${DBZIW_IOPLIFT_TESTGEN}" "${DBZIW_IOPLIFT}")
        set(DBZIW_IOPLIFT_TOOLS dbziw_host_tools "${DBZIW_IOPLIFT_TESTGEN}" "${DBZIW_IOPLIFT}")
    else()
        set(DBZIW_IOPLIFT_TESTGEN $<TARGET_FILE:dbziw_ioplift_testgen>)
        set(DBZIW_IOPLIFT $<TARGET_FILE:dbziw_ioplift>)
        set(DBZIW_IOPLIFT_TOOLS dbziw_ioplift_testgen dbziw_ioplift)
    endif()
    add_custom_command(OUTPUT "${DBZIW_IOPLIFT_TEST_DIR}/iop_IOPTEST.cpp"
        COMMAND "${DBZIW_IOPLIFT_TESTGEN}" "${DBZIW_IOPLIFT_TEST_DIR}/program.bin"
        COMMAND "${DBZIW_IOPLIFT}" --raw "${DBZIW_IOPLIFT_TEST_DIR}/program.bin" 80000 IOPTEST "${DBZIW_IOPLIFT_TEST_DIR}"
        DEPENDS ${DBZIW_IOPLIFT_TOOLS} "${CMAKE_CURRENT_SOURCE_DIR}/tests/ioplift_test_program.h"
        VERBATIM)
    add_executable(dbziw_ioplift_test tests/ioplift_test.cpp "${DBZIW_IOPLIFT_TEST_DIR}/iop_IOPTEST.cpp"
        $<TARGET_OBJECTS:dbziw_iop_translator>)
    target_include_directories(dbziw_ioplift_test PRIVATE tests setup/ioplift "${PS2RECOMP_DIR}/ps2xIOP/src" "${PS2RECOMP_DIR}/ps2xIOP/include")
    target_link_libraries(dbziw_ioplift_test PRIVATE ps2_iop)
    add_test(NAME dbziw_ioplift_test COMMAND dbziw_ioplift_test)
    set_tests_properties(dbziw_ioplift_test PROPERTIES TIMEOUT 120)
    # DBZIW-TEST vu1_lift (issue 012; DW5 036): every translated VU1 program against the interpreter, on images
    # rebuilt from the player's executable. Only where translated code exists.
    if(DBZIW_VU1_LIFTED_SOURCES)
        set(DBZIW_TEST_ELF "${CMAKE_CURRENT_SOURCE_DIR}/../game/SLES_553.47" CACHE FILEPATH
            "Player's PAL executable for the vu1_lift test")
        add_executable(dbziw_vu1_lift_test tests/vu1_lift_test.cpp ${DBZIW_VU1_LIFTED_SOURCES})
        target_link_libraries(dbziw_vu1_lift_test PRIVATE ps2_runtime dbziw_setup_core)
        target_include_directories(dbziw_vu1_lift_test PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/src/lib"
            "${PS2RECOMP_DIR}/ps2xRuntime/include" "${CMAKE_CURRENT_SOURCE_DIR}")
        target_compile_options(dbziw_vu1_lift_test PRIVATE ${DBZIW_ARCH_FLAGS} ${DBZIW_STRICT_FP_FLAGS})
        set_target_properties(dbziw_vu1_lift_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
        add_test(NAME vu1_lift COMMAND dbziw_vu1_lift_test)
        set_tests_properties(vu1_lift PROPERTIES ENVIRONMENT "DBZIW_TEST_ELF=${DBZIW_TEST_ELF}" TIMEOUT 300)
    endif()
    # DBZIW-TEST: the PollSema contract under the game's own memory-card write (issue 018): compiles three generated
    # functions, so only where generated code exists, as DW5's version did.
    set(DBZIW_POLL_SEMA_SOURCES)
    if(DBZIW_GENERATED_SOURCES)
        foreach(generated FUN_00113948_0x113948.cpp PollSema_0x101df0.cpp SignalSema_0x101dc0.cpp)
            dbziw_generated_function("${DBZIW_GENERATED_DIR}" "${generated}" generated_source)
            list(APPEND DBZIW_POLL_SEMA_SOURCES "${generated_source}")
        endforeach()
    endif()
    set(dbziw_have_poll_sema_sources ON)
    if(NOT DBZIW_POLL_SEMA_SOURCES)
        set(dbziw_have_poll_sema_sources OFF)
    endif()
    foreach(source IN LISTS DBZIW_POLL_SEMA_SOURCES)
        if(NOT EXISTS "${source}")
            set(dbziw_have_poll_sema_sources OFF)
        endif()
    endforeach()
    if(dbziw_have_poll_sema_sources)
        dbziw_runtime_test(ee_poll_sema ${DBZIW_POLL_SEMA_SOURCES})
        target_include_directories(dbziw_ee_poll_sema_test PRIVATE "${DBZIW_GENERATED_DIR}" "${PS2RECOMP_DIR}/ps2xRuntime/src/lib")
    endif()
endif()

# Windows: /bigobj, stack reserve, DLL staging
get_property(DBZIW_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(dbziw_target IN LISTS DBZIW_TARGETS)
    dbziw_windows_target_options(${dbziw_target})
endforeach()
