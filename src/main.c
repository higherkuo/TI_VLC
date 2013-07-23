
#include <msp430.h> 
#include "msp430g2553.h"
#include "digital_pot.h"
#include "uart.h"

/*
 * main.c
 * VLC Booster Pack 
 * Author: Ye-sheng Kuo
 */

#define VLC_MODE_TX
//#define ARBI_DC

#define SFD 0xa7

// Function prototype
#ifdef VLC_MODE_TX
void vlc_transmit(unsigned char*, unsigned char, unsigned int, unsigned int);
inline unsigned char bit_extraction(unsigned char*, unsigned char, unsigned char);
#else
int timer_diff_cal(int, int);
void rx_cmp_agc(void);
#endif

volatile unsigned char TIMER0_TAIV_INT, TIMER1_TAIV_INT, TIMER1_COV_INT, TIMER0_CCR0_INT ;
volatile unsigned char BUTTON_INT;
volatile unsigned char tx_bit_out, tx_next_bit_out;
volatile unsigned char UART_RX_INT;
volatile unsigned int timer1_cap_val;
volatile unsigned char RX_CMP_INT;

// PORTS definition
#define TX_GPIO BIT3
#define RX_CMP BIT1
#define LED1 BIT0

int main(void) {
	// Stop watchdog timer
    WDTCTL = WDTPW | WDTHOLD;
	// Set clock source
	BCSCTL1 = CALBC1_16MHZ;
	DCOCTL = CALDCO_16MHZ;
	// MCLK = 16 MHz, SMCLK = 16 MHz
	BCSCTL2 = SELM_0 + DIVM_0 + DIVS_0;
	// Enabling interrupts
	__enable_interrupt();
	// interrupt variable
	TIMER0_TAIV_INT = 0;
	BUTTON_INT = 0;
	TIMER0_CCR0_INT = 0;
	TIMER1_TAIV_INT = 0;
	TIMER1_COV_INT = 0;
	UART_RX_INT = 0;
	// UART
	uart_init();
	// Digital Pot
	digital_pot_init();
	P1DIR |= LED1;		// test purpose only
	P1OUT &= ~LED1;

#ifdef VLC_MODE_TX
	uart_puts("\r\nVLC TX mode\r\n");
	unsigned char user_input;
	DIGITAL_POT_ENUM_t my_pot = TX_REF;
	GAIN_DIR_t my_gain = INCREASE;
	digital_pot_adj(my_pot, my_gain, 64); // max TX power

	// Set P1.2 output mode
	P1DIR |= TX_GPIO;		// using P1.3

	// global variable
	tx_bit_out = 0;
	tx_next_bit_out = 0;

	unsigned char data_array[127];
	unsigned char number_of_element = 11;
	data_array[0] = 0x30;
	data_array[1] = 0x30;
	data_array[2] = 0x30;
	data_array[3] = 0x30;
	data_array[4] = 0xa7;
	data_array[5] = 0x05;
	data_array[6] = 0x00;
	data_array[7] = 0x00;
	data_array[8] = 0x00;
	data_array[9] = 0x00;

	/*
		16000	= 1 Kbps
		8000	= 2 Kbps
		4000	= 4 Kbps
		2000	= 8 Kbps
		1000	= 16 Kbps
		500		= 32 Kbps
	}
	*/
	unsigned int period = 1000;
	unsigned int threshold1 = 700;
	unsigned int threshold2 = period - threshold1;
	//TA0CCR1 = (TA0CCR0>>1);	// 50% DC
	TA0CCR0 = period;
	TA0CCR1 = threshold1;
	// configure timer 0
	// select timer source from SMCLK, /1, up counting mode
	TA0CTL = TASSEL_2 + ID_0 + MC_1;
	TA0CCTL1 = CCIE;
	TA0CCTL0 = CCIE;	// CCR0 interrupt
#else
	uart_puts("\r\nVLC RX mode\r\n");
	rx_cmp_agc();
	typedef enum {
		EDGE_FINDING,
		EDGE_FOUND
	} RX_EDGE_MODE_t;
	RX_EDGE_MODE_t rx_edge_mode = EDGE_FINDING;

	typedef enum{
		IDLE,
#ifdef ARBI_DC
		TRAIN_SHORT1,
		TRAIN_LONG1,
		TRAIN_SHORT0,
		TRAIN_LONG0,
#else
		TRAIN_SHORT,
		TRAIN_LONG,
#endif
		SET_TIMER,
		COLLECT_DATA
	} RX_TRAIN_MODE_t;
	RX_TRAIN_MODE_t rx_train_mode = IDLE;

	typedef enum{
		WATCHDOG_STBY,
		WAIT_SFD,
		LENGTH_FIELD,
		DATA_FIELD
	} RX_MODE_t;
	RX_MODE_t rx_mode = WATCHDOG_STBY;

	// configure timer 1
	// select timer source from SMCLK, /1, continues mode
	TA1CTL = TASSEL_2 + ID_0 + MC_2;
	// capture both edges, asyn, CCI1A, capture en, interrupt en
	TA1CCTL1 = CM_3 + CCIS_0 + SCS + CAP + CCIE;
	// Set P2.1 input mode with capture
	P2DIR &= ~RX_CMP;
	P2DIR |= BIT3 + BIT4 + BIT5;
	P2OUT &= ((~BIT3) & (~BIT4) & (~BIT5));
	P2SEL |= BIT1;					// Select primary peripheral module (timer)

	unsigned int timer1_diff;
	unsigned int timer1_last_cap_val;
	unsigned char bit_in, byte_in, bit_counter;
	unsigned char length_cnt;
	unsigned char rx_data_buf[128] = {0};
	unsigned char uart_temp, i;
	TA0CTL = TASSEL_2 + ID_0 + MC_1;
	TA0CCR0 = 65535;
	TA0CCTL0 = CCIE;
	unsigned int watchdog_cnt = 500;
	unsigned int period;
	typedef enum{
		POSITIVE_EDGE,
		NEGATIVE_EDGE
	} EDGE_TYPE_t;
	EDGE_TYPE_t edge_out = POSITIVE_EDGE;
	#ifdef ARBI_DC
	unsigned int short0, short1;
	#endif
#endif

	while (1){
#ifdef VLC_MODE_TX
		if (UART_RX_INT==1){
			UART_RX_INT = 0;
			user_input = uart_getc();
			if (user_input=='\r')
				uart_puts("\r\n");
			else
				uart_putc(user_input);
			data_array[10] = user_input;
			vlc_transmit(data_array, number_of_element, threshold1, threshold2);
		}
#else
		if (TIMER1_TAIV_INT==1){
			TIMER1_TAIV_INT = 0;
			switch (rx_edge_mode){
				case EDGE_FINDING:
					rx_edge_mode = EDGE_FOUND;
					timer1_last_cap_val = timer1_cap_val;
				break;

				case EDGE_FOUND:
					timer1_diff = timer_diff_cal(timer1_cap_val, timer1_last_cap_val);
					timer1_last_cap_val = timer1_cap_val;
					if ((P2IN & RX_CMP)==0x2)
						edge_out = POSITIVE_EDGE;
					else
						edge_out = NEGATIVE_EDGE;
				break;
			}
			switch (rx_train_mode){

				case IDLE:
					if (rx_edge_mode==EDGE_FOUND)
					#ifdef ARBI_DC
						rx_train_mode = TRAIN_SHORT1;
					#else
						rx_train_mode = TRAIN_SHORT;
					#endif
				break;

#ifdef ARBI_DC
				case TRAIN_SHORT1:
					if (edge_out==NEGATIVE_EDGE){
						short1 = timer1_diff;
						rx_train_mode = TRAIN_LONG1;
					}
				break;

				case TRAIN_LONG1:
					// 1.75 < diff < 2.25
					if (edge_out==NEGATIVE_EDGE){
						if ((timer1_diff > (short1 + (short1>>1) + (short1>>2))) && (timer1_diff < ((short1<<1) + (short1>>2))))
							rx_train_mode = TRAIN_SHORT0;
						else
							short1 = timer1_diff;
					}
				break;

				case TRAIN_SHORT0:
					if (edge_out==POSITIVE_EDGE){
						short0 = timer1_diff;
						rx_train_mode = TRAIN_LONG0;
					}
				break;

				case TRAIN_LONG0:
					// 1.75 < diff < 2.25
					if (edge_out==POSITIVE_EDGE){
						if ((timer1_diff > (short0 + (short0>>1) + (short0>>2))) && (timer1_diff < ((short0<<1) + (short0>>2)))){
							period = short1 + short0;
							rx_train_mode = SET_TIMER;
							TA0CCR0 = 0;
							rx_mode = WAIT_SFD;
							byte_in = 0;
						}
						else{
							short0 = timer1_diff;
						}
					}
				break;
#else
				case TRAIN_SHORT:
					period = timer1_diff;
					rx_train_mode = TRAIN_LONG;
				break;

				case TRAIN_LONG:
					// 1.75 < diff < 2.25
					if ((timer1_diff > (period + (period>>1) + (period>>2))) && (timer1_diff < ((period<<1) + (period>>2)))){
						period = timer1_diff;
						rx_train_mode = SET_TIMER;
						TA0CCR0 = 0;
						rx_mode = WAIT_SFD;
						byte_in = 0;
					}
					else{
						period = timer1_diff;
					}
				break;
#endif

				case SET_TIMER:
					TA0CCR0 = period;
					watchdog_cnt = 32768;
					rx_train_mode = COLLECT_DATA;
				break;

				case COLLECT_DATA:
					// Valid trigger point
					if ((TA0R < ((period>>1) + (period>>2) + (period>>3))) && (TA0R> (period>>3))){
						#ifdef ARBI_DC
						if (edge_out==POSITIVE_EDGE){
							bit_in = 0;
							TA0R = short0;
						}
						else{
							bit_in = 1;
							TA0R = short1;
						}
						#else
						if (edge_out==POSITIVE_EDGE)
							bit_in = 0;
						else
							bit_in = 1;
						TA0R = (period>>1);
						#endif
					}
					else {
						// symbol realign
						if (TA0R < (period>>3))
							TA0R = 0;
						//else
						//	TA0R = period-1;
					}
				break;
			}
		}

		if (TIMER0_CCR0_INT==1){
			TIMER0_CCR0_INT = 0;
			byte_in = (((byte_in<<1) + bit_in) & 0xff);

			switch (rx_mode){
				case WAIT_SFD:
					if (byte_in==SFD){
						rx_mode = LENGTH_FIELD;
						bit_counter = 0;
						P1OUT |= LED1;
					}
					else if (watchdog_cnt==0){
						TA0CCR0 = 0;		// stop timer
						watchdog_cnt = 500;
						TA0CCR0 = 65535;
						rx_train_mode = IDLE;
						rx_edge_mode = EDGE_FINDING;
						rx_mode = WATCHDOG_STBY;
					}
					else
						watchdog_cnt--;
				break;

				case LENGTH_FIELD:
					bit_counter++;
					if (bit_counter==8){
						length_cnt = 0;
						bit_counter = 0;
						rx_mode = DATA_FIELD;
						rx_data_buf[0] = (unsigned char)byte_in;
					}
				break;

				case DATA_FIELD:
					bit_counter++;
					if (bit_counter==8){
						bit_counter = 0;
						length_cnt++;
						rx_data_buf[length_cnt] = (unsigned char)byte_in;
					}

					if (length_cnt==rx_data_buf[0]) {
						TA0CCR0 = 0;
						P1OUT &= ~LED1;
						byte_in = 0;
						if (rx_data_buf[5]=='\r')
							uart_puts("\r\n");
						else
							uart_putc(rx_data_buf[5]);
						rx_mode = WATCHDOG_STBY;
						rx_train_mode = IDLE;
						rx_edge_mode = EDGE_FINDING;
						TA0CCR0 = 65535;
						watchdog_cnt = 500;
					}

				break;

				case WATCHDOG_STBY:
					if (watchdog_cnt==0){
						rx_train_mode = IDLE;
						rx_edge_mode = EDGE_FINDING;
						watchdog_cnt = 500;
					}
					else
						watchdog_cnt--;
				break;
			}
		}
#endif
	}
}

