# AGENTS.md

## Board / target

- **MCU**: STM32F446RE — ARM Cortex-M4F @ up to 180 MHz, 512 KB Flash, 128 KB SRAM
- **SVD reference**: `svd/stm32f446re.svd`
  - This is the **only** authoritative register definition for this board.
  - Ignore any other `.svd` files on disk (e.g. samples bundled inside an MCP server's `node_modules/`) — never use them for this project.
  - When querying the SVD, always pass an **absolute** path to `svd_file` (e.g. `…/c-programming/svd/stm32f446re.svd`) and verify the returned device name is `STM32F446`.
- **Toolchain / build**: `arm-none-eabi-gcc` + `probe-rs`; driven by the root `Makefile`.
  - `make <name>` builds `programs/<name>.c`, or the folder `programs/<name>/` (needs `main.c`; also compiles `lib/core/*.c` and `lib/devices/*.c`), into `build/<name>/`. Both forms are auto-discovered.
  - `make flash PROGRAM=<name>` builds and flashes over SWD.
  - `make example NAME=<name>` emits assembly for `examples/<name>.c` at -O0/1/2.
  - Compile flags: `-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -DSTM32F446xx -Icmsis`.

## Reference manuals (PDF)

Books live outside the repo under `~/Books/Hardware-and-Systems/`, in three folders:

| Folder | Holds | Authority for |
|---|---|---|
| `STM32/` | ST documents for the MCU and the board | everything on the STM32 side |
| `Components/` | datasheets for off-board parts wired to the board | that part's registers, bus framing and commands |
| `ARM/` | general Cortex-M teaching slides | nothing: background reading only |

New component datasheets go in `Components/`, with an index in `docs/components/<part>.md` and a row in `docs/contents.md`.

`STM32/`:

- `rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf` — RM0390, the F446RE **reference manual** (1321 pages)
- `stm32f446mc.pdf` — the F446RE **datasheet** (197 pages)
- `um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf` — UM1724, Nucleo-64 board user manual (91 pages)
- `pm0214-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf` — PM0214, Cortex-M4 **programming manual** (262 pages): core registers, exception model/vector table, instruction set, and core peripherals (NVIC, SCB, SysTick, MPU, FPU)

`Components/`:

- `MFRC522.pdf` — NXP MFRC522, the RFID reader IC on the RC522 module (95 pages): SPI address byte format, register map, command set, self test
- `MF1S50.pdf` — NXP MF1S50 (MIFARE Classic EV1 1K), the tag inside the cards and fobs (36 pages): REQA/anticollision command codes, ATQA and SAK values, UID sizes, frame timing
- `MPU6500.pdf` — InvenSense MPU-6500 product specification (41 pages): pins, SPI rules and timing (max 1 MHz), start-up time
- `MPU6500-registers.pdf` — MPU-6500 Register Map and Descriptions (47 pages): register addresses and bits (WHO_AM_I, power management, data registers, SPI reset sequence)
- `sn74hc595.pdf` — TI SN74HC595 8-bit shift register (41 pages): DIP pin numbers, function table, timing

Use the `pdf-mcp` tools (`pdf_search`, `pdf_get_toc`, `pdf_read_pages`) to query
them. Indexes live under `docs/` — start at `docs/contents.md` to pick a
document, then open its per-doc index (`rm0390.md`, `datasheet.md`, `um1724.md`, `pm0214.md`, and `components/mfrc522.md`, `components/mf1s50.md`, `components/mpu6500.md`, `components/sn74hc595.md`)
to find the chapter/section/table. Do not dump full TOCs into context; query on demand.

## Conventions

- SVD files live under `svd/`, named `<chip>.svd`.
- When starting work on a register/peripheral, look it up from the SVD first; do not guess addresses or bit positions.
- Core peripherals (NVIC, SCB, SysTick, MPU, FPU) and ARM instruction/exception details come from PM0214, not RM0390; ST peripherals (RCC, GPIO, USART...) come from RM0390.
- Off-board components (e.g. the MFRC522 reader, the MF1S50 tag, the MPU-6500) come from their own datasheet in `Components/`. Their registers are not in the SVD; do not look them up there.
- For prose/sequences beyond the SVD (clock tree, AF mapping, programming steps), consult the reference manual via `pdf-mcp` — see `docs/contents.md` for the doc index.

## Skills

- To write a program or module (or when given a "what to do next" brief), follow `.claude/skills/write-module/SKILL.md`.
- To explain a register or hardware behaviour, follow `.claude/skills/explain-hardware/SKILL.md`.
- To build an interactive lesson page (sequences, register builders, labs) in `visualizations/`, follow `.claude/skills/visualize-concept/SKILL.md`.
