# STM32F446RE Cortex-M4 build.
#
# programs/  -> firmware you flash to the board (needs startup.s + linker.ld)
#               programs/<name>.c         single-file program
#               programs/<name>/main.c    multi-file program: every .c in that
#                                         folder plus every .c in lib/
# lib/       -> shared modules for the multi-file programs (unused code is
#               dropped by --gc-sections)
# examples/  -> C sources compiled only to assembly for study (never flashed)

BUILD := build
PROGRAM ?= blink
NAME ?= regs

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
OBJDUMP := arm-none-eabi-objdump

CPUFLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
CMSIS_DIR := cmsis
CFLAGS := $(CPUFLAGS) -ffreestanding -fno-builtin -ffunction-sections -fdata-sections \
           -Wall -Wextra -g -O0 -DSTM32F446xx -I$(CMSIS_DIR) -Ilib
ASMFLAGS := $(CPUFLAGS) -ffreestanding -fno-builtin -Wall -Wextra
OPTS := 0 1 2
LDFLAGS := $(CPUFLAGS) -T linker.ld -nostdlib -Wl,--gc-sections

# Discovered from the source trees, so adding a program needs no Makefile edit.
PROGRAMS := $(notdir $(basename $(wildcard programs/*.c))) \
            $(patsubst programs/%/main.c,%,$(wildcard programs/*/main.c))
EXAMPLES := $(notdir $(basename $(wildcard examples/*.c)))

.PHONY: all programs flash monitor clean examples example program $(PROGRAMS)

all: programs

programs: $(PROGRAMS)

# Build one board program: make program P=blink
program:
	@mkdir -p $(BUILD)/$(P)
	@if [ -f programs/$(P)/main.c ]; then srcs="$(wildcard programs/$(P)/*.c) $(wildcard lib/*.c)"; \
	else srcs="programs/$(P).c"; fi; \
	objs=""; \
	for f in $$srcs; do \
		o=$(BUILD)/$(P)/$$(basename $$f .c).o; objs="$$objs $$o"; \
		printf '  CC      %s\n' "$$f"; \
		$(CC) $(CFLAGS) -c $$f -o $$o || exit 1; \
	done; \
	$(CC) $(CPUFLAGS) -c startup.s -o $(BUILD)/$(P)/startup.o && \
	$(CC) $(LDFLAGS) -Wl,-Map=$(BUILD)/$(P)/$(P).map \
		$$objs $(BUILD)/$(P)/startup.o \
		-o $(BUILD)/$(P)/$(P).elf
	@printf '  BIN     %s\n' "$(P).bin"
	@$(OBJCOPY) -O binary $(BUILD)/$(P)/$(P).elf $(BUILD)/$(P)/$(P).bin
	@printf '  LST     %s\n' "$(P).lst"
	@$(OBJDUMP) -d -S $(BUILD)/$(P)/$(P).elf > $(BUILD)/$(P)/$(P).lst

# Convenience target per program: make blink / make button / make systick
$(PROGRAMS): %: ; @$(MAKE) --no-print-directory program P=$*

# Flash a program to the board (defaults to blink)
flash:
	@$(MAKE) --no-print-directory program P=$(PROGRAM)
	@printf '  FLASH   %s\n' "$(PROGRAM)"
	@probe-rs download --chip STM32F446RE --protocol swd --connect-under-reset \
		--verify --reset --disable-progressbars \
		$(BUILD)/$(PROGRAM)/$(PROGRAM).elf

# Open the board's UART console (ST-LINK virtual COM port) at 115200 baud.
# Auto-detects /dev/tty.usbmodem*; override with: make monitor MONITOR_PORT=/dev/...
# Quit tio with Ctrl-t then q.
MONITOR_PORT ?= $(firstword $(wildcard /dev/tty.usbmodem*))
monitor:
	@test -n "$(MONITOR_PORT)" || { echo "no /dev/tty.usbmodem* found: is the Nucleo plugged in?"; exit 1; }
	@tio -b 115200 $(MONITOR_PORT); rc=$$?; stty sane 2>/dev/null; exit $$rc

# Generate assembly for one example at every -O level: make example NAME=regs
example: example-$(NAME)

example-%:
	@mkdir -p $(BUILD)/examples/$*
	@for opt in $(OPTS); do \
		$(CC) $(ASMFLAGS) -O$$opt -S examples/$*.c \
			-o $(BUILD)/examples/$*/$*-O$$opt.s; \
	done

examples: $(addprefix example-,$(EXAMPLES))

clean:
	rm -rf $(BUILD)
