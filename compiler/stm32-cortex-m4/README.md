# STM32 Cortex-M4

Target: STM32F446RE on the NUCLEO-F446RE board (Cortex-M4, Thumb, ELF, bare metal).

Authoritative register reference: `svd/stm32f446re.svd` at the repo root.
Board and chip documentation: `docs/` and the RM0390/UM1724 PDFs.

## Layout

```text
startup.s      reset handler + vector table, shared by every program
linker.ld      Flash/RAM memory layout, shared by every program
programs/      firmware you flash to the board (one folder per program)
examples/      C sources compiled to assembly for study, never flashed
build/         generated output, mirrors programs/ and examples/, gitignored
```

## Build a program and flash it

```sh
make blink            # programs/blink -> build/blink/blink.elf/.bin/.lst/.map
make flash            # build blink and program it via ST-LINK (default: blink)
make flash PROGRAM=button   # build+flash another program
```

Flash command used:

```sh
probe-rs download --chip STM32F446RE --protocol swd --verify --reset \
    build/<program>/<program>.elf
```

## Generate assembly for an example

```sh
make examples                  # every example, at -O0, -O1 and -O2
make example NAME=regs         # one example
```

Each example produces one `.s` file per optimization level:

```text
build/examples/regs/regs-O0.s
build/examples/regs/regs-O1.s
build/examples/regs/regs-O2.s
```

These are compiled with `-S` only. Nothing is linked or flashed.

## Adding a new program

1. Create `programs/<name>/main.c`.
2. Add `<name>` to `PROGRAMS` and one `make` convenience line in the Makefile.

## Adding a new example

1. Create `examples/<name>/<name>.c`.
2. Add `<name>` to `EXAMPLES` and one `example-<name>` convenience line.