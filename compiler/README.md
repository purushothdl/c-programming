# Compiler Examples

Each directory is a target platform, not just an instruction-set label. The
calling convention, object format, system libraries, and available tools can
all differ even when two targets use ARM instructions.

## Targets

- `apple-silicon/` - macOS on Apple Silicon (`arm64`, AArch64, Mach-O,
  hosted C with libc).
- `stm32-cortex-m4/` - STM32F446RE (`Cortex-M4`, ARM Thumb, ELF, bare metal).
- `x86_64-windows/` - existing Windows/MSYS2 x86-64 example (PE/COFF).

Generated files belong in each target's `build/` directory and are ignored by
Git. Source files and target-specific instructions stay outside `build/`.
