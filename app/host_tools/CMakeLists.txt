# DW5 054 (015): the ioplift CTest's build-time tools, built for the build machine. A cross build (Windows on Linux, CI's
# windows-build) cannot run the programs it builds, so it builds this project with the build machine's own compiler
# and runs these copies instead (app/CMakeLists.txt). Same sources as the app's dbziw_ioplift and
# dbziw_ioplift_testgen; only ps2xIOP besides the standard library.
cmake_minimum_required(VERSION 3.21)
project(dbziw_host_tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DBZIW_APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(PS2RECOMP_DIR "${DBZIW_APP_DIR}/../external/PS2Recomp")
add_subdirectory("${PS2RECOMP_DIR}/ps2xIOP" ps2xIOP EXCLUDE_FROM_ALL)

add_executable(dbziw_ioplift "${DBZIW_APP_DIR}/setup/ioplift/ioplift_main.cpp"
    "${DBZIW_APP_DIR}/setup/ioplift/iop_translator.cpp" "${DBZIW_APP_DIR}/setup/ioplift/iop_generate.cpp")
target_include_directories(dbziw_ioplift PRIVATE "${DBZIW_APP_DIR}/setup/ioplift" "${PS2RECOMP_DIR}/ps2xIOP/include"
    "${PS2RECOMP_DIR}/ps2xIOP/src")
target_link_libraries(dbziw_ioplift PRIVATE ps2_iop)

add_executable(dbziw_ioplift_testgen "${DBZIW_APP_DIR}/tests/ioplift_testgen.cpp")
