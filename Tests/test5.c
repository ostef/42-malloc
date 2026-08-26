#include <ft_malloc.h>
#include <unistd.h>
#include <string.h>

// What the fuck is this stupid test???
// It's supposed to segfault, how the fuck do they expect me to check if the address is valid or not?

void print(char *s)
{
	write(1, s, strlen(s));
}

int main()
{
	char *addr;

	addr = malloc(16);
	free(NULL);
	free((void *)addr + 5);
	if (realloc((void *)addr + 5, 10) == NULL)
		print("Bonjours\n");
}
