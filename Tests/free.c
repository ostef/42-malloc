#include "common.h"

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

    void **allocated_pointers = (void **)alloc_func(sizeof(void *) * N);
    for (int i = 0; i < N; i += 1)
    {
        size_t size = GetRandomAllocSize();
        allocated_pointers[i] = alloc_func(size);
    }

    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    for (int i = 0; i < N; i += 1)
    {
        free_func(allocated_pointers[i]);
    }

    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    const char *name = alloc_func == malloc ? "   malloc" : "ft_malloc";

    printf("%s(N=%d) elapsed: %f ms\n", name, N, ElapsedTimeMS(start_time, end_time));
}

int main()
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);

    srand(time.tv_sec * 1000000000 + time.tv_nsec);
    Test(10000, malloc, free, realloc);

    srand(time.tv_sec * 1000000000 + time.tv_nsec);
    Test(10000, Alloc, Free, Realloc);
}
