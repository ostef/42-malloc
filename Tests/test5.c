#include <ft_malloc.h>
#include <assert.h>

int main()
{
    void *ptr;

	ptr = malloc(1024);
	ptr = malloc(1024 * 32);
	ptr = malloc(1024 * 1024);
	ptr = malloc(1024 * 1024 * 16);
	ptr = malloc(1024 * 1024 * 128);
	show_alloc_mem();
	return (0);
}
