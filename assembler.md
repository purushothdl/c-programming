PS D:\Ad Astra\Practice Code\Embedded\C Programming> gcc -c main.c -o main.o 


PS D:\Ad Astra\Practice Code\Embedded\C Programming> objdump -s main.o 
## Full Contents

main.o:     file format pe-x86-64

Contents of section .text:
 0000 554889e5 894d108b 45100faf c05dc355  UH...M..E....].U
 0010 4889e548 83ec20e8 00000000 b90a0000  H..H.. .........
 0020 00e8daff ffff89c2 488d0504 00000041  ........H......A
 0030 89d0ba0a 00000048 89c1e800 00000048  .......H.......H
 0040 8d051c00 00004889 c1e80000 0000b800  ......H.........
 0050 00000048 83c4205d c3909090 90909090  ...H.. ]........
Contents of section .rdata:
 0000 0a000000 54686520 73717561 7265206f  ....The square o
 0010 66202564 20697320 25640a00 54686520  f %d is %d..The 
 0020 70726f67 72616d20 69732064 6f6e6500  program is done.
Contents of section .xdata:
 0000 01040205 04030150 01080305 08320403  .......P.....2..
 0010 01500000                             .P..            
Contents of section .pdata:
 0000 00000000 0f000000 00000000 0f000000  ................
 0010 59000000 08000000                    Y.......        
Contents of section .rdata$zzz:
 0000 4743433a 20285265 76382c20 4275696c  GCC: (Rev8, Buil
 0010 74206279 204d5359 53322070 726f6a65  t by MSYS2 proje
 0020 63742920 31352e32 2e300000 00000000  ct) 15.2.0......



PS D:\Ad Astra\Practice Code\Embedded\C Programming> objdump -d main.o
## See the Machine Code (disassemble)

main.o:     file format pe-x86-64


Disassembly of section .text:

0000000000000000 <square>:
   0:   55                      push   %rbp
   1:   48 89 e5                mov    %rsp,%rbp
   4:   89 4d 10                mov    %ecx,0x10(%rbp)
   7:   8b 45 10                mov    0x10(%rbp),%eax
   a:   0f af c0                imul   %eax,%eax
   d:   5d                      pop    %rbp
   e:   c3                      ret

000000000000000f <main>:
   f:   55                      push   %rbp
  10:   48 89 e5                mov    %rsp,%rbp
  13:   48 83 ec 20             sub    $0x20,%rsp
  17:   e8 00 00 00 00          call   1c <main+0xd>
  1c:   b9 0a 00 00 00          mov    $0xa,%ecx
  21:   e8 da ff ff ff          call   0 <square>
  26:   89 c2                   mov    %eax,%edx
  28:   48 8d 05 04 00 00 00    lea    0x4(%rip),%rax        # 33 <main+0x24>
  2f:   41 89 d0                mov    %edx,%r8d
  32:   ba 0a 00 00 00          mov    $0xa,%edx
  37:   48 89 c1                mov    %rax,%rcx
  3a:   e8 00 00 00 00          call   3f <main+0x30>
  3f:   48 8d 05 1c 00 00 00    lea    0x1c(%rip),%rax        # 62 <main+0x53>
  46:   48 89 c1                mov    %rax,%rcx
  49:   e8 00 00 00 00          call   4e <main+0x3f>
  4e:   b8 00 00 00 00          mov    $0x0,%eax
  53:   48 83 c4 20             add    $0x20,%rsp
  57:   5d                      pop    %rbp
  58:   c3                      ret
  59:   90                      nop
  5a:   90                      nop
  5b:   90                      nop
  5c:   90                      nop
  5d:   90                      nop
  5e:   90                      nop
  5f:   90                      nop



PS D:\Ad Astra\Practice Code\Embedded\C Programming> objdump -r main.o
## Linker's "Sticky Notes" (relocations)

main.o:     file format pe-x86-64

RELOCATION RECORDS FOR [.text]:
OFFSET           TYPE              VALUE
0000000000000018 IMAGE_REL_AMD64_REL32  __main
000000000000002b IMAGE_REL_AMD64_REL32  .rdata
000000000000003b IMAGE_REL_AMD64_REL32  printf
0000000000000042 IMAGE_REL_AMD64_REL32  .rdata
000000000000004a IMAGE_REL_AMD64_REL32  puts


RELOCATION RECORDS FOR [.pdata]:
OFFSET           TYPE              VALUE
0000000000000000 IMAGE_REL_AMD64_ADDR32NB  .text
0000000000000004 IMAGE_REL_AMD64_ADDR32NB  .text
0000000000000008 IMAGE_REL_AMD64_ADDR32NB  .xdata
000000000000000c IMAGE_REL_AMD64_ADDR32NB  .text
0000000000000010 IMAGE_REL_AMD64_ADDR32NB  .text
0000000000000014 IMAGE_REL_AMD64_ADDR32NB  .xdata



PS D:\Ad Astra\Practice Code\Embedded\C Programming> objdump -t main.o
## The symbol table (the assembler's internal scratchpad)

main.o:     file format pe-x86-64

SYMBOL TABLE:
[  0](sec -2)(fl 0x00)(ty    0)(scl 103) (nx 1) 0x0000000000000000 main.c
File 
[  2](sec  1)(fl 0x00)(ty   20)(scl   2) (nx 1) 0x0000000000000000 square
AUX tagndx 0 ttlsiz 0x0 lnnos 0 next 4
[  4](sec  1)(fl 0x00)(ty   20)(scl   2) (nx 1) 0x000000000000000f main
AUX tagndx 0 ttlsiz 0x0 lnnos 0 next 0
[  6](sec  1)(fl 0x00)(ty    0)(scl   3) (nx 1) 0x0000000000000000 .text
AUX scnlen 0x59 nreloc 5 nlnno 0
[  8](sec  2)(fl 0x00)(ty    0)(scl   3) (nx 1) 0x0000000000000000 .data
AUX scnlen 0x0 nreloc 0 nlnno 0
[ 10](sec  3)(fl 0x00)(ty    0)(scl   3) (nx 1) 0x0000000000000000 .bss
AUX scnlen 0x61a80 nreloc 0 nlnno 0
[ 12](sec  4)(fl 0x00)(ty    0)(scl   3) (nx 1) 0x0000000000000000 .rdata
AUX scnlen 0x30 nreloc 0 nlnno 0
[ 14](sec  5)(fl 0x00)(ty    0)(scl   3) (nx 1) 0x0000000000000000 .xdata
AUX scnlen 0x14 nreloc 0 nlnno 0
[ 16](sec  6)(fl 0x00)(ty    0)(scl   3) (nx 1) 0x0000000000000000 .pdata
AUX scnlen 0x18 nreloc 6 nlnno 0
[ 18](sec  7)(fl 0x00)(ty    0)(scl   3) (nx 1) 0x0000000000000000 .rdata$zzz
AUX scnlen 0x2b nreloc 0 nlnno 0
[ 20](sec  4)(fl 0x00)(ty    0)(scl   2) (nx 0) 0x0000000000000000 a
[ 21](sec  3)(fl 0x00)(ty    0)(scl   2) (nx 0) 0x0000000000000000 massive_array
[ 22](sec  0)(fl 0x00)(ty   20)(scl   2) (nx 0) 0x0000000000000000 __main
[ 23](sec  0)(fl 0x00)(ty   20)(scl   2) (nx 0) 0x0000000000000000 printf
[ 24](sec  0)(fl 0x00)(ty   20)(scl   2) (nx 0) 0x0000000000000000 puts
