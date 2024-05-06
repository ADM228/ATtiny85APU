project(t85apu_emu VERSION 1.0.0.0 LANGUAGES C CXX)
add_library(t85apu_emu ${CMAKE_CURRENT_SOURCE_DIR}/emu/libt85apu/t85apu.c)
target_include_directories(t85apu_emu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/emu/libt85apu)
target_compile_features(t85apu_emu PRIVATE c_std_99)