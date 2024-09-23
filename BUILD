* Build with cmake:


mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=debug -G Ninja  ../

cmake --build .

* debug:
script zapis  -c ninja


* Xorg headers files not clean to be used by C++ compiler

** min/max macros

** wrong files picked by:
#include <math.h>
#include <stdlib.h>

*** fix in:
/usr/include/xorg/os.h
/usr/include/xorg/misc.h

#define _GLIBCXX_INCLUDE_NEXT_C_HEADERS 1
#include
#undef _GLIBCXX_INCLUDE_NEXT_C_HEADERS


sudo chown michal  /usr/include/xorg/misc.h


* bugs:
error: 'KeyCode machineRec::forkActive [256]' is private within this context

error: redeclaration of 'Time machineRec::mCurrent_time'


* CMake Error: INSTALL(EXPORT) given unknown export "EXPORT_module"



* ctest:
ai libgtest-dev

g++ test-circular.cpp
g++ test-circular.cpp  -lgtest

cd tests; ctest

# now that I  moved enable_testing() to the root:
cmake --build . -t test


* invoke single target:
ninja src/CMakeFiles/fork.dir/history.cpp.o

ai libgmock-dev

https://www.reddit.com/r/cpp/comments/iwbdx7/is_ctest_worth_the_effort/


If you use gtest_discover_tests from the GoogleTest CMake module instead
of add_test then every unit test will be added as a separate CTest test
with its full name (MyTest.TestA). This makes it possible to execute
exactly one unit test by using ctest -R MyTest.TestA.



* idea for: C & C++
#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif
EXTERNC void mylibrary_mytype_doit(mylibrary_mytype_t self, int param);

#undef EXTERNC
