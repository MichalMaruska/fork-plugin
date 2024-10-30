cmake_minimum_required(VERSION 3.13)


set(sources
  fork-plugin.cpp

)


add_library(fork-libinput MODULE ${sources})

target_compile_definitions(fork-libinput PRIVATE VERSION_STRING="git-local-2")

# include dir
target_include_directories(fork-libinput PRIVATE ../src/ ../include/)
set_property(TARGET fork-libinput PROPERTY NO_SONAME)

# link with libinput
