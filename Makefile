#
# Builds nimbleRTOS + the power-supply demo app for STM32F407VG.
#
# Requires: arm-none-eabi-gcc toolchain (see docs/BUILDING.md for setup
# and for QEMU instructions to run this without real hardware).
#
TARGET      = nimbleRTOS
BUILD_DIR   = build

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

CPU_FLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

INCLUDES = \
    -Ikernel/include \
    -Iport/stm32f4/include \
    -Iapp/power_supply

C_SOURCES = \
    $(wildcard kernel/src/*.c) \
    $(wildcard port/arch/cortex-m4/*.c) \
    $(wildcard port/stm32f4/*.c) \
    $(wildcard app/power_supply/*.c)

ASM_SOURCES = \
    port/arch/cortex-m4/context_switch.S \
    port/stm32f4/startup/startup_stm32f407xx.S

LDSCRIPT = port/stm32f4/linker/STM32F407VG_FLASH.ld

CFLAGS  = $(CPU_FLAGS) $(INCLUDES) -Wall -Wextra -Wshadow -std=c11 \
          -ffunction-sections -fdata-sections -O2 -g3
ASFLAGS = $(CPU_FLAGS)
LDFLAGS = $(CPU_FLAGS) -T$(LDSCRIPT) -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(TARGET).map \
          -specs=nano.specs -specs=nosys.specs

OBJECTS  = $(addprefix $(BUILD_DIR)/, $(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/, $(notdir $(ASM_SOURCES:.S=.o)))
VPATH = $(sort $(dir $(C_SOURCES) $(ASM_SOURCES)))

.PHONY: all clean qemu

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin
	$(SIZE) $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

# Runs under QEMU's STM32F405-based netduinoplus2 machine model - no
# real hardware needed. See docs/BUILDING.md for what to expect on the
# console and how this differs from a real board.
qemu: $(BUILD_DIR)/$(TARGET).elf
	qemu-system-arm -M netduinoplus2 -nographic -kernel $(BUILD_DIR)/$(TARGET).elf
