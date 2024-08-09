# include(CTest)
# add_test(NAME test-circular COMMAND ExecutableToRun arg1 arg2 ...)

include(GoogleTest)


# this was crucial to use GTest::gtest_main
find_package(GTest REQUIRED)
# include(GTest/GTestTargets)

add_executable(test-circular test-circular.cpp)
# target_link_libraries(TestInstantiator vtkCommon)
#       set_property(TEST test_name
#             PROPERTY prop1 value1 value2 ...)
target_link_libraries(test-circular GTest::gtest_main)

add_test(NAME test-circular
         COMMAND test-circular)
gtest_discover_tests(test-circular)


add_executable(test-queue test-queue.cpp)
target_link_libraries(test-queue GTest::gtest_main)

add_test(NAME test-queue COMMAND test-queue)
gtest_discover_tests(test-queue)




