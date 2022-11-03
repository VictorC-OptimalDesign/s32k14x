/*
 * Copyright (c) 2015 - 2016 , Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.
 *
 * THIS SOFTWARE IS PROVIDED BY NXP "AS IS" AND ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL NXP OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
 /* ###################################################################
**     Filename    : main.c
**     Project     : lpuart_echo_s32k142
**     Processor   : S32K142_100
**     Version     : Driver 01.00
**     Compiler    : GNU C Compiler
**     Date/Time   : 2017-08-04, 12:27, # CodeGen: 1
**     Abstract    :
**         Main module.
**         This module contains user's application code.
**     Settings    :
**     Contents    :
**         No public methods
**
** ###################################################################*/
/*!
** @file main.c
** @version 01.00
** @brief
**         Main module.
**         This module contains user's application code.
*/
/*!
**  @addtogroup main_module main module documentation
**  @{
*/
/* MODULE main */

/* Including needed modules to compile this module/procedure */
#include "Cpu.h"
#include "pin_mux.h"
#include "dmaController1.h"
#include "clockMan1.h"
#include "lpuart1.h"

volatile int exit_code = 0;
/* User includes (#include below this line is not maintained by Processor Expert) */
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define welcomeMsg "This example is an simple echo using LPUART\r\n\
it will send back any character you send to it.\r\n\
The board will greet you if you send 'Hello Board'\r\
\nNow you can begin typing:\r\n"

/*!< Macro to enable all interrupts. */
// This function enables IRQ interrupts by clearing the I-bit in the CPSR.
#define EnableInterrupts()      __asm__(" CPSIE i")

/*!< Macro to disable all interrupts. */
// This function disables IRQ interrupts by setting the I-bit in the CPSR.
#define DisableInterrupts()     __asm__(" CPSID i")

static void SCG_Init(void)
{
    // Configure SCG

    /* FIRC 48MHz, core clock 48MHz, system clock 48MHz, bus clock 48MHz, Flash clock 24MHz */
    /* FIRC Configuration 48MHz */
    SCG->FIRCDIV = SCG_FIRCDIV_FIRCDIV1(0b100)      /*FIRC DIV1=8*/
            |SCG_FIRCDIV_FIRCDIV2(0b001);           /*FIRC DIV2=1*/

    SCG->FIRCCFG =SCG_FIRCCFG_RANGE(0b00);          /* Fast IRC trimmed 48MHz*/

    while(SCG->FIRCCSR & SCG_FIRCCSR_LK_MASK);      /*Is PLL control and status register locked?*/

    SCG->FIRCCSR = SCG_FIRCCSR_FIRCEN_MASK;         /*Enable FIRC*/

    while(!(SCG->FIRCCSR & SCG_FIRCCSR_FIRCVLD_MASK));  /*Check that FIRC clock is valid*/

    /* RUN Clock Configuration */
    SCG->RCCR=SCG_RCCR_SCS(0b0011)                  /* FIRC as clock source*/
    |SCG_RCCR_DIVCORE(0b00)                         /* DIVCORE=1, Core clock = 48 MHz*/
    |SCG_RCCR_DIVBUS(0b00)                          /* DIVBUS=1, Bus clock = 48 MHz*/
    |SCG_RCCR_DIVSLOW(0b01);                        /* DIVSLOW=2, Flash clock= 24 MHz*/

    // Enable very low power modes
    SMC->PMPROT = SMC_PMPROT_AVLP_MASK;
}

static void PCC_Init(void)
{
    // PORTA
    PCC->PCCn[PCC_PORTA_INDEX] = PCC_PCCn_CGC_MASK;
    // PORTB
    PCC->PCCn[PCC_PORTB_INDEX] = PCC_PCCn_CGC_MASK;
    // PORTC
    PCC->PCCn[PCC_PORTC_INDEX] = PCC_PCCn_CGC_MASK;
    // PORTD
    PCC->PCCn[PCC_PORTD_INDEX] = PCC_PCCn_CGC_MASK;
    // PORTE
    PCC->PCCn[PCC_PORTE_INDEX] = PCC_PCCn_CGC_MASK;

    // Configure PCC
    // LPTMR0
    if(PCC->PCCn[PCC_LPTMR0_INDEX] & 0x40000000)
    {PCC->PCCn[PCC_LPTMR0_INDEX]  = 0x80000000;}

    PCC->PCCn[PCC_LPTMR0_INDEX] = 0xC0000000;   //0xC3000000;       // enable clock, use FIRCDIV2 clock, no clock division

    // ADC0
    if(PCC->PCCn[PCC_ADC0_INDEX] & 0x40000000)
    {PCC->PCCn[PCC_ADC0_INDEX]  = 0x80000000;}
    PCC->PCCn[PCC_ADC0_INDEX] = 0xC3000000;         // enable clock, use FIRCDIV2 clock, no clock division

    // ADC1
    if(PCC->PCCn[PCC_ADC1_INDEX] & 0x40000000)
    {PCC->PCCn[PCC_ADC1_INDEX]  = 0x80000000;}
    PCC->PCCn[PCC_ADC1_INDEX] = 0xC3000000;         // enable clock, use FIRCDIV2 clock, no clock division

    // LPUART0
    PCC->PCCn[PCC_LPUART0_INDEX] = 0xC3000000;      // enable clock, use FIRCDIV2 clock, no clock division

    // LPUART1
    PCC->PCCn[PCC_LPUART1_INDEX] = 0xC3000000;      // enable clock, use FIRCDIV2 clock, no clock division

    // DMA MUX
    PCC->PCCn[PCC_DMAMUX_INDEX] = 0xC3000000;
}

