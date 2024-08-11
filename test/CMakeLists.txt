include(GoogleTest)

# include(CTest)
# add_test(NAME test-circular COMMAND ExecutableToRun arg1 arg2 ...)

# this was crucial to use GTest::gtest_main
find_package(GTest REQUIRED)
# find_package(GMock REQUIRED)
# include(GTest/GTestTargets)

add_executable(test-circular test-circular.cpp)
target_link_libraries(test-circular GTest::gtest_main)

gtest_discover_tests(test-circular)


add_executable(test-queue test-queue.cpp)
target_link_libraries(test-queue GTest::gtest_main)

gtest_discover_tests(test-queue)



find_package(PkgConfig)
pkg_search_module(xorg-server REQUIRED xorg-server
  IMPORTED_TARGET GLOBAL)

add_executable(test-machine test-machine.cpp ../src/machine.cpp)
target_include_directories(test-machine SYSTEM PUBLIC ${xorg-server_INCLUDE_DIRS})
target_link_libraries(test-machine GTest::gtest_main)
target_link_libraries(test-machine GTest::gmock_main)

gtest_discover_tests(test-machine)




