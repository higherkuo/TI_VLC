EP-light design files

Author: Ye-Sheng Kuo
Date: 7/22 '2013
-----------------------------

File structure:
	BOM: 

	Gerber:
		.G1: inner layer 1
		.G2: inner layer 2
		.GKO: outline layer
		.TXT: NC drill
		.GB*: bottom layer
		.GT*: top layer
		.G*L: copper layer
		.G*S: soldermask layer
		.G*O: silkscreen layer

	Schematic:

	src:
		source code for EP-Light, working with MSP430 Launchpad MSP430G2553.
	The code can be compiled under latest code composer studio (CCS).

		main.c:
			main program. Two macros at the begining of the file. VLC_MODE_TX
		and ARBI_DC. In order to compile the launchpad as a transmitter, 
		users need to define VLC_MODE_TX. Otherwise the node will become a
		receiver. Macro ARBI_DC only works for receiver, it allows the receiver
		support arbitrary duty cycle.

		uart.c, uart.h:
			hardware uart configuration sending/receving control.

		digital_pot.c, digital_pot.h:
			digital potentiometer control.
