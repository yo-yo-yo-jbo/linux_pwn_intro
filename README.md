# Introduction to Linux pwn
I hope this blogpost would be a nice introduction to Linux pwn challenges.  
The idea behind those kind of challenges is usually to gain an arbitrary code execution capability.  
In most cases you'll have a [SUID binary](https://en.wikipedia.org/wiki/Setuid) that runs as a specific user that has read access to a certain `flag.txt` file.  
By exploiting vulnerabilities in the SUID binary and eventually gaining code execution capabilities, it'd be possible to read the contents of `flag.txt`, which contains the `flag` for the challenge.  
So, in this blogpost I hope to explain a bit about memory corruption, mitigations you'd probably find in Linux [ELF files](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) and why they are not silver bullets.  
If you feel you have good experience with `pwn` challenges, I invite you to try the challenge I hand-coded for this blogpost, it exists in this repository!

## Memory corruption in a nutshell
There are many classes of memory corruption bugs that we could discuss, but I think that I'd name just a few.

### Stack-based buffer overflow
Coming straight from the 80's, stack based buffer overflow abuse the fact local ("automatic") variables are allocated on the stack, *as well as return addresses*.  
This gives means going out-of-bounds (or "overflowing" a buffer) eventually reaches that return address that was saved on the stack, overriding it with a value we control.  
Of course, there are other implications of overflowing the stack (e.g. before we touch the return address we might affect other local variables) but we will leave that part alone for now.  
The most prevalent architectures nowadays are `Intel` and `ARM`, so we can describe what happens on those:

- On Intel architectures, everytime we use a `call` instruction we essentially push the `RIP` (instruction pointer) on the stack and `jmp` somewhere else (changing `RIP` value), so it's obvious the return address is saved on the stack.
- In ARM architectures, calling a function is done via the `bl` instruction (or `blx` if the CPU's mode might change). This places the `PC` register (the program counter register) onto another register called `LR` (called Link register) and then changes `PC` to point to the destination address. While this has no pushes to the stack, it means the *old* `LR` value needs to be saved somewhere, which is the stack (unless we have a leaf function or some compiler optimization).

How does one overflow the stack? Commonly there are a few unsafe C runtime functions (such as [sprintf](https://cplusplus.com/reference/cstdio/sprintf/) or [strcpy](https://cplusplus.com/reference/cstdio/strcpy)) as well as raw memory operations (such as assigning a value in an array with an attacker controlled index) that can cause out-of-bounds.  
Let's see an example!

```c
#include <stdio.h>
#include <string.h>

int main()
{
    char buf[10] = { 0 };

    printf("Please enter your name: ");
    scanf("%s", buf);
    printf("Welcome %s!\n", buf);
    return 0;
}
```

Here, supplying an input larger than 9 characters overflows the stack. If you think it's an unrealistic scenario you are more than welcome to read my blogpost about a [remotely triggerable buffer overflow in ChromeOS](https://www.microsoft.com/en-us/security/blog/2022/08/19/uncovering-a-chromeos-remote-memory-corruption-vulnerability/) I reported the other day.  
Note that even `main` has a return address, so the scenario is totally viable. Here's what happens on a normal Linux box:

```shell
jbo@jbo-nix:~/pwn$ gcc -O0 -opwn ./pwn.c
jbo@jbo-nix:~/pwn$ ./pwn
Please enter your name: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
Welcome AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA!
*** stack smashing detected ***: terminated
Aborted (core dumped)
```

I will talk about that `*** stack smashing detected ***` part later, but that's basically a sign we have overflown the stack. Also note I use the `-O0` flag to dsiable `gcc` optimizations.  
If we wanted to be more subtle, we could abuse a stack-based buffer overflow for overriding a local variable:

```c
#include <stdio.h>
#include <string.h>

typedef struct
{
    char name[10];
    char prefix[10];
} greeting_t;

int main()
{
    greeting_t greeting = { 0 };

    strcpy(greeting.prefix, "Hello");
    printf("Please enter your name: ");
    scanf("%s", greeting.name);
    printf("%s %s!\n", greeting.prefix, greeting.name);
    return 0;
}
```

And the output:

```shell
jbo@jbo-nix:~/pwn$ gcc -O0 -opwn ./pwn.c
jbo@jbo-nix:~/pwn$ ./pwn
Please enter your name: AAAAAAAAAAGoodbye
Goodbye AAAAAAAAAAGoodbye!
```

Note how surprising it is, we were expecting to see `Hello` and instead got `Goodbye`, due to overflowing the stack.  
Note that the `struct` that I defined is there just to assure a certain order on the stack, as `gcc` has certain heuristics to arrange local variables. I do hope the point is clear!  

### Heap-based buffer overflow
The same idea but applies to the program `heap` instead of the `stack`. The heap is where all the [dynamic allocations](https://en.wikipedia.org/wiki/C_dynamic_memory_allocation) occur (such as `malloc`, `calloc`, `free`).  
It is a fascinating subject since exploitation here depends a lot on the heap implementation (several popular ones are `dlmalloc`, `ptmalloc`, `jemalloc` and of course the `glibc heap` which was derived from `ptmalloc`).  
There are several ideas when it comes to heap overflows that focus on modifying the heap metadata (which is usually saved a few bytes before an allocated chunk).

### Out-of-bounds
These usually happen when a programmer doesn't take certain edge cases into account. Some of those could just be `off-by-one` bugs, others could just forget about negative numbers entirely!  
Here's a nice example:

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define GAME_SIZE (50)

int main()
{
    int game[GAME_SIZE] = { 0 };
    int choice = 0;

    // Choose a random value
    srand(time(NULL));
    game[rand() % GAME_SIZE] = 1;

    // Get the user choice
    printf("Please enter a number between 0 and %d: ", GAME_SIZE - 1);
    scanf("%d", &choice);
    if (GAME_SIZE - 1 < choice)
    {
        printf("Invalid choice!\n");
        return -1;
    }

    // Check user choice
    if (game[choice])
    {
        printf("Winner!\n");
    }
    else
    {
        printf("Loser!\n");
    }

    return 0;
}
```

There is a check here that ensures the user choice doesn't exceed the game array. However, the choice is a *signed* integer so it might be negative!

```shell
jbo@jbo-nix:~/pwn$ ./pwn
Please enter a number between 0 and 49: -1
Winner!
```

### Integer overflows
Integer overflows and underflows are very hard to spot, and therefore, very hard to detect, and they could lead to out-of-bounds access that turns into a buffer overflow.  
`Integer overflows` are cases where an integer goes beyond its variable size. For example, take a look at the following code and execution:

```c
#include <stdio.h>

int main()
{
    unsigned short len = 0;
    char name[256] = { 0 };

    printf("Please enter the number of random characters you wish to generate: ");
    scanf("%u", &len);
    len += 6;        // Room for prefix

    

    return 0;
}
```


