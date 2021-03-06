#include <stdio.h>
#include <stddef.h>

#include "cpu.h"
#include "sched.h"

#include "board.h"

#include "at86rf231.h"
#include "at86rf231_spi.h"
extern volatile unsigned int sched_context_switch_request;

/*

SPI2
  SCLK : PB13
  MISO : PB14
  MOSI : PB15
  CS : PA1

GPIO
  IRQ0 : PC2 : Frame buff empty indicator
  DIG2 : TIM3_CH4 : RX Frame Time stamping TODO : NOT USED, TIM3 is used as general timer.
  Reset : PC1 : active low, enable chip
  SLEEP : PA0 : control sleep, tx & rx state

*/

inline static void RESET_CLR(void) { GPIOC->BRR = 1<<1; }
inline static void RESET_SET(void) { GPIOC->BSRR = 1<<1; }
inline static void CSn_SET(void) { GPIOA->BSRR = 1<<1; }
inline static void CSn_CLR(void) { GPIOA->BRR = 1<<1; }
inline static void SLEEP_CLR(void) { GPIOA->BRR = 1<<0; }

uint8_t at86rf231_get_status(void)
{
    return at86rf231_reg_read(AT86RF231_REG__TRX_STATUS)
           & AT86RF231_TRX_STATUS_MASK__TRX_STATUS;
}

static
void enable_exti_interrupt(void)
{
    EXTI_InitTypeDef   EXTI_InitStructure;

    EXTI_InitStructure.EXTI_Line = EXTI_Line2;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
}
static
void disable_exti_interrupt(void)
{
    EXTI_InitTypeDef   EXTI_InitStructure;

    EXTI_InitStructure.EXTI_Line = EXTI_Line2;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = DISABLE;
    EXTI_Init(&EXTI_InitStructure);
}


void at86rf231_gpio_spi_interrupts_init(void)
{
    GPIO_InitTypeDef   GPIO_InitStructure;
    NVIC_InitTypeDef   NVIC_InitStructure;

    // SPI2 init
    at86rf231_spi2_init();

    // IRQ0 : PC2, INPUT and IRQ
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // Enable AFIO clock
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    // Connect EXTI2 Line to PC2 pin
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource2);

    // Configure EXTI2 line
    enable_exti_interrupt();

    // Enable and set EXTI2 Interrupt to the lowest priority
    NVIC_InitStructure.NVIC_IRQChannel = EXTI2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0F;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // Init GPIOs

    // CS & SLEEP
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // RESET
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

void at86rf231_reset(void)
{
    // force reset
    RESET_CLR();
    CSn_SET();
    SLEEP_CLR();

    vtimer_usleep(AT86RF231_TIMING__RESET);

    RESET_SET();

    // Wait until TRX_OFF is entered
    vtimer_usleep(AT86RF231_TIMING__RESET_TO_TRX_OFF);

    // Send a FORCE TRX OFF command
    at86rf231_reg_write(AT86RF231_REG__TRX_STATE, AT86RF231_TRX_STATE__FORCE_TRX_OFF);

    // Wait until TRX_OFF state is entered from P_ON
    vtimer_usleep(AT86RF231_TIMING__SLEEP_TO_TRX_OFF);

    // busy wait for TRX_OFF state
    uint8_t status;
    uint8_t max_wait = 100;   // TODO : move elsewhere, this is in 10us

    do {
        status = at86rf231_get_status();

        vtimer_usleep(10);

        if (!--max_wait) {
            printf("at86rf231 : ERROR : could not enter TRX_OFF mode");
            break;
        }
    }
    while ((status & AT86RF231_TRX_STATUS_MASK__TRX_STATUS) != AT86RF231_TRX_STATUS__TRX_OFF);
}

void at86rf231_spi_select(void)
{
    CSn_CLR();
}
void at86rf231_spi_unselect(void)
{
    CSn_SET();
}

void at86rf231_enable_interrupts(void)
{
    enable_exti_interrupt();
}
void at86rf231_disable_interrupts(void)
{
    disable_exti_interrupt();
}

extern void at86rf231_rx_irq(void);
__attribute__((naked))
void EXTI2_IRQHandler(void)
{
    save_context();

    if (EXTI_GetITStatus(EXTI_Line2) != RESET)
    {
        // IRQ_3 (TRX_END), read Frame Buffer
        EXTI_ClearITPendingBit(EXTI_Line2);

        at86rf231_rx_irq();

        if (sched_context_switch_request) {
            thread_yield();
        }
    }

    restore_context();
}

