#include "../malloc.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>

void Test(
    int alloc_size,
    int N,
    void *(*alloc_func)(size_t),
    void (*free_func)(void *),
    void *(*realloc_func)(void *, size_t)
)
{
    (void)alloc_func;
    (void)free_func;
    (void)realloc_func;

    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    for (int i = 0; i < N; i += 1)
    {
        (void)alloc_func(alloc_size);
    }

    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    uint64_t elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000000000 + end_time.tv_nsec - start_time.tv_nsec;

    const char *name = alloc_func == malloc ? "   malloc" : "ft_malloc";
    printf("%s elapsed: %f ms\n", name, elapsed_time / 1000000.0);
}

int main()
{
    Test(10, 10000, malloc, free, realloc);
    Test(10, 10000, Allocate, Free, ResizeAllocation);

    Test(100, 10000, malloc, free, realloc);
    Test(100, 10000, Allocate, Free, ResizeAllocation);

    Test(500, 10000, malloc, free, realloc);
    Test(500, 10000, Allocate, Free, ResizeAllocation);

    Test(1000, 10000, malloc, free, realloc);
    Test(1000, 10000, Allocate, Free, ResizeAllocation);
}
