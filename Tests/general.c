#include "../malloc.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

float ElapsedTimeMS(struct timespec start, struct timespec end)
{
    uint64_t elapsed_sec = (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec;

    return elapsed_sec / 1000000.0;
}

size_t GetRandomAllocSize()
{
    static const size_t Sizes[] = {
        32, 64, 128, 512, 1024, 4096, 16839, 50000, 100000, 300000, 1 * 1024 * 1024, 10 * 1024 * 1024
    };
    static const int Num_Sizes = sizeof(Sizes) / sizeof(*Sizes);

    unsigned int index = rand();
    index = index % Num_Sizes;

    return Sizes[index];
}

#if 0
#define DebugLog(...) printf(__VA_ARGS__)
#else
#define DebugLog(...)
#endif

void Test(
    int N,
    void *(*alloc_func)(size_t),
    void (*free_func)(void *),
    void *(*realloc_func)(void *, size_t)
)
{
    (void)alloc_func;
    (void)free_func;
    (void)realloc_func;

    void *allocated_pointers[1000] = {};
    int num_allocated_pointers = 0;

    const char *name = alloc_func == malloc ? "   malloc" : "ft_malloc";

    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    for (int i = 0; i < N; i += 1)
    {
        size_t size = GetRandomAllocSize();
        DebugLog("Allocating %lu bytes\n", size);
        void *ptr = alloc_func(size);
        if (!ptr)
        {
            printf("%s: Could not allocate %lu bytes (%s)\n", name, size, strerror(errno));
            exit(1);
        }
        DebugLog("Result ptr: %p\n", ptr);

        if ((num_allocated_pointers > 0 && (unsigned int)rand() / (float)RAND_MAX < 0.2)
            || num_allocated_pointers == 10000)
        {
            unsigned int num_to_free = ((unsigned int)rand() % (num_allocated_pointers - 1)) + 1;
            for (unsigned int i = 0; i < num_to_free && num_allocated_pointers > 0; i += 1)
            {
                int index = (unsigned int)rand() % num_allocated_pointers;
                DebugLog("Freeing %d %p\n", index, allocated_pointers[index]);
                fflush(stdout);
                free_func(allocated_pointers[index]);
                allocated_pointers[index] = allocated_pointers[num_allocated_pointers - 1];
                num_allocated_pointers -= 1;
            }
        }

        allocated_pointers[num_allocated_pointers] = ptr;
        num_allocated_pointers += 1;
    }

    for (int i = 0; i < num_allocated_pointers; i += 1)
    {
        DebugLog("Freeing %d %p\n", i, allocated_pointers[i]);
        free_func(allocated_pointers[i]);
    }
    num_allocated_pointers = 0;

    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    printf("%s(N=%d) elapsed: %f ms\n", name, N, ElapsedTimeMS(start_time, end_time));
}

int main()
{
    printf("Page size: %ld\n", sysconf(_SC_PAGESIZE));

    Test(10000, malloc, free, realloc);
    Test(10000, Allocate, Free, ResizeAllocation);

    AllocationStats stats = GetAllocationStats();
    if (stats.num_allocations > 0 || stats.num_allocated_bytes > 0)
    {
        printf("Error: not all allocations were freed\n");
        exit(1);
    }

    CleanupAllocations();
}
