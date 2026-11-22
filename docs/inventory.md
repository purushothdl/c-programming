# Parts inventory (what Purushoth owns)

Listed 2026-11-22 from the storage box. "Likely part" is a guess from the pin names
the owner reported, not read from the chip markings. Confirm the printed markings
before wiring, and read the part's datasheet (save it in `~/Books/Hardware-and-Systems/Components/`).

## Boards and tools

| Item | Notes |
|------|-------|
| NUCLEO-F446RE | main board, 3.3 V logic |
| Arduino Uno | 5 V logic. A known-good board for testing modules (see the risk note below) |
| Logic analyser, 8 channel, cheap clone | probably a 24 MHz Saleae-style clone (guess); works with sigrok/PulseView (guess). Fast enough for SPI at 351 kHz |
| Multimeter | |
| Resistors, capacitors, MOSFETs, BJTs, small box of ICs (gates, decoders, op-amps) | contains a 74HC595N (used, see below); check for 74HC165 |

## Modules

| Item | Pins / interface (as reported) | Likely part (guess) | Use for learning |
|------|-------------------------------|---------------------|------------------|
| Display, not OLED | GND, VCC, SDA, SCL (I2C) | I2C character LCD with a PCF8574 backpack (address 0x27 or 0x3F), or a small I2C display | a known I2C device, for when I2C work starts |
| MH Real-Time Clock x2 | not yet checked | DS1307 / DS3231 (I2C, address 0x68) or DS1302 (3-wire). SDA+SCL pins means I2C | I2C test device, timestamps for logging |
| Bluetooth module | RXD, TXD (UART) | HC-05 or HC-06 | second USART, UART with DMA, wireless CLI |
| Wi-Fi module | not yet checked | ESP-01 (ESP8266) or similar, UART | later: telemetry. Needs a clean 3.3 V supply |
| 8x8 LED matrix | not yet checked | bare matrix or a MAX7219 module (SPI: DIN, CS, CLK) | if MAX7219: a visible SPI success |
| 4-digit display | not yet checked | TM1637 module (2-wire, not I2C) or bare 7-segment | timer interrupt multiplexing |
| Stepper motor + ULN2003 board | 4 control inputs | 28BYJ-48 (5 V) | timer-driven step sequence |
| Servo motor | PWM, 50 Hz | hobby servo | TIM PWM at 50 Hz; later a chute-release actuator |
| Relay module | 1 input | | GPIO switching (mind the supply) |
| Flame sensor, water sensor, KY-038 sound sensor | analogue and digital outputs | | ADC practice, ADC + DMA |
| Joystick module | 2 analogue axes + button | | ADC, two channels |
| 4x4 keypad | 8 matrix lines | | GPIO matrix scan with a timer interrupt |

## Working (verified on the board)

| Item | Result |
|------|--------|
| 74HC595N (TI SN74HC595) | drives a one-digit common-cathode 7-segment display over SPI (`programs/sr595`); QH' read-back matches |
| One-digit 7-segment display, common cathode | segments a to g and dp work through 330 ohm resistors |

## Known faulty

| Item | Result |
|------|--------|
| MPU6500 module (Robu, 2026-11) | SPI `WHO_AM_I` reads `0x00`; I2C scan finds nothing. Likely dead. Driver `lib/devices/mpu6500.c` and `programs/imu_whoami` are ready for a replacement |
| RC522 module | works only while pressed into the breadboard; contact or power fault, not fully explained |

## Safety notes

- The Nucleo F446RE is 3.3 V. Never feed a 5 V signal to a pin without checking it is 5 V tolerant (datasheet, "FT" pins).
- The Arduino Uno drives 5 V on its I/O. Testing a 3.3 V module on it is outside the module's rating. It is acceptable for a part already believed dead, not for a new one.
