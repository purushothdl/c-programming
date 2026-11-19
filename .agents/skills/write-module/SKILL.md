---
name: write-module
description: Write a bare-metal STM32F446RE program or driver module in this repo (programs/<name>.c, programs/<name>/, lib/*.c) from a short brief. Use when Purushoth asks to write, build or add a program, peripheral driver or module, or pastes a "what to do next" brief from another chat (steps like clock, pin, configure, enable, plus gotchas). Covers the lookup, verify, write and report loop.
---

# Write a module

Purushoth learns the concept and the key configuration values in another chat, then brings a brief here. **Default mode: write the code.** They learn by reading it and questioning it, so write code that reads clearly. Do not hand back hints or ask them to look up registers, unless they say "review" or "I'll write it".

## The brief is the plan

- Follow its step order (typically: clock, pin, configure, enable). Do not re-plan.
- Treat its **numbers and names as unchecked**. Compute what they give (for example PSC = 15 and ARR = 999 at 16 MHz gives a 1 MHz tick and a 1 kHz period) and say so.
- Check the **repo facts** it states against the repo before using them: where a file lives (`cli.c` is in `lib/`, not in a program folder), which programs exist, table and page numbers (the datasheet AF table is Table 11; the brief may say Table 10), pin and AF choices. The other chat cannot see the repo and goes stale.
- If the brief is wrong or incomplete, say what you changed and why. Do not silently fix it.
- The brief's "check it with you after" step is replaced by the report in step 7.

## Loop

1. **List the registers** the module touches.
2. **Look each up in the SVD** (`svd/stm32f446re.svd`, absolute path; check the device name is `STM32F446`). Use the exact register and field names. Never write an address or bit position from memory.
3. **Read the manual section** for the sequence and the preconditions (RM0390 for ST peripherals, PM0214 for core peripherals; index in `docs/contents.md`, then `pdf-mcp`). Look for: write-only-while-disabled fields, preload and update-event rules, enable ordering, flags that clear on read.
4. **Write the code** in the repo style (below).
5. **Build** with `make <name>`, and also at `-O2` if the module has timing or volatile concerns.
6. **Verify on hardware** when a board is connected: flash, then read the key registers back with the embedded debugger and compare with what the code intended. Host-test pure logic (rings, formatting, parsing) off the board.
7. **Report** against the brief: what matched, what changed and why, what was checked in the manual (give pages or sections), what was **not** verified.

## Repo style (see `programs/uart_cli/` and `lib/`)

- Small `main`: init calls, then the loop. Details live in named functions (`xxx_init`, `xxx_set`).
- Reusable driver goes in `lib/<name>.c` with `lib/<name>.h`. Keep a one-off program in one file until a second program needs the code.
- The header comment states the **contract**: who produces and consumes, what the interrupt does and when it is armed, units, limits.
- Every number is a named constant (`UART_BRR`, `PA5_AF_TIM2_CH1`) with the arithmetic in a short header comment when the value is derived.
- Use the CMSIS macros and field names from the device header, not hex masks.
- **Comment the why, not the what.** Keep: ordering reasons, preload and update-event rules, ownership of shared state, hardware quirks, manual references (`RM0390 17.4.7`). Delete: narration of what a line does when the macro name already says it.

## Do not

- Do not guess a register name or bit; look it up.
- Do not claim the code works on the board unless it was flashed and observed. Say "builds" when it only builds.
- Do not leave a verification gap unmentioned.
