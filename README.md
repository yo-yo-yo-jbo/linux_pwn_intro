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
    char buf[100] = { 0 };

    scanf("Please enter your name: ", buf);
    printf("Welcome %s!\n", buf);
    return 0;
}
```



