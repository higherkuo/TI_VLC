
#ifndef __DIGITAL_POT_H_
#define __DIGITAL_POT_H_

typedef enum{
	TX_REF = 0,
	RX_GAIN = 1,
	RX_CMP_REF = 2
} DIGITAL_POT_ENUM_t;

typedef enum{
	INCREASE = 1,
	DECREASE = 0
} GAIN_DIR_t;

void digital_pot_adj(DIGITAL_POT_ENUM_t, GAIN_DIR_t);
void digital_pot_init(void);

#endif
