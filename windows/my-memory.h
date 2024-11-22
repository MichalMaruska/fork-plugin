#pragma once
// my-memory.h

// I want Allocator

// default constructible

#include <wdm.h>

/*

alloc_
// not used:
destroy -- call d-tor
construct -- call copy c-tor (array_ + next, item);
max_size

// used:
deallocate
deallocate(array_, array_size_);
allocate(1)
allocate(capacity)
*/

// Interesting:

//template<typename _Tp>
//    using __allocator_base = __new_allocator<_Tp>;


template<typename _Tp>
class kernelAllocator //: public __allocator_base<_Tp>
{
public:
      typedef _Tp        value_type;
      typedef size_t     size_type;
      typedef ptrdiff_t  difference_type;

#if __cplusplus <= 201703L
      // These were removed for C++20.
      typedef _Tp*       pointer;
      typedef const _Tp* const_pointer;
      typedef _Tp&       reference;
      typedef const _Tp& const_reference;
#endif
private:
      const static long tag = (long) 'mike';
  // _drv_strictTypeMatch(__drv_typeExpr)POOL_TYPE PoolType,
      const static POOL_FLAGS mem_type = POOL_FLAG_PAGED; // NonPagedPool POOL_TYPE

public:
      // default c-tible
      kernelAllocator() noexcept { };
      ~kernelAllocator() noexcept { }

      // constexpr
      static _Tp*
      allocate(size_t size) noexcept {
          size *=  sizeof(value_type);

          auto* p = ExAllocatePool2(mem_type, size, tag);
          // (tag == 0) ? : ExAllocatePool2(mem_type, size, tag);

          if (p == nullptr) {
              // output to debugger if it's attached?
              KdPrint(("Failed to allocate %lu bytes\n", size));
          } else {
              KdPrint(("Allocated %lu bytes\n", size));
          };

          return (pointer) p;
      }


      static void
      deallocate(_Tp* __p, size_t __n)
      {
          UNREFERENCED_PARAMETER(__n);
          ExFreePool(__p);
      }

      void construct(pointer p, value_type const& val) {
          new(p) value_type(val);
      }

      void
      destroy(pointer object) {
          object->~value_type();
      }

#if 0
      friend __attribute__((__always_inline__)) _GLIBCXX20_CONSTEXPR
      bool
      operator==(const allocator&, const allocator&) _GLIBCXX_NOTHROW
      { return true; }

#if __cpp_impl_three_way_comparison < 201907L
      friend __attribute__((__always_inline__)) _GLIBCXX20_CONSTEXPR
      bool
      operator!=(const allocator&, const allocator&) _GLIBCXX_NOTHROW
      { return false; }
#endif
#endif
      // Inherit everything else.
};
