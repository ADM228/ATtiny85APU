cmake_minimum_required(VERSION 3.0)

project(t85apu_examples VERSION 1.0.0.0 LANGUAGES C CXX)

add_executable(example_c ${CMAKE_CURRENT_SOURCE_DIR}/example.c)
target_link_libraries(example_c PRIVATE t85apu_emu)