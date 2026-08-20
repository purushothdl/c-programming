# AGENTS.md

## Board / target

- **MCU**: STM32F446RE — ARM Cortex-M4F @ up to 180 MHz, 512 KB Flash, 128 KB SRAM
- **SVD reference**: `svd/stm32f446re.svd`
  - This is the **only** authoritative register definition for this board.
  - Ignore any other `.svd` files on disk (e.g. samples bundled inside an MCP server's `node_modules/`) — never use them for this project.
  - When querying the SVD, always pass `svd_file: "svd/stm32f446re.svd"` and verify the returned device name is `STM32F446`.
- **Toolchain / build**: (to be added)

## Reference manuals (PDF)

Books live outside the repo under `~/books/Hardware-and-Systems/STM32/`:

- `rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf` — RM0390, the F446RE **reference manual** (1321 pages)
- `stm32f446mc.pdf` — the F446RE **datasheet** (197 pages)
- `um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf` — UM1724, Nucleo-64 board user manual (91 pages)

Use the `pdf-mcp` tools (`pdf_search`, `pdf_get_toc`, `pdf_read_pages`) to query
them. Indexes live under `docs/` — start at `docs/contents.md` to pick a
document, then open its per-doc index (`rm0390.md`, `datasheet.md`, `um1724.md`)
to find the chapter/section/table. Do not dump full TOCs into context; query on demand.

## Conventions

- SVD files live under `svd/`, named `<chip>.svd`.
- When starting work on a register/peripheral, look it up from the SVD first; do not guess addresses or bit positions.
- For prose/sequences beyond the SVD (clock tree, AF mapping, programming steps), consult the reference manual via `pdf-mcp` — see `docs/contents.md` for the doc index.
