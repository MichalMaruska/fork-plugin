cmake_minimum_required(VERSION 3.13)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)


# https://medium.com/@alasher/colored-c-compiler-output-with-ninja-clang-gcc-10bfe7f2b949
# force for Ninja:
option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." FALSE)
if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options(-fdiagnostics-color=always)
       add_link_options(-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       add_compile_options(-fcolor-diagnostics)
       add_link_options(-fcolor-diagnostics)
    endif ()
endif ()
#end


add_subdirectory(src)
add_subdirectory(libinput)

export(EXPORT fork FILE dump.cmake)

include(./CodeCoverage.cmake)
# ai cmake-extras
enable_testing()
add_subdirectory(test)
