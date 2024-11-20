#ifndef FT_MALLOC_H
#define FT_MALLOC_H

#include <stddef.h>

#ifdef FT_MALLOC_OVERRIDE_LIBC_MALLOC
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t new_size);
#endif

// We have these versions below just in case the linker resolves
// the wrong malloc/free/realloc and uses the one from libc, or
// if we also want to use libc's malloc e.g. for comparison
void *Allocate(size_t size);
void Free(void *ptr);
void *ResizeAllocation(void *ptr, size_t new_size);

void PrintAllocationState();

#endif
