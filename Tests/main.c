#include "common.h"

bool Test(
    int N,
    void *(*alloc_func)(size_t),
    void (*free_func)(void *),
    void *(*realloc_func)(void *, size_t)
)
{
    (void)alloc_func;
    (void)free_func;
    (void)realloc_func;

    int *ptr1 = (int *)alloc_func(sizeof(int) * N);
    if (!ptr1)
    {
        printf("Error: %s\n", strerror(errno));
        return false;
    }

    int *ptr2 = (int *)alloc_func(sizeof(int) * N);
    if (!ptr2)
    {
        printf("Error: %s\n", strerror(errno));
        return false;
    }

    for (int i = 0; i < N; i += 1)
    {
        ptr2[i] = N + i;
        ptr1[i] = i;
    }

    for (int i = 0; i < N; i += 1)
        printf("%d ", ptr1[i]);

    for (int i = 0; i < N; i += 1)
        printf("%d ", ptr2[i]);

    printf("\n");

    free_func(NULL);
    ptr1 = realloc_func(ptr1, sizeof(int) * N * 2);
    int *ptr3 = (int *)realloc_func(NULL, sizeof(int) * N);

    free_func(ptr1);
    free_func(ptr3);
    free_func(ptr2);

    return true;
}

int main()
{
    if (!Test(10, malloc, free, realloc))
        return 1;

    if (!Test(10, Alloc, Free, Realloc))
        return 1;

    if (!Test(50, malloc, free, realloc))
        return 1;

    if (!Test(50, Alloc, Free, Realloc))
        return 1;

    PrintAllocationState();
    DestroyGlobalHeap();

    return 0;
}
