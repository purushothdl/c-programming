PS D:\Ad Astra\Practice Code\Embedded\C Programming> objdump -d main.exe | Select-String -Pattern "<square>:|<main>:|<printf>:|<puts>:" -Context 0,15

> 0000000140001450 <square>:
     140001450: 55                      push   %rbp
     140001451: 48 89 e5                mov    %rsp,%rbp
     140001454: 89 4d 10                mov    %ecx,0x10(%rbp)
     140001457: 8b 45 10                mov    0x10(%rbp),%eax
     14000145a: 0f af c0                imul   %eax,%eax
     14000145d: 5d                      pop    %rbp
     14000145e: c3                      ret
  
> 000000014000145f <main>:
     14000145f: 55                      push   %rbp
     140001460: 48 89 e5                mov    %rsp,%rbp
     140001463: 48 83 ec 20             sub    $0x20,%rsp
     140001467: e8 14 01 00 00          call   140001580 <__main>
     14000146c: b9 0a 00 00 00          mov    $0xa,%ecx
     140001471: e8 da ff ff ff          call   140001450 <square>
     140001476: 89 c2                   mov    %eax,%edx
     140001478: 48 8d 05 85 2b 00 00    lea    0x2b85(%rip),%rax        # 140004004 <a+0x4>
     14000147f: 41 89 d0                mov    %edx,%r8d
     140001482: ba 0a 00 00 00          mov    $0xa,%edx
     140001487: 48 89 c1                mov    %rax,%rcx
     14000148a: e8 71 11 00 00          call   140002600 <printf>
     14000148f: 48 8d 05 86 2b 00 00    lea    0x2b86(%rip),%rax        # 14000401c <a+0x1c>
     140001496: 48 89 c1                mov    %rax,%rcx
     140001499: e8 f2 12 00 00          call   140002790 <puts>
> 0000000140002600 <printf>:
     140002600: 57                      push   %rdi
     140002601: 56                      push   %rsi
     140002602: 53                      push   %rbx
     140002603: 48 83 ec 40             sub    $0x40,%rsp
     140002607: 48 89 cb                mov    %rcx,%rbx
     14000260a: 48 8d 74 24 68          lea    0x68(%rsp),%rsi
     14000260f: b9 01 00 00 00          mov    $0x1,%ecx
     140002614: 48 89 54 24 68          mov    %rdx,0x68(%rsp)
     140002619: 4c 89 44 24 70          mov    %r8,0x70(%rsp)
     14000261e: 4c 89 4c 24 78          mov    %r9,0x78(%rsp)
     140002623: 48 89 74 24 38          mov    %rsi,0x38(%rsp)
     140002628: e8 43 01 00 00          call   140002770 <__acrt_iob_func>
     14000262d: 48 89 c7                mov    %rax,%rdi
     140002630: e8 7b 00 00 00          call   1400026b0 <__local_stdio_printf_options>
     140002635: 45 31 c9                xor    %r9d,%r9d
> 0000000140002790 <puts>:
     140002790: ff 25 1a 6c 06 00       jmp    *0x66c1a(%rip)        # 1400693b0 <__imp_puts>
     140002796: 90                      nop
     140002797: 90                      nop
     140002798: 0f 1f 84 00 00 00 00    nopl   0x0(%rax,%rax,1)
     14000279f: 00 
  
  00000001400027a0 <__p___argc>:
     1400027a0: ff 25 72 6b 06 00       jmp    *0x66b72(%rip)        # 140069318 <__imp___p___argc>
     1400027a6: 90                      nop
     1400027a7: 90                      nop
  
  00000001400027a8 <__p___argv>:
     1400027a8: ff 25 72 6b 06 00       jmp    *0x66b72(%rip)        # 140069320 <__imp___p___argv>
     1400027ae: 90                      nop
     1400027af: 90                      nop


PS D:\Ad Astra\Practice Code\Embedded\C Programming> 