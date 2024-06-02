# Introduction to Linux pwn
I hope this blogpost would be a nice introduction to Linux pwn challenges, I intend on doing a multi-part series on the subject.  
The idea behind those challenges is usually gaining arbitrary code execution capabilities, either remotely or locally through the use of [SUID binary](https://en.wikipedia.org/wiki/Setuid) files.  
In this blogpost I'll show the simplest example that I hand-coded. We will focus primarily on C and Linux, but not a lot of background is necessary for now!

## First example
Here's a toy examine for us to begin:

```c
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
```

Let us analyze the code:
1. Function `say_hello` gets a pointer to a variable called `magic`. Then, it gets a `name` from `stdin` (from the standard input, aka "the keyboard") and prints out a greeting.
2. If the `magic` value supplied to the function `say_hello` is exactly `0x1337CAFE` (a completely random value I chose for this exercise!) then we print `woot` and execute `/bin/sh`, i.e. "getting a shell".
3. The `main` function simply called `say_hello` with a `magic` value that was initialized to `0`, hence we should never fulfill the condition to get us a shell.
4. Also notice `setbuf(stdout, NULL)` - this basically replaces `fflush` on `stdout` and simply makes sure we print out immidiately without buffering. It's not super important for now.


