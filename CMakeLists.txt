project(t85apu_emu VERSION 1.0.0.0 LANGUAGES C CXX)
add_library(t85apu_emu STATIC t85apu.h t85apu.hpp t85apu.c)
target_include_directories(t85apu_emu PUBLIC {CMAKE_CURRENT_SOURCE_DIR}/emu/libt85apu)