# Datasheet — STM32F446xC/E — Key Sections & Tables

Document: `~/books/Hardware-and-Systems/STM32/stm32f446mc.pdf`
(197 pages, Tables 1–118)

Page numbers are **PDF page numbers**, not printed numbers.

## Sections

| Section | Page |
|---------|------|
| 1 Introduction | 12 |
| 2 Description | 13 |
| 3 Functional overview | 18 |
| 4 Pinout and pin description | 40 |
| 5 Memory mapping | 66 |
| 6 Electrical characteristics | 71 |
| 7 Package information | 173 |
| 8 Ordering information | 189 |

## Key tables

| Table | Topic | Page |
|-------|-------|------|
| Table 1 | Device summary | 2 |
| Table 9 | Legend/abbreviations for pinout | 45 |
| Table 10 | Pin and ball descriptions | 45 |
| Table 11 | **Alternate function** (AF mapping) | 58 |
| Table 12 | Register boundary addresses | 67 |
| Table 13 | Voltage characteristics (absolute max) | 73 |
| Table 14 | Current characteristics (absolute max) | 74 |
| Table 15 | Thermal characteristics | 74 |
| Table 16 | **General operating conditions** (VDD/VDDA/VBAT, V12 power scales) | 75 |
| Table 23–35 | Supply current consumption (Run/Sleep/Stop/Standby/VBAT/peripheral) | 82–98 |
| Table 37 | High-speed external clock (HSE) characteristics | 102 |
| Table 39 | HSE 4–26 MHz oscillator characteristics | 104 |
| Table 41 | HSI oscillator characteristics | 106 |
| Table 43 | **Main PLL characteristics** | 107 |
| Table 44 | PLLI2S (audio PLL) characteristics | 108 |
| Table 45 | PLLSAI characteristics | 109 |
| Table 47–50 | Flash memory characteristics | 111–113 |
| Table 56–58 | I/O port characteristics | 117–120 |
| Table 60 | TIM timer characteristics | 123 |
| Table 61–66 | I2C / FMPI2C / SPI / QSPI / I2S characteristics | 124–132 |
| Table 74–79 | ADC characteristics & accuracy | 140–142 |
| Table 80–81 | Temperature sensor | 145 |
| Table 85 | DAC characteristics | 146 |
| Table 86–103 | FMC/SDRAM timings | 151–168 |
| Table 108–116 | Package mechanical data | 174–186 |
| Table 117 | Package thermal characteristics | 188 |

## Notes on querying

- The datasheet is **table-driven**. Values come back as ordered text lines via
  `pdf_search`/`pdf_read_pages`; native table objects are NOT returned
  (PyMuPDF `find_tables` returns 0 on dense ST datasheet tables).
- Diagrams and waveform figures require `pdf_render_pages` (image output) —
  they do not extract as text.
