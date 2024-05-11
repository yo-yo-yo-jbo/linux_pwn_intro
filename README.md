# Introduction to Linux pwn
I hope this blogpost would be a nice introduction to Linux pwn challenges.  
The idea behind those kind of challenges is usually to gain an arbitrary code execution capability.  
In most cases you'll have a [SUID binary](https://en.wikipedia.org/wiki/Setuid) that runs as a specific user that has read access to a certain `flag.txt` file.  
By exploiting vulnerabilities in the SUID binary and eventually gaining code execution capabilities, it'd be possible to read the contents of `flag.txt`, which contains the `flag` for the challenge.  
So, in this blogpost I hope to explain a bit about memory corruption, mitigations you'd probably find in Linux [ELF files](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) and why they are not silver bullets.  
If you feel you have good experience with `pwn` challenges, I invite you to try the challenge I hand-coded for this blogpost, it exists in this repository!
