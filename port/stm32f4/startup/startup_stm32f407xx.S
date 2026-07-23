/*
 * Startup code for STM32F407VG.
 *
 * Two things live here:
 *   1. The vector table - placed by the linker script at 0x08000000,
 *      the address the Cortex-M core reads on power-up/reset to find
 *      the initial MSP value and the reset entry point.
 *   2. Reset_Handler - copies .data from Flash to RAM, zeroes .bss,
 *      then jumps into main(). Nothing RTOS-specific happens here;
 *      the kernel doesn't start running until main() calls
 *      nimble_scheduler_start().
 *
 * PendSV_Handler, SVC_Handler (context_switch.S) and SysTick_Handler
 * (exception_handlers.c) are pulled in by name below - this file
 * doesn't implement them, it just points the vector table at them.
 */

    .syntax unified
    .cpu cortex-m4
    .fpu softvfp
    .thumb

.global Reset_Handler
.global g_pfnVectors

.extern _sidata
.extern _sdata
.extern _edata
.extern _sbss
.extern _ebss
.extern _estack
.extern main

.extern PendSV_Handler
.extern SVC_Handler
.extern SysTick_Handler

/* ------------------------------------------------------------------ */
    .section .text.Reset_Handler
    .weak Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    /* Copy .data initializers from Flash (_sidata) to RAM (_sdata..._edata). */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    movs r3, #0
    b copy_data_loop_check
copy_data_loop:
    ldr r4, [r2, r3]
    str r4, [r0, r3]
    adds r3, r3, #4
copy_data_loop_check:
    adds r4, r0, r3
    cmp r4, r1
    bcc copy_data_loop

    /* Zero .bss. */
    ldr r2, =_sbss
    ldr r4, =_ebss
    movs r3, #0
    b zero_bss_loop_check
zero_bss_loop:
    str r3, [r2]
    adds r2, r2, #4
zero_bss_loop_check:
    cmp r2, r4
    bcc zero_bss_loop

    bl main

    /* main() should never return (it ends in
     * nimble_scheduler_start(), which doesn't return either) - trap
     * loudly if it somehow does, same philosophy as task_exit_trap()
     * in task_stack_init.c. */
main_returned_trap:
    bkpt #0
    b main_returned_trap
    .size Reset_Handler, . - Reset_Handler

/* ------------------------------------------------------------------ */
/* Default handler for any interrupt without a real implementation.
 * Traps instead of silently returning, so an unexpected/unconfigured
 * interrupt firing is loud and debuggable, not a mystery hang. */
    .section .text.Default_Handler, "ax", %progbits
Default_Handler:
    bkpt #0
Infinite_Loop:
    b Infinite_Loop
    .size Default_Handler, . - Default_Handler

/* ------------------------------------------------------------------ */
/*
 * Vector table.
 *
 * Cortex-M core exceptions (fixed positions for every Cortex-M part),
 * followed by STM32F407-specific peripheral IRQs (fixed positions for
 * this specific device family, per the reference manual). Every entry
 * not explicitly wired to a real handler below is weakly aliased to
 * Default_Handler, so declaring, say, ADC_IRQHandler as a real
 * function elsewhere (app/ or port/ code) transparently overrides it
 * at link time - standard CMSIS-style weak-vector convention.
 */
    .section .isr_vector, "a", %progbits
    .type g_pfnVectors, %object
