
set(sources
  # main.cpp
)

if(false)
add_library(kernel MODULE ${sources})

target_include_directories(kernel PRIVATE ../src/)
target_include_directories(kernel PRIVATE AFTER ".")
endif()

# link with libinput
