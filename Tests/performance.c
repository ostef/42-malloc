#include "common.h"

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

    const char *name = alloc_func == malloc ? "   malloc" : "ft_malloc";

    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    for (int i = 0; i < N; i += 1)
    {
        void *ptr = alloc_func(alloc_size);
        if (!ptr)
        {
            printf("%s: Could not allocate %d bytes (%s)\n", name, alloc_size, strerror(errno));
            exit(1);
        }
    }

    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    uint64_t elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000000000 + end_time.tv_nsec - start_time.tv_nsec;

    printf("%s(alloc_size=%d, N=%d) elapsed: %f ms\n", name, alloc_size, N, elapsed_time / 1000000.0);
}

int main()
{
    printf("Page size: %ld\n", sysconf(_SC_PAGESIZE));

    Test(10, 10000, malloc, free, realloc);
    Test(10, 10000, Alloc, Free, ResizeAlloc);

    Test(100, 10000, malloc, free, realloc);
    Test(100, 10000, Alloc, Free, ResizeAlloc);

    Test(500, 10000, malloc, free, realloc);
    Test(500, 10000, Alloc, Free, ResizeAlloc);

    Test(1000, 10000, malloc, free, realloc);
    Test(1000, 10000, Alloc, Free, ResizeAlloc);

    Test(10000, 10000, malloc, free, realloc);
    Test(10000, 10000, Alloc, Free, ResizeAlloc);

    Test(100000, 10, malloc, free, realloc);
    Test(100000, 10, Alloc, Free, ResizeAlloc);

    Test(10000000, 10, malloc, free, realloc);
    Test(10000000, 10, Alloc, Free, ResizeAlloc);

    printf("\n");
    // PrintAllocationState();
    DestroyGlobalHeap();
}
