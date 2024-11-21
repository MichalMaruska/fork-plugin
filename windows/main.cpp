
#define KERNEL 1
#define DISABLE_SWAP 1
#define DISABLE_EQUAL 1

// ------------------------------------------------
using size_t = unsigned long;
// #include <cstddef>
using ptrdiff_t = long;
enum POOL_TYPE {
    NonPagedPool,
};

void* ExAllocatePoolWithTag(POOL_TYPE type, size_t size, long tag);
void* ExAllocatePool(POOL_TYPE type, size_t size);
#define KdPrint(x)  while(0) {}

// ------------------------------------------------



#include "circular.h"
// #include "machine.h"


void test()
{
};

