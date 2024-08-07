cmake_minimum_required(VERSION 3.0)

project(t85apu_emu VERSION 1.0.0.0 LANGUAGES C CXX)

option(T85APU_REGWRITE_BUFFER_SIZE "The size of the register write buffer. Leave at 0 to make it dynamically allocated. Default is 0." 0)

add_library(t85apu_emu ${CMAKE_CURRENT_SOURCE_DIR}/t85apu.c)
target_include_directories(t85apu_emu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(t85apu_emu PRIVATE c_std_99)
if (T85APU_REGWRITE_BUFFER_SIZE)
    target_compile_definitions(t85apu_emu PUBLIC T85APU_REGWRITE_BUFFER_SIZE=${T85APU_REGWRITE_BUFFER_SIZE})
endif()

find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(t85apu_emu PRIVATE ${MATH_LIBRARY})
endif()