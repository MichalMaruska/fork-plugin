cmake_minimum_required(VERSION 3.13)
# https://medium.com/@alasher/colored-c-compiler-output-with-ninja-clang-gcc-10bfe7f2b949
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
add_compile_options (-fdiagnostics-color=always)
add_subdirectory(src)

export(EXPORT fork FILE dump.cmake)

enable_testing()
add_subdirectory(test)
