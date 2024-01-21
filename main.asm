; Soundchip pinout:

;		   __ __
;		--|  U  |-- GND
;	CS0	--|		|-- SPI CLK
;	CS1	--|		|-- SPI DO (MOSI)
;	GND	--|_____|-- SPI DI (MISO)
;
;	The soundchip is the master of SPI
;	CS0 and CS1 connect to multiplexers to accept 4 targets:
;		0. DAC (Left)
;		1. DAC (Right)
;		2. Incoming register write buffer
;		3. ROM/RAM with wave data
;	
;	The primary DAC im gonna use is DAC7311 by TI. It's smol af
;	




.include "./tn85def.inc"
.org 0
rjmp Init
.org OVF1addr
rjmp Cycle

.org INT_VECTORS_SIZE
Init:
	ldi r16,0
	mov r1,r16
	mov r0,r16
	ldi r16,1<<PB2
	mov r17,r16
	out DDRB,r16
	out PortB,r16
Forever:
	rjmp Forever

Cycle:
	reti