#ifdef VLC_MODE_TX
// ISRs
// Timer 0 CCR0 interrupt vector
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer0_A_CCR0 (void) 
{ 
	TA0CCTL0 &= ~CCIFG;
	TIMER0_CCR0_INT = 1;
	if (tx_next_bit_out==1)
		P1OUT |= TX_GPIO;
	else
		P1OUT &= ~TX_GPIO;
}

// Timer 0 TAIV interrupt vector
#pragma vector=TIMER0_A1_VECTOR
__interrupt void Timer0_A_TAIV (void) 
{ 
	TA0CCTL1 &= ~CCIFG;	// clear interrupt flag
	TIMER0_TAIV_INT = 1;
	if (tx_bit_out==0)
		P1OUT |= TX_GPIO;
	else
		P1OUT &= ~TX_GPIO;
}

// Timer 1 TAIV interrupt vector
#pragma vector=TIMER1_A1_VECTOR
__interrupt void Timer1_A_TAIV (void) 
{ 
	TA1CTL &= ~TAIFG;
	TIMER1_TAIV_INT = 1;
}

// End of ISR
void vlc_transmit(unsigned char* data, unsigned char num_of_element, unsigned int thres1, unsigned int thres2){
	unsigned char i, k;
	TIMER0_TAIV_INT = 0;			// clear previous interrupt
	TIMER0_CCR0_INT = 0;
	while (TIMER0_TAIV_INT==0){}	// Wait for 1 interrupt
	TIMER0_TAIV_INT = 0;
	for (i=num_of_element; i>0; i--){
		for (k=8; k>0; k--){
			tx_next_bit_out = bit_extraction(data, (num_of_element-i), (k-1));
			while (TIMER0_CCR0_INT==0){}
			TIMER0_CCR0_INT = 0;
			tx_bit_out = tx_next_bit_out;
			if (tx_bit_out==1)
				TA0CCR1 = thres2;
			else
				TA0CCR1 = thres1;
		}
	}
	
	tx_next_bit_out = 0;
	while(TIMER0_CCR0_INT==0){}
	TA0CCR1 = thres1;
	tx_bit_out = 0;
}

