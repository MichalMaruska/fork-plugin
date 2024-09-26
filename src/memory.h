#pragma once

#include <cstdlib>
// I want to track the memory usage, and warn when it's too high.
extern size_t memory_balance;

inline
void* mmalloc(size_t size)
{
    void* p = malloc(size);
    if (p) {
        memory_balance += size;
        // disable till template
#if 0
        if (memory_balance > sizeof(machineRec) + sizeof(PluginInstance) + 2000)
            ErrorF("%s: memory_balance = %ld\n", __FUNCTION__, memory_balance);
#endif

    }
    return p;
}

// #pragma message ( "Debug configuration - OK" )
inline
void
mxfree(void* p, const size_t size)
{
  memory_balance -= size;
  free(p);
}
