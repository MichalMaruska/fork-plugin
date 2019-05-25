#if USE_LOCKING
#define CHECK_LOCKED(m)   assert((m)->lock)
#define CHECK_UNLOCKED(m) assert((m)->lock == 0)
// might be: CHECK_UNLOCKED(m)      ((m)->lock==1)

#define LOCK(m)    (m)->lock=1
#define UNLOCK(m)  (m)->lock=0


#else

#error "define locking macros!"
// m UNUSED
#define CHECK_LOCKED(m) while (0) {}
#define CHECK_UNLOCKED(m) while (0) {}
#define LOCK(m)  while (0) {}
#define UNLOCK(m)while (0) {}

#endif
