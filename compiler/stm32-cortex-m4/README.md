# STM32 Cortex-M4

This directory is reserved for the STM32F446RE bare-metal target. It is a
different ARM target from Apple Silicon: the MCU uses the 32-bit Cortex-M4
Thumb instruction set, ELF objects, a startup file, and a linker script.

The SVD and board documentation for this target are at the repository root:

- `svd/stm32f446re.svd`
- `docs/`

Cross-compiled objects belong in `build/`. A hosted `printf` example cannot be
linked for the MCU without a runtime and an `_write` retarget, so STM32 source
and build steps will be added separately from the macOS example.
