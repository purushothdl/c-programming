# MFRC522 — RFID Reader IC Datasheet — Key Sections

Document: `~/Books/Hardware-and-Systems/Components/MFRC522.pdf`
(NXP, "MFRC522 Standard performance MIFARE and NTAG frontend", 95 pages)

The chip on the RC522 RFID module. It is an off-board component: use this
datasheet for its registers, SPI framing and commands, and RM0390 for the
STM32 side of the SPI link. Its registers are not in the SVD.

| Topic | Section | Page |
|-------|---------|------|
| Version 1.0 vs 2.0 differences | 1.1 | 1 |
| Quick reference data (supply, current) | 4 | 2 |
| Pin description | 7.1 | 6 |
| Interface detection (SPI / UART / I2C selected by pins) | 8.1.1 | 9 |
| **SPI: read data, write data, address byte** | 8.1.2–8.1.2.3 | 10–11 |
| CRC coprocessor | 8.2.5 | 29 |
| FIFO buffer (access, control, status) | 8.3 | 29 |
| Interrupt request system (IRQ pin) | 8.4 | 30 |
| Timer unit | 8.5 | 31 |
| Power reduction modes | 8.6 | 33 |
| Reset and oscillator start-up time | 8.8 | 34 |
| Register bit behavior | 9.1 | 35 |
| **Register overview (address map)** | 9.2 | 36 |
| Page 0: command and status (CommandReg, ComIrqReg, ErrorReg, FIFODataReg, FIFOLevelReg, BitFramingReg, CollReg) | 9.3.1 | 38–47 |
| Page 1: communication (ModeReg, TxControlReg, TxASKReg, ...) | 9.3.2 | 47–56 |
| Page 2: configuration (CRCResultReg, RFCfgReg, TModeReg/TPrescalerReg, TReloadReg) | 9.3.3 | 57–62 |
| Page 3: test (AutoTestReg, **VersionReg**) | 9.3.4 | 63–68 |
| Command set (Idle, Mem, CalcCRC, Transmit, Receive, Transceive, MFAuthent, SoftReset) | 10 | 70–73 |
| Limiting values, operating conditions | 11–12 | 74 |
| **SPI timing characteristics** | 14.1 | 78 |
| Application information (antenna circuit) | 15 | 81 |
| **Self test** (expected bytes) | 16.1.1 | 82 |

Card-side protocol (ISO/IEC 14443A request, anticollision, MIFARE
authentication) is not in this datasheet: it only lists the reader commands.
