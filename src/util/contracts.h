// Contracts are assert statements that are compiled if the debug flag is set.
#include <assert.h>

#ifdef DEBUG

#define CONTRACT(cond) assert(cond)

#else

#define CONTRACT(cond) ((void)0)

#endif