g_pfnVectors:
    .word _estack                    /* initial MSP */
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0                          /* reserved */
    .word 0                          /* reserved */
    .word 0                          /* reserved */
    .word 0                          /* reserved */
    .word SVC_Handler
    .word DebugMon_Handler
    .word 0                          /* reserved */
    .word PendSV_Handler
    .word SysTick_Handler
    /* STM32F407 peripheral IRQs (positions 0-81, per RM0090). Only the
     * ones the demo app (Section 7) actually uses get a real name here;
     * everything else weakly resolves to Default_Handler. */
    .word WWDG_IRQHandler
    .word PVD_IRQHandler
    .word TAMP_STAMP_IRQHandler
    .word RTC_WKUP_IRQHandler
    .word FLASH_IRQHandler
    .word RCC_IRQHandler
    .word EXTI0_IRQHandler
    .word EXTI1_IRQHandler
    .word EXTI2_IRQHandler
    .word EXTI3_IRQHandler
    .word EXTI4_IRQHandler
    .word DMA1_Stream0_IRQHandler
    .word DMA1_Stream1_IRQHandler
    .word DMA1_Stream2_IRQHandler
    .word DMA1_Stream3_IRQHandler
    .word DMA1_Stream4_IRQHandler
    .word DMA1_Stream5_IRQHandler
    .word DMA1_Stream6_IRQHandler
    .word ADC_IRQHandler
    .word CAN1_TX_IRQHandler
    .word CAN1_RX0_IRQHandler
    .word CAN1_RX1_IRQHandler
    .word CAN1_SCE_IRQHandler
    .word EXTI9_5_IRQHandler
    .word TIM1_BRK_TIM9_IRQHandler
    .word TIM1_UP_TIM10_IRQHandler
    .word TIM1_TRG_COM_TIM11_IRQHandler
    .word TIM1_CC_IRQHandler
    .word TIM2_IRQHandler
    .word TIM3_IRQHandler
    .word TIM4_IRQHandler
    .word I2C1_EV_IRQHandler
    .word I2C1_ER_IRQHandler
    .word I2C2_EV_IRQHandler
    .word I2C2_ER_IRQHandler
    .word SPI1_IRQHandler
    .word SPI2_IRQHandler
    .word USART1_IRQHandler
    .word USART2_IRQHandler
    .word USART3_IRQHandler
    .word EXTI15_10_IRQHandler
    .word RTC_Alarm_IRQHandler
    .word OTG_FS_WKUP_IRQHandler
    .word TIM8_BRK_TIM12_IRQHandler
    .word TIM8_UP_TIM13_IRQHandler
    .word TIM8_TRG_COM_TIM14_IRQHandler
    .word TIM8_CC_IRQHandler
    .word DMA1_Stream7_IRQHandler
    .word FSMC_IRQHandler
    .word SDIO_IRQHandler
    .word TIM5_IRQHandler
    .word SPI3_IRQHandler
    .word UART4_IRQHandler
    .word UART5_IRQHandler
    .word TIM6_DAC_IRQHandler
    .word TIM7_IRQHandler
    .word DMA2_Stream0_IRQHandler
    .word DMA2_Stream1_IRQHandler
    .word DMA2_Stream2_IRQHandler
    .word DMA2_Stream3_IRQHandler
    .word DMA2_Stream4_IRQHandler
    .word ETH_IRQHandler
    .word ETH_WKUP_IRQHandler
    .word CAN2_TX_IRQHandler
    .word CAN2_RX0_IRQHandler
    .word CAN2_RX1_IRQHandler
    .word CAN2_SCE_IRQHandler
    .word OTG_FS_IRQHandler
    .word DMA2_Stream5_IRQHandler
    .word DMA2_Stream6_IRQHandler
    .word DMA2_Stream7_IRQHandler
    .word USART6_IRQHandler
    .word I2C3_EV_IRQHandler
    .word I2C3_ER_IRQHandler
    .word OTG_HS_EP1_OUT_IRQHandler
    .word OTG_HS_EP1_IN_IRQHandler
    .word OTG_HS_WKUP_IRQHandler
    .word OTG_HS_IRQHandler
    .word DCMI_IRQHandler
    .word CRYP_IRQHandler
    .word HASH_RNG_IRQHandler
    .word FPU_IRQHandler
    .size g_pfnVectors, . - g_pfnVectors

