cmake_minimum_required(VERSION 3.0)

project(t85apu VERSION 1.0.0.0 LANGUAGES C CXX)

add_subdirectory(emu)
add_subdirectory(examples EXCLUDE_FROM_ALL)