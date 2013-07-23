/*
 * This file is part of the MSP430 hardware UART example.
 *
 * Copyright (C) 2012 Stefan Wendler <sw@kaltpost.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/******************************************************************************
 * Hardware UART example for MSP430.
 *
 * Stefan Wendler
 * sw@kaltpost.de
 * http://gpio.kaltpost.de
 *
 * Echos back each character received. Blinks green LED in main loop. Toggles
 * red LED on RX.
 *
 * Use /dev/ttyACM0 at 9600 Bauds (and 8,N,1).
 *
 * Note: RX-TX must be swaped when using the MSPg2553 with the Launchpad! 
 *       You could easliy do so by crossing RX/TX on the jumpers of the 
 *       Launchpad.
 * Last modified by: Ye-Sheng Kuo
 ******************************************************************************/

#include "uart.h"
extern volatile unsigned char UART_RX_INT;

void uart_init(void) {
	//uart_set_rx_isr_ptr(0L);
	P1SEL  = RXD + TXD;
  	P1SEL2 = RXD + TXD;
  	UCA0CTL1 |= UCSSEL_2;                     // SMCLK
  	UCA0BR0 = 130;                            // 16MHz 9600, UCBR = 1666 = 256 * 6 + 130
  	UCA0BR1 = 6;                              //								 ^	  ^ (UCA0BR1, UCA0BR0)
  	UCA0MCTL = UCBRS_6 + UCBRF_0;             // Modulation UCBRSx = 6
  	UCA0CTL1 &= ~UCSWRST;                     // Initialize USCI state machine
  	IE2 |= UCA0RXIE;                          // Enable USCI_A0 RX interrupt
}

void uart_putc(unsigned char c) {
	while (!(IFG2&UCA0TXIFG));              // USCI_A0 TX buffer ready?
  	UCA0TXBUF = c;                    		// TX
}

void uart_puts(const char *str) {
     while(*str) uart_putc(*str++);
}

/*
void uart_set_rx_isr_ptr(void (*isr_ptr)(unsigned char c))
{
	uart_rx_isr_ptr = isr_ptr;
}
*/

unsigned char uart_getc() {
	return UCA0RXBUF;
}

#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)
{
	IFG2 &= ~UCA0RXIFG;
	UART_RX_INT = 1;
}