inline unsigned char bit_extraction(unsigned char* data, unsigned char idx, unsigned char bit_position){
	if (data[idx] & (0x1<<bit_position))
		return 1;
	else
		return 0;
}

#else
// Timer 0 CCR0 interrupt vector
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer0_A_CCR0 (void) 
{ 
	TA0CCTL0 &= ~CCIFG;
	TIMER0_CCR0_INT = 1;
}

// Timer 1 TAIV interrupt vector
#pragma vector=TIMER1_A1_VECTOR
__interrupt void Timer1_A_TAIV (void) 
{ 
	timer1_cap_val = TA1CCR1;
	TA1CCTL1 &= 0x3fff;	// disable capture
	P2OUT |= BIT4;
	TIMER1_TAIV_INT = 1;
	TA1CCTL1 &= ((~CCIFG) & (~COV));	// clear both interrupt flags
	__delay_cycles(32);	// delay 10 us
	TA1CCTL1 |= CM_3;	// enable capture
	P2OUT &= ~BIT4;
}

int timer_diff_cal(int timer1_cur_val, int timer1_cap_val_pre){
	if (timer1_cur_val> timer1_cap_val_pre)
		return (timer1_cur_val - timer1_cap_val_pre);
	else
		return timer1_cur_val + (0xffff - timer1_cap_val_pre);
}

void rx_cmp_agc(void){
	P2DIR &= ~RX_CMP;	// configure input mode
	unsigned char j;
	DIGITAL_POT_ENUM_t my_pot;
	GAIN_DIR_t my_gain;
	my_pot = RX_CMP_REF;
	my_gain = INCREASE;
	digital_pot_adj(my_pot, my_gain, 64);
	P2IE |= RX_CMP;			// Enable interrupt
	P2IES &= ~RX_CMP;		// Set positive edge interrupt
	RX_CMP_INT = 0;
	
	my_gain = DECREASE;
	for (j=64; j>0; j--){
		digital_pot_adj(my_pot, my_gain, 1);
		__delay_cycles(10);	// delay 10 cycles 
		if (RX_CMP_INT){
			P2IE &= ~RX_CMP;// Diasble interrupt
			break;
		}
	}

	// by experience
	my_gain = INCREASE;
	digital_pot_adj(my_pot, my_gain, 9);
	RX_CMP_INT = 0;
}

#endif

#pragma vector=PORT2_VECTOR
__interrupt void Port2_INT (void){
	P2IFG &= ~RX_CMP;	// clear interrupt flag
	RX_CMP_INT = 1;
}
