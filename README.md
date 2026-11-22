# STM32 Cortex-M4

Target: STM32F446RE on the NUCLEO-F446RE board (Cortex-M4, Thumb, ELF, bare metal).

Authoritative register reference: `svd/stm32f446re.svd` at the repo root.
Board and chip documentation: `docs/` (start at `docs/contents.md`) and the PDFs
under `~/Books/Hardware-and-Systems/`: `STM32/` for the MCU and board,
`Components/` for off-board parts such as the MFRC522 RFID reader and the MF1S50 tag
(their indexes are in `docs/components/`).

## Layout

```text
startup.s      reset handler + vector table, shared by every program
linker.ld      Flash/RAM memory layout, shared by every program
cmsis/         CMSIS core + STM32F446xx device headers
programs/      firmware you flash to the board: <name>.c, or <name>/ with a main.c
lib/core/      peripheral drivers and helpers (uart, spi, clock, delay, ring, fmt, cli, pwm)
lib/devices/   drivers for off-board parts (rc522, mpu6500), built on lib/core
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

1. Create `programs/<name>.c` (needs a `int main(void)`), or a folder
   `programs/<name>/` containing a `main.c` and any other `.c` files.
2. That's it: the Makefile discovers both forms, so `make <name>` works
   immediately. A folder program is built from every `.c` in its folder plus
   every `.c` in `lib/core/` and `lib/devices/` (unused code is dropped by
   `--gc-sections`). Name programs with a topic prefix (`tim_`, `uart_`, `rc522_`,
   `spi_`) so related ones sort together.

## Adding a new example

1. Create `examples/<name>.c`.
2. That's it: the Makefile discovers `examples/*.c`, so `make example NAME=<name>`
   works immediately.