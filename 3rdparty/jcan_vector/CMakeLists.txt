# Vendored Vector VN1640A stack extracted from ImArjunJ/jcan (MIT).
# Linux-only. Requires libusb-1.0 and a C++23 compiler for the implementation TU.

cmake_minimum_required(VERSION 3.16)
project(jcan_vector LANGUAGES C CXX ASM)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

set(JCAN_FW_MAIN_BIN "${CMAKE_CURRENT_SOURCE_DIR}/firmware/vn1640a_main_fw.bin")
set(JCAN_FW_FPGA_BIN "${CMAKE_CURRENT_SOURCE_DIR}/firmware/vn1640a_fpga.bin")
set(JCAN_ASM_RODATA_SECTION ".rodata")
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/firmware_blobs.S.in
  ${CMAKE_CURRENT_BINARY_DIR}/firmware_blobs.S
  @ONLY
)

add_library(jcan_vector STATIC
  src/internal/discovery_vector.cpp
  src/vector_device.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/firmware_blobs.S
)

add_library(jcan_vector::jcan_vector ALIAS jcan_vector)

target_include_directories(jcan_vector
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(jcan_vector PUBLIC cxx_std_17)
set_target_properties(jcan_vector PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
# CMake < 3.20 may ignore CXX_STANDARD 23; force the dialect for g++/clang.
target_compile_options(jcan_vector PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++23>)

target_compile_definitions(jcan_vector PUBLIC JCAN_HAS_VECTOR=1)
target_link_libraries(jcan_vector PUBLIC PkgConfig::LIBUSB)

option(JCAN_VECTOR_BUILD_DEMO "Build jcan_vector_list demo" ON)
if(JCAN_VECTOR_BUILD_DEMO)
  add_executable(jcan_vector_list src/demo_list.cpp)
  target_link_libraries(jcan_vector_list PRIVATE jcan_vector)
  target_compile_features(jcan_vector_list PRIVATE cxx_std_17)
endif()
