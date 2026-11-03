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
add_executable(dbziw_gsrec "${DBZIW_GSREPLAY_DIR}/gsrec.cpp")
target_include_directories(dbziw_gsrec PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/src/lib" "${PS2RECOMP_DIR}/ps2xRuntime/include")
add_executable(dbziw_gsrender "${DBZIW_GSREPLAY_DIR}/gsrender.cpp")
target_link_libraries(dbziw_gsrender PRIVATE ps2_runtime)
target_include_directories(dbziw_gsrender PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/src/lib" "${PS2RECOMP_DIR}/ps2xRuntime/include")
add_executable(dbziw_vif1_replay "${DBZIW_GSREPLAY_DIR}/vif1_replay.cpp")
target_link_libraries(dbziw_vif1_replay PRIVATE ps2_runtime)
target_include_directories(dbziw_vif1_replay PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/src/lib" "${DBZIW_GSREPLAY_DIR}")
target_compile_options(dbziw_vif1_replay PRIVATE ${DBZIW_ARCH_FLAGS})

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
foreach(name stdio_pump log_lines_whole stdio_pump_exit) # the pump on its own, as in DW5; exit paths: DBZIW 011
    add_executable(dbziw_${name}_test tests/${name}_test.cpp "${PS2RECOMP_DIR}/ps2xRuntime/src/lib/ps2_stdio_pump.cpp")
    target_include_directories(dbziw_${name}_test PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/include")
    set_target_properties(dbziw_${name}_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
    add_test(NAME ${name} COMMAND dbziw_${name}_test)
endforeach()
set_tests_properties(stdio_pump PROPERTIES TIMEOUT 20)
set_tests_properties(log_lines_whole PROPERTIES TIMEOUT 30)
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

if(NOT WIN32) # drives the tools through a POSIX shell (env -u, quoting, wait status)
    add_executable(dbziw_gsreplay_test tests/gsreplay_test.cpp)
    target_link_libraries(dbziw_gsreplay_test PRIVATE ps2_runtime)
    target_include_directories(dbziw_gsreplay_test PRIVATE "${PS2RECOMP_DIR}/ps2xRuntime/src/lib" "${DBZIW_GSREPLAY_DIR}")
    target_compile_options(dbziw_gsreplay_test PRIVATE ${DBZIW_ARCH_FLAGS})
    set_target_properties(dbziw_gsreplay_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
    add_test(NAME gsreplay_synthetic COMMAND dbziw_gsreplay_test $<TARGET_FILE:dbziw_vif1_replay>
             $<TARGET_FILE:dbziw_gsrec> "${CMAKE_CURRENT_BINARY_DIR}/gsreplay_test")
endif()

# Game executable (when generated sources exist). ON by default, as in DW5 (6ee9b0b): normal builds and the README
# steps get the game; OFF builds only what needs no game data (CI).
option(DBZIW_BUILD_GAME "Build the game (needs locally generated code)" ON)
if(DBZIW_BUILD_GAME)
    file(GLOB DBZIW_GENERATED_SOURCES CONFIGURE_DEPENDS "${DBZIW_GENERATED_DIR}/*.cpp")
    if(DBZIW_GENERATED_SOURCES)
        add_executable(dbziw
            ${DBZIW_GENERATED_SOURCES}
            "${PS2RECOMP_DIR}/ps2xRuntime/src/main.cpp"
            "${PS2RECOMP_DIR}/ps2xRuntime/src/lib/ps2_debug_panel.cpp"
            setup/wizard_console.cpp
            src/dbziw_movies.cpp
            src/dbziw_rom_profile.cpp
        )
        target_include_directories(dbziw PRIVATE "${DBZIW_GENERATED_DIR}" "${PS2RECOMP_DIR}/ps2xRuntime/src/lib")
        target_link_libraries(dbziw PRIVATE ps2_runtime rlImGui imgui dbziw_setup_ui dbziw_movie_input)
        target_compile_definitions(dbziw PRIVATE PS2X_ENABLE_DEBUG_UI=1 DBZIW_INSTALLED_MODE=1)
        target_compile_options(dbziw PRIVATE ${DBZIW_ARCH_FLAGS} ${DBZIW_STRICT_FP_FLAGS})
        set_target_properties(dbziw PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 64)
        set_source_files_properties(setup/wizard_console.cpp src/dbziw_movies.cpp src/dbziw_rom_profile.cpp
            PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    endif()
    # DBZIW-TEST: the PollSema contract under the game's own memory-card write (issue 018): compiles three generated
    # functions, so only where generated code exists, as DW5's version did.
    set(DBZIW_POLL_SEMA_SOURCES
        "${DBZIW_GENERATED_DIR}/FUN_00113948_0x113948.cpp"
        "${DBZIW_GENERATED_DIR}/PollSema_0x101df0.cpp"
        "${DBZIW_GENERATED_DIR}/SignalSema_0x101dc0.cpp")
    set(dbziw_have_poll_sema_sources ON)
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
