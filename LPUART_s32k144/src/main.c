/*
 * main.c       UART transmission simple example
 * 2016 Mar 17 O Romero - Initial version
 * 2016 Oct 31 SM: Clocks adjusted for 160 MHz SPLL
 * 2017 Jul 03 SM: Correced "receive" spelling for function
 */


#include "S32K144.h" /* include peripheral declarations S32K144 */
#include "clocks_and_modes.h"
#include "LPUART.h"

char data=0;
void PORT_init (void) {
  PCC->PCCn[PCC_PORTC_INDEX ]|=PCC_PCCn_CGC_MASK; /* Enable clock for PORTC */
  PORTC->PCR[6]|=PORT_PCR_MUX(2);           /* Port C6: MUX = ALT2,UART1 TX */
  PORTC->PCR[7]|=PORT_PCR_MUX(2);           /* Port C7: MUX = ALT2,UART1 RX */
}

void WDOG_disable (void){
  WDOG->CNT=0xD928C520;     /* Unlock watchdog */
  WDOG->TOVAL=0x0000FFFF;   /* Maximum timeout value */
  WDOG->CS = 0x00002100;    /* Disable watchdog */
}

int main(void)
{
  WDOG_disable();        /* Disable WDGO*/
  SOSC_init_8MHz();      /* Initialize system oscilator for 8 MHz xtal */
  SPLL_init_160MHz();    /* Initialize SPLL to 160 MHz with 8 MHz SOSC */
  NormalRUNmode_80MHz(); /* Init clocks: 80 MHz sysclk & core, 40 MHz bus, 20 MHz flash */
  PORT_init();           /* Configure ports */

  LPUART1_init();        /* Initialize LPUART @ 9600*/
  LPUART1_transmit_string("Running LPUART example\n\r");     /* Transmit char string */
  LPUART1_transmit_string("Input character to echo...\n\r"); /* Transmit char string */

  for(;;) {
	  LPUART1_transmit_char('>');  		/* Transmit prompt character*/
	  LPUART1_receive_and_echo_char();	/* Wait for input char, receive & echo it*/
  }
}


