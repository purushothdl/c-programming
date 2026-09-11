# STM32F446RE Cortex-M4 build.
#
# programs/  -> firmware you flash to the board (needs startup.s + linker.ld)
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
           -Wall -Wextra -g -O0 -DSTM32F446xx -I$(CMSIS_DIR)
ASMFLAGS := $(CPUFLAGS) -ffreestanding -fno-builtin -Wall -Wextra
OPTS := 0 1 2
LDFLAGS := $(CPUFLAGS) -T linker.ld -nostdlib -Wl,--gc-sections

PROGRAMS := blink button systick
EXAMPLES := regs

.PHONY: all programs flash clean examples example program

all: programs

programs: $(PROGRAMS)

# Build one board program: make program P=blink
program:
	@mkdir -p $(BUILD)/$(P)
	$(CC) $(CFLAGS) -c programs/$(P)/main.c -o $(BUILD)/$(P)/main.o
	$(CC) $(CPUFLAGS) -c startup.s -o $(BUILD)/$(P)/startup.o
	$(CC) $(LDFLAGS) -Wl,-Map=$(BUILD)/$(P)/$(P).map \
		$(BUILD)/$(P)/main.o $(BUILD)/$(P)/startup.o \
		-o $(BUILD)/$(P)/$(P).elf
	$(OBJCOPY) -O binary $(BUILD)/$(P)/$(P).elf $(BUILD)/$(P)/$(P).bin
	$(OBJDUMP) -d -S $(BUILD)/$(P)/$(P).elf > $(BUILD)/$(P)/$(P).lst

# Convenience targets, one per program in PROGRAMS
blink: ; @$(MAKE) --no-print-directory program P=blink
button: ; @$(MAKE) --no-print-directory program P=button
systick: ; @$(MAKE) --no-print-directory program P=systick

# Flash a program to the board (defaults to blink)
flash:
	@$(MAKE) --no-print-directory program P=$(PROGRAM)
	probe-rs download --chip STM32F446RE --protocol swd --verify --reset \
		$(BUILD)/$(PROGRAM)/$(PROGRAM).elf

# Generate assembly for one example at every -O level: make example NAME=regs
example: example-$(NAME)

example-%:
	@mkdir -p $(BUILD)/examples/$*
	@for opt in $(OPTS); do \
		$(CC) $(ASMFLAGS) -O$$opt -S examples/$*/$*.c \
			-o $(BUILD)/examples/$*/$*-O$$opt.s; \
	done

examples: $(addprefix example-,$(EXAMPLES))

clean:
	rm -rf $(BUILD)
