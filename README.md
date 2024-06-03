# Introduction to Linux pwn
I hope this blogpost would be a nice introduction to Linux pwn challenges, I intend on doing a multi-part series on the subject.  
The idea behind those challenges is usually gaining arbitrary code execution capabilities, either remotely or locally through the use of [SUID binary](https://en.wikipedia.org/wiki/Setuid) files.  
In this blogpost I'll show the simplest example that I hand-coded. We will focus primarily on C and Linux, but not a lot of background is necessary for now!  
Remark: I will mostly cover the Intel architecture (64 bit). There are substantial differences between 32 and 64 bit, and even more when we talk about other architectures (e.g. ARM).  
However, pwn data is transferrable in a general sense, so let us not worry about that now.

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

Well, let us compile using `gcc`, but with a special flag I will mention shortly: `-fno-stack-protector`. I also use `-w` to ignore all warnings.  
Since I do not like typing a lot, I put things in a `Makefile`:

```
CC=gcc
CFLAGS=-fno-stack-protector -w

chall: chall.c
	$(CC) $(CFLAGS) -o chall chall.c

clean:
	rm -f chall
```

Upon compilation:

```shell
gcc -fno-stack-protector -w -o chall chall.c
/usr/bin/ld: /tmp/ccODuVW9.o: in function `say_hello':
chall.c:(.text+0x48): warning: the `gets' function is dangerous and should not be used.
```

That's just a warning (even though I ignored the warnings with `-w`!), but a big hint on the issue.  
After successfully compiling, let's run:

```shell
$ ./chall
What is your name? JBO
Hello JBO!
```

So far so good. Although, take a look at this!

```shell
$ printf "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\xFE\xCA\x37\x13" | ./chall
What is your name? Hello AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA��7!
woot!
```

Looks like we satisfied the condition of `*magic == 0x1337CAFE`! How?

## What happened
As I "gently" hinted, the `gets` function is the one doing all the harm. Note that `gets` does not get the target buffer length as an input.  
As a result, `gets` is condidered *highly* dangerous as one can simply write more bytes than intended, and this is all it takes here.  
In our example, the buffer size is `20` bytes, but we wrote `64` bytes instead! This raises the question - what happens "beyond" the 20 bytes of the `name` variable?  

### The stack
In most modern architectures, local variables are saved in a special memory region called "the stack". Actually, there's one stack per-thread, but we're dealing with a single-threaded process so the name *the* stack is justified.  
The stack is interesting - it allows `push` and `pop` operations (in the `Intel` architecture - ARM is a bit more "raw" but most of its Assemblers has "push" and "pop" macros still) and most importantly - it grows *down* - when you `push`, you 

