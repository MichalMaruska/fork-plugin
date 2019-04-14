cmake_minimum_required(VERSION 3.13)

find_package(PkgConfig)

# no _RECURSE
file(GLOB sources "*.cpp")
add_library(fork SHARED ${sources})

pkg_search_module(xorg-server REQUIRED xorg-server)
# /usr/lib/x86_64-linux-gnu/pkgconfig/xorg-server.pc
# or SDK?
target_include_directories(fork SYSTEM PUBLIC ${xorg-server_INCLUDE_DIRS})
target_link_libraries(fork ${xorg-server_LIBRARIES})

pkg_get_variable(moduledir xorg-server moduledir)
pkg_get_variable(includedir xorg-server sdkdir)


message(STATUS "Building Fork & install in " ${moduledir})
message(STATUS "USing Xorg headers from " ${includedir})

# PROJECT_BINARY_DIR
# ${PROJECT_SOURCE_DIR}/
include_directories(../include/)

set(CMAKE_MY_NAME ""
  CACHE STRING "Flags used by the compiler during all build types.")

# $<INSTALL_PREFIX>
install(FILES ../include/fork_requests.h
  #PUBLIC_HEADER
  DESTINATION ${includedir}
  )

install(TARGETS fork
  LIBRARY    DESTINATION ${moduledir}
  # CMAKE_INSTALL_PREFIX CMAKE_INSTALL_LIBDIR
  )
