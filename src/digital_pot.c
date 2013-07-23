
#include "digital_pot.h"
#include "msp430g2553.h"
// port mapping
// POT_UD -> P1.7
// POT_CLK -> P1.6
// POT_TX_REF -> P2.5
// POT_RX_GAIN -> P2.4
// POT_CMP_REF -> P2.3
//
void digital_pot_init(void){
	P1DIR |= BIT6 + BIT7;
	P1OUT |= BIT6 + BIT7;
	P2DIR |= BIT3 + BIT4 + BIT5;
	P2OUT |= BIT3 + BIT4 + BIT5;
}

// P1OUT high -> increase Rwb, decrease rx gain 
// (need to modify schematic in the future)
// POT adj procedure:
// 1. assert/deassert POT_UD
// 2. deassert POT_CS
// 3. deassert POT_CLK
// 4. assert POT_CS
// 5. assert POT_CLK
// Note: 1,2 could be done at the same cycle
void digital_pot_adj(DIGITAL_POT_ENUM_t pot_type, GAIN_DIR_t direction){

	if (pot_type==RX_GAIN){
		if (direction==INCREASE)
			P1OUT &= ~BIT7;
		else
			P1OUT |= BIT7;
	}
	else{
		if (direction==INCREASE)
			P1OUT |= BIT7;
		else
			P1OUT &= ~BIT7;
	}

	switch (pot_type){
		case TX_REF:	// P2.5
			P2OUT &= ~BIT5;
		break;
		case RX_GAIN: // P2.4
			P2OUT &= ~BIT4;
		break;
		case RX_CMP_REF: // P2.3
			P2OUT &= ~BIT3;
		break;
	}
	P1OUT &= ~BIT6;
	P2OUT |= BIT3 + BIT4 + BIT5;
	P1OUT |= BIT6;
}
