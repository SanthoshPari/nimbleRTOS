/*
 * Linker script for STM32F407VG (1 MB Flash / 128 KB SRAM).
 *
 * Memory figures are the device's actual datasheet numbers, not
 * placeholders - this is meant to link and run on real hardware, not
 * just simulate.
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Top of RAM - used as the initial main stack pointer (MSP) in the
 * vector table. Exception/ISR stack only; task stacks are entirely
 * separate, statically-allocated arrays (see kernel/include/nimble_task.h) -
 * this is deliberately NOT shared with any task's stack. */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* Reserve space for the main (MSP) stack, used only while in an
 * exception/interrupt handler once tasks are running (task code itself
 * runs on PSP - see docs/CONTEXT_SWITCH.md). Sized generously since
 * ISR nesting depth is small in this design (PendSV/SysTick/one or two
 * peripheral IRQs at a time). */
_Min_MSP_Stack_Size = 0x800; /* 2 KB */
_Min_Heap_Size = 0; /* No heap: this kernel and this project never call
                        malloc(). Left at 0, not omitted, so a stray
                        malloc()-using library dependency fails to link
                        loudly instead of silently succeeding with an
                        undersized heap. */

ENTRY(Reset_Handler)

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    /* ARM exception unwinding tables - required by libgcc even though
     * this project doesn't use C++ exceptions, because libgcc's
     * division/float-emulation helpers reference this section. */
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT> FLASH

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >RAM

    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_MSP_Stack_Size;
        . = ALIGN(8);
    } >RAM

    /DISCARD/ :
    {
        libc.a(*)
        libm.a(*)
        libgcc.a(*)
    }
}
