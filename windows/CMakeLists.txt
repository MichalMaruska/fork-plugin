
set(sources
  main.cpp
)

add_library(kernel MODULE ${sources})

target_include_directories(kernel PRIVATE ../src/)
target_include_directories(kernel PRIVATE AFTER ".")


# link with libinput
