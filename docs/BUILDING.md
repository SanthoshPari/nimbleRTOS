# Building & Running

## Toolchain setup

Install the ARM GNU embedded toolchain:

```bash
# Debian/Ubuntu
sudo apt install gcc-arm-none-eabi

# macOS (Homebrew)
brew install --cask gcc-arm-embedded
```

Verify: `arm-none-eabi-gcc --version`.

## Building

```bash
make
```

Produces `build/nimbleRTOS.elf` and `build/nimbleRTOS.bin`, and prints
a `.text`/`.data`/`.bss` size summary (`arm-none-eabi-size`) — worth
watching over time as a rough proxy for "did that change bloat the
image."

## Running without hardware (QEMU)

```bash
# Debian/Ubuntu
sudo apt install qemu-system-arm

make qemu
```

This runs under QEMU's `netduinoplus2` machine model, which emulates
an STM32F405 (same Cortex-M4 core and peripheral set family as the
STM32F407 this port targets) — close enough to boot, run the
scheduler, and exercise the kernel without owning a physical board.
Peripheral fidelity (exact ADC/UART timing) isn't cycle-accurate in
QEMU, so timing-sensitive validation ultimately still wants real
hardware — noted here rather than glossed over.

Exit QEMU with `Ctrl-A` then `X`.

## Running on real hardware

Any STM32F407-based board (e.g. an STM32F4-Discovery) works. Flash
with `st-flash` (via [stlink-tools](https://github.com/stlink-org/stlink))
or OpenOCD:

```bash
# stlink-tools
st-flash write build/nimbleRTOS.bin 0x8000000

# OpenOCD
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build/nimbleRTOS.elf verify reset exit"
```

## What you should see

With the demo app (Section 7) in place: the power-supply state machine
cycles `IDLE -> SOFT_START -> RUNNING`, `LoggerTask` prints periodic
status over the UART CLI, and forcing a simulated fault condition
drives the system into `FAULT` and demonstrates `ProtectionTask`
preempting everything else via the priority-inheritance mutex path
described in `docs/PRIORITY_INHERITANCE.md`.
