cmake_minimum_required(VERSION 3.28)
project(ex7-app)

include(../../nanopi.cmake)
add_executable(user user.c ../commun/commun.h)

target_include_directories(user PRIVATE ../commun)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(TOOLCHAIN_BIN_DIR "/buildroot/output/host/usr/bin")
set(CMAKE_C_COMPILER "${TOOLCHAIN_BIN_DIR}/aarch64-linux-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_BIN_DIR}/aarch64-linux-g++")

add_compile_options(-mcpu=cortex-a53 -funwind-tables)