/* Weak aliases: every named handler above falls back to
 * Default_Handler unless something else in the final link defines it
 * for real (e.g. app/power_supply/ providing ADC_IRQHandler). */
    .weak NMI_Handler
    .thumb_set NMI_Handler, Default_Handler
    .weak HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler
    .weak MemManage_Handler
    .thumb_set MemManage_Handler, Default_Handler
    .weak BusFault_Handler
    .thumb_set BusFault_Handler, Default_Handler
    .weak UsageFault_Handler
    .thumb_set UsageFault_Handler, Default_Handler
    .weak DebugMon_Handler
    .thumb_set DebugMon_Handler, Default_Handler

    .weak WWDG_IRQHandler
    .thumb_set WWDG_IRQHandler, Default_Handler
    .weak PVD_IRQHandler
    .thumb_set PVD_IRQHandler, Default_Handler
    .weak TAMP_STAMP_IRQHandler
    .thumb_set TAMP_STAMP_IRQHandler, Default_Handler
    .weak RTC_WKUP_IRQHandler
    .thumb_set RTC_WKUP_IRQHandler, Default_Handler
    .weak FLASH_IRQHandler
    .thumb_set FLASH_IRQHandler, Default_Handler
    .weak RCC_IRQHandler
    .thumb_set RCC_IRQHandler, Default_Handler
    .weak EXTI0_IRQHandler
    .thumb_set EXTI0_IRQHandler, Default_Handler
    .weak EXTI1_IRQHandler
    .thumb_set EXTI1_IRQHandler, Default_Handler
    .weak EXTI2_IRQHandler
    .thumb_set EXTI2_IRQHandler, Default_Handler
    .weak EXTI3_IRQHandler
    .thumb_set EXTI3_IRQHandler, Default_Handler
    .weak EXTI4_IRQHandler
    .thumb_set EXTI4_IRQHandler, Default_Handler
    .weak DMA1_Stream0_IRQHandler
    .thumb_set DMA1_Stream0_IRQHandler, Default_Handler
    .weak DMA1_Stream1_IRQHandler
    .thumb_set DMA1_Stream1_IRQHandler, Default_Handler
    .weak DMA1_Stream2_IRQHandler
    .thumb_set DMA1_Stream2_IRQHandler, Default_Handler
    .weak DMA1_Stream3_IRQHandler
    .thumb_set DMA1_Stream3_IRQHandler, Default_Handler
    .weak DMA1_Stream4_IRQHandler
    .thumb_set DMA1_Stream4_IRQHandler, Default_Handler
    .weak DMA1_Stream5_IRQHandler
    .thumb_set DMA1_Stream5_IRQHandler, Default_Handler
    .weak DMA1_Stream6_IRQHandler
    .thumb_set DMA1_Stream6_IRQHandler, Default_Handler
    .weak ADC_IRQHandler
    .thumb_set ADC_IRQHandler, Default_Handler
    .weak CAN1_TX_IRQHandler
    .thumb_set CAN1_TX_IRQHandler, Default_Handler
    .weak CAN1_RX0_IRQHandler
    .thumb_set CAN1_RX0_IRQHandler, Default_Handler
    .weak CAN1_RX1_IRQHandler
    .thumb_set CAN1_RX1_IRQHandler, Default_Handler
    .weak CAN1_SCE_IRQHandler
    .thumb_set CAN1_SCE_IRQHandler, Default_Handler
    .weak EXTI9_5_IRQHandler
    .thumb_set EXTI9_5_IRQHandler, Default_Handler
    .weak TIM1_BRK_TIM9_IRQHandler
    .thumb_set TIM1_BRK_TIM9_IRQHandler, Default_Handler
    .weak TIM1_UP_TIM10_IRQHandler
    .thumb_set TIM1_UP_TIM10_IRQHandler, Default_Handler
    .weak TIM1_TRG_COM_TIM11_IRQHandler
    .thumb_set TIM1_TRG_COM_TIM11_IRQHandler, Default_Handler
    .weak TIM1_CC_IRQHandler
    .thumb_set TIM1_CC_IRQHandler, Default_Handler
    .weak TIM2_IRQHandler
    .thumb_set TIM2_IRQHandler, Default_Handler
    .weak TIM3_IRQHandler
    .thumb_set TIM3_IRQHandler, Default_Handler
    .weak TIM4_IRQHandler
    .thumb_set TIM4_IRQHandler, Default_Handler
    .weak I2C1_EV_IRQHandler
    .thumb_set I2C1_EV_IRQHandler, Default_Handler
    .weak I2C1_ER_IRQHandler
    .thumb_set I2C1_ER_IRQHandler, Default_Handler
    .weak I2C2_EV_IRQHandler
    .thumb_set I2C2_EV_IRQHandler, Default_Handler
    .weak I2C2_ER_IRQHandler
    .thumb_set I2C2_ER_IRQHandler, Default_Handler
    .weak SPI1_IRQHandler
    .thumb_set SPI1_IRQHandler, Default_Handler
    .weak SPI2_IRQHandler
    .thumb_set SPI2_IRQHandler, Default_Handler
    .weak USART1_IRQHandler
    .thumb_set USART1_IRQHandler, Default_Handler
    .weak USART2_IRQHandler
    .thumb_set USART2_IRQHandler, Default_Handler
    .weak USART3_IRQHandler
    .thumb_set USART3_IRQHandler, Default_Handler
    .weak EXTI15_10_IRQHandler
    .thumb_set EXTI15_10_IRQHandler, Default_Handler
    .weak RTC_Alarm_IRQHandler
    .thumb_set RTC_Alarm_IRQHandler, Default_Handler
    .weak OTG_FS_WKUP_IRQHandler
    .thumb_set OTG_FS_WKUP_IRQHandler, Default_Handler
    .weak TIM8_BRK_TIM12_IRQHandler
    .thumb_set TIM8_BRK_TIM12_IRQHandler, Default_Handler
    .weak TIM8_UP_TIM13_IRQHandler
    .thumb_set TIM8_UP_TIM13_IRQHandler, Default_Handler
    .weak TIM8_TRG_COM_TIM14_IRQHandler
    .thumb_set TIM8_TRG_COM_TIM14_IRQHandler, Default_Handler
    .weak TIM8_CC_IRQHandler
    .thumb_set TIM8_CC_IRQHandler, Default_Handler
    .weak DMA1_Stream7_IRQHandler
    .thumb_set DMA1_Stream7_IRQHandler, Default_Handler
    .weak FSMC_IRQHandler
    .thumb_set FSMC_IRQHandler, Default_Handler
    .weak SDIO_IRQHandler
    .thumb_set SDIO_IRQHandler, Default_Handler
    .weak TIM5_IRQHandler
    .thumb_set TIM5_IRQHandler, Default_Handler
    .weak SPI3_IRQHandler
    .thumb_set SPI3_IRQHandler, Default_Handler
    .weak UART4_IRQHandler
    .thumb_set UART4_IRQHandler, Default_Handler
    .weak UART5_IRQHandler
    .thumb_set UART5_IRQHandler, Default_Handler
    .weak TIM6_DAC_IRQHandler
    .thumb_set TIM6_DAC_IRQHandler, Default_Handler
    .weak TIM7_IRQHandler
    .thumb_set TIM7_IRQHandler, Default_Handler
    .weak DMA2_Stream0_IRQHandler
    .thumb_set DMA2_Stream0_IRQHandler, Default_Handler
    .weak DMA2_Stream1_IRQHandler
    .thumb_set DMA2_Stream1_IRQHandler, Default_Handler
    .weak DMA2_Stream2_IRQHandler
    .thumb_set DMA2_Stream2_IRQHandler, Default_Handler
    .weak DMA2_Stream3_IRQHandler
    .thumb_set DMA2_Stream3_IRQHandler, Default_Handler
    .weak DMA2_Stream4_IRQHandler
    .thumb_set DMA2_Stream4_IRQHandler, Default_Handler
    .weak ETH_IRQHandler
    .thumb_set ETH_IRQHandler, Default_Handler
    .weak ETH_WKUP_IRQHandler
    .thumb_set ETH_WKUP_IRQHandler, Default_Handler
    .weak CAN2_TX_IRQHandler
    .thumb_set CAN2_TX_IRQHandler, Default_Handler
    .weak CAN2_RX0_IRQHandler
    .thumb_set CAN2_RX0_IRQHandler, Default_Handler
    .weak CAN2_RX1_IRQHandler
    .thumb_set CAN2_RX1_IRQHandler, Default_Handler
    .weak CAN2_SCE_IRQHandler
    .thumb_set CAN2_SCE_IRQHandler, Default_Handler
    .weak OTG_FS_IRQHandler
    .thumb_set OTG_FS_IRQHandler, Default_Handler
    .weak DMA2_Stream5_IRQHandler
    .thumb_set DMA2_Stream5_IRQHandler, Default_Handler
    .weak DMA2_Stream6_IRQHandler
    .thumb_set DMA2_Stream6_IRQHandler, Default_Handler
    .weak DMA2_Stream7_IRQHandler
    .thumb_set DMA2_Stream7_IRQHandler, Default_Handler
    .weak USART6_IRQHandler
    .thumb_set USART6_IRQHandler, Default_Handler
    .weak I2C3_EV_IRQHandler
    .thumb_set I2C3_EV_IRQHandler, Default_Handler
    .weak I2C3_ER_IRQHandler
    .thumb_set I2C3_ER_IRQHandler, Default_Handler
    .weak OTG_HS_EP1_OUT_IRQHandler
    .thumb_set OTG_HS_EP1_OUT_IRQHandler, Default_Handler
    .weak OTG_HS_EP1_IN_IRQHandler
    .thumb_set OTG_HS_EP1_IN_IRQHandler, Default_Handler
    .weak OTG_HS_WKUP_IRQHandler
    .thumb_set OTG_HS_WKUP_IRQHandler, Default_Handler
    .weak OTG_HS_IRQHandler
    .thumb_set OTG_HS_IRQHandler, Default_Handler
    .weak DCMI_IRQHandler
    .thumb_set DCMI_IRQHandler, Default_Handler
    .weak CRYP_IRQHandler
    .thumb_set CRYP_IRQHandler, Default_Handler
    .weak HASH_RNG_IRQHandler
    .thumb_set HASH_RNG_IRQHandler, Default_Handler
    .weak FPU_IRQHandler
    .thumb_set FPU_IRQHandler, Default_Handler
