# Reference Manuals — Index

Reference documents live outside the repo under `~/Books/Hardware-and-Systems/`:

- `STM32/` — the MCU and the board (ST documents, plus a copy of the SVD)
- `Components/` — off-board parts wired to the board (sensors, readers, displays)
- `ARM/` — general Cortex-M teaching material, not authoritative

## STM32F446RE and the Nucleo board (`STM32/`)

Use the `pdf-mcp` tools (`pdf_search`, `pdf_get_toc`, `pdf_read_pages`) to query them.

| Doc | File | Pages | Index |
|-----|------|-------|-------|
| RM0390 — Reference manual | `rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf` | 1321 | [docs/rm0390.md](rm0390.md) |
| Datasheet — STM32F446xC/E | `stm32f446mc.pdf` | 197 | [docs/datasheet.md](datasheet.md) |
| UM1724 — Nucleo-64 board user manual | `um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf` | 91 | [docs/um1724.md](um1724.md) |
| PM0214 — Cortex-M4 programming manual | `pm0214-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf` | 262 | [docs/pm0214.md](pm0214.md) |

## Components (`Components/`)

| Doc | File | Pages | Index |
|-----|------|-------|-------|
| MFRC522 — RFID reader IC (RC522 module) | `MFRC522.pdf` | 95 | [docs/components/mfrc522.md](components/mfrc522.md) |
| MF1S50 — MIFARE Classic EV1 1K tag (the cards and fobs) | `MF1S50.pdf` | 36 | [docs/components/mf1s50.md](components/mf1s50.md) |
| MPU-6500 — 6-axis motion sensor, product specification (pins, SPI timing) | `MPU6500.pdf` | 41 | [docs/components/mpu6500.md](components/mpu6500.md) |
| MPU-6500 — Register Map and Descriptions | `MPU6500-registers.pdf` | 47 | [docs/components/mpu6500.md](components/mpu6500.md) |
| SN74HC595 — 8-bit shift register (SPI-like, write-only) | `sn74hc595.pdf` | 41 | [docs/components/sn74hc595.md](components/sn74hc595.md) |

A component's registers come from its own datasheet, never from the STM32 SVD
or RM0390. The STM32 side of the link (SPI, I2C, UART setup) still comes from RM0390.

Parts the owner has on hand, with likely part numbers (guesses until checked): [docs/inventory.md](inventory.md).

Workflow: pick a doc, open its index, then `pdf_search`/`pdf_read_pages` on demand.
Never dump a full TOC into context — query on demand.

## Query guidance by doc type

- **Reference manual** — chapter index; jump to a chapter, then search within its page range.
- **Datasheet** — table-driven (Tables 1–118); jump to a section/table, values come back as ordered text lines (native tables are not returned as structured objects).
- **Programming manual (PM0214)** — ARM core only: exceptions/vector table, instruction set, and the
  core peripherals (NVIC, SCB, SysTick, MPU, FPU). Use it for anything at `0xE000_E000`+ and for
  assembly/startup questions; use RM0390 for ST peripherals.
- **Board manual** — curated board facts; pinouts for other Nucleo boards are queryable on demand.
- **Component datasheet** — register map, bus framing (e.g. the SPI address byte), command set,
  timing limits. Check the bus mode and maximum clock here before configuring the STM32 side.