/*!
 \brief The main function for the project.
 \details The startup initialization sequence is the following:
 * - startup asm routine
 * - main()
 */
int main(void)
{
  /* Write your local variable definition here */
  bool strReceived = false;
  /* Declare a buffer used to store the received data */
  uint8_t	 buffer[255] 	=	{0, };
  uint8_t  i = 0;
  uint32_t bytesRemaining;
  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
#ifdef PEX_RTOS_INIT
  PEX_RTOS_INIT(); /* Initialization of the selected RTOS. Macro is defined by the RTOS component. */
#endif
  /*** End of Processor Expert internal initialization.                    ***/

  /* Write your code here */
  /* For example: for(;;) { } */

  DisableInterrupts();

#if 0
  /* Initialize and configure clocks
   * 	-	see clock manager component for details
   */
  CLOCK_SYS_Init(g_clockManConfigsArr, CLOCK_MANAGER_CONFIG_CNT,
                 g_clockManCallbacksArr, CLOCK_MANAGER_CALLBACK_CNT);
  CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_AGREEMENT);
#else
  SCG_Init();                             // System clock generator init
  PCC_Init();                             // Peripheral clock enable
#endif

  /* Initialize pins
   *	-	See PinSettings component for more info
   */
  PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);

  /* Initialize LPUART instance */
  LPUART_DRV_Init(INST_LPUART1, &lpuart1_State, &lpuart1_InitConfig0);

  EnableInterrupts();

  /* Send a welcome message */
  LPUART_DRV_SendData(INST_LPUART1, (uint8_t *)welcomeMsg, strlen(welcomeMsg));
  /* Wait for transmission to be complete */
  while(LPUART_DRV_GetTransmitStatus(INST_LPUART1, &bytesRemaining) != STATUS_SUCCESS);

  /* Infinite loop:
   * 	- Receive data from user
   * 	- Echo the received data back
   */
  while (1)
    {
      /* Get the received data */
      while(strReceived == false)
        {
          /* Because the terminal appends new line to user data,
          *	 receive and store data into a buffer until it is received
          */
          LPUART_DRV_ReceiveData(INST_LPUART1, &buffer[i], 1UL);
          /* Wait for transfer to be completed */
          while(LPUART_DRV_GetReceiveStatus(INST_LPUART1, &bytesRemaining) != STATUS_SUCCESS);
          /* Check if current byte is new line */
          if(buffer[i++] == '\n')
            strReceived = true;
        }
      /* Append null termination to the received string */
      buffer[i] = 0;
      /* Check if data is "Hello Board".
      *	 If comparison is true, send back "Hello World"
      */
      if(strcmp((char *)buffer, "Hello Board\r\n") == 0)
        {
          strcpy((char *)buffer, "Hello World!!\r\n");
          i = strlen((char *)buffer);
        }

      /* Send the received data back */
      LPUART_DRV_SendData(INST_LPUART1, buffer, i);
      /* Wait for transmission to be complete */
      while(LPUART_DRV_GetTransmitStatus(INST_LPUART1, &bytesRemaining) != STATUS_SUCCESS);
      /* Reset the buffer length and received complete flag */
      strReceived = false;
      i = 0;
    }
  /*** Don't write any code pass this line, or it will be deleted during code generation. ***/
  /*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS component. DON'T MODIFY THIS CODE!!! ***/
  #ifdef PEX_RTOS_START
    PEX_RTOS_START();                  /* Startup of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of RTOS startup code.  ***/
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for(;;) {
    if(exit_code != 0) {
      break;
    }
  }
  return exit_code;
  /*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
} /*** End of main routine. DO NOT MODIFY THIS TEXT!!! ***/

/* END main */
/*!
 ** @}
 */
/*
 ** ###################################################################
 **
 **     This file was created by Processor Expert 10.1 [05.21]
 **     for the Freescale S32K series of microcontrollers.
 **
 ** ###################################################################
 */

