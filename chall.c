#include <stdio.h>
#include <unistd.h>

static
void
say_hello(int* magic)
{
	char name[20] = { 0 };

	printf("What is your name? ");
	gets(name);
	printf("Hello %s!\n", name);

	if (*magic == 0x1337CAFE)
	{
		printf("woot!\n");
		execve("/bin/sh", NULL, NULL);
	}
}

int
main()
{
	int magic = 0;

	setbuf(stdout, NULL);

	say_hello(&magic);

	return 0;
}
