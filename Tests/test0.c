#include <ft_malloc.h>

int main()
{
	int i;

	i = 0;
	while (i < 1024)
	{
		i++;
	}

	show_alloc_mem_better();

	return (0);
}
