# Reference Manuals — Index

STM32F446RE reference documents, all under `~/books/Hardware-and-Systems/STM32/`.

Use the `pdf-mcp` tools (`pdf_search`, `pdf_get_toc`, `pdf_read_pages`) to query them.

| Doc | File | Pages | Index |
|-----|------|-------|-------|
| RM0390 — Reference manual | `rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf` | 1321 | [docs/rm0390.md](rm0390.md) |
| Datasheet — STM32F446xC/E | `stm32f446mc.pdf` | 197 | [docs/datasheet.md](datasheet.md) |
| UM1724 — Nucleo-64 board user manual | `um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf` | 91 | [docs/um1724.md](um1724.md) |

Workflow: pick a doc, open its index, then `pdf_search`/`pdf_read_pages` on demand.
Never dump a full TOC into context — query on demand.

## Query guidance by doc type

- **Reference manual** — chapter index; jump to a chapter, then search within its page range.
- **Datasheet** — table-driven (Tables 1–118); jump to a section/table, values come back as ordered text lines (native tables are not returned as structured objects).
- **Board manual** — curated board facts; pinouts for other Nucleo boards are queryable on demand.
