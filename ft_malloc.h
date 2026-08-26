#ifndef FT_MALLOC_H
#define FT_MALLOC_H

#include <stddef.h>

extern void *ft_malloc(size_t size);
extern void *ft_calloc(size_t num_elements, size_t element_size);
extern void *ft_realloc(void *ptr, size_t size);
extern void ft_free(void *ptr);

extern void *malloc(size_t size);
extern void *calloc(size_t num_elements, size_t element_size);
extern void *realloc(void *ptr, size_t size);
extern void free(void *ptr);

extern void show_alloc_mem();
extern void show_alloc_mem_better();

#endif
