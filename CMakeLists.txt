cmake_minimum_required(VERSION 3.0)

project(t85apu_emu VERSION 1.0.0.0 LANGUAGES C CXX)

option(T85APU_REGWRITE_BUFFER_SIZE "The size of the register write buffer. Leave at 0 to make it dynamically allocated. Default is 0." 0)

add_library(t85apu_emu ${CMAKE_CURRENT_SOURCE_DIR}/emu/libt85apu/t85apu.c)
target_include_directories(t85apu_emu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/emu/libt85apu)
target_compile_features(t85apu_emu PRIVATE c_std_99)
target_compile_definitions(t85apu_emu PUBLIC T85APU_REGWRITE_BUFFER_SIZE=${T85APU_REGWRITE_BUFFER_SIZE})
target_link_libraries(t85apu_emu PRIVATE m)

