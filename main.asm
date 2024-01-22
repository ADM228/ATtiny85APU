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
;	DACs that will be supported:
;		Name				| Manufacturer
;		DAC7311/6311/5311 	| TI
;		DAC7612				| TI
;		MCP4801/4811/4821	| Microchip
;		MCP4802/4812/4822	| Microchip
;	Feel free to request any other DACs to support



.include "./tn85def.inc"

.cseg

.org 0
rjmp Init
.org OVF1addr
rjmp Cycle

; .org INT_VECTORS_SIZE	; Unnecessary
Init:
	cli
	.ifdef SPH ; if SPH is defined
	ldi	r16,	High(RAMEND)
	out	SPH,	r16 ; Init MSB stack pointer
	.endif
	ldi	r16,	Low(RAMEND)
	out	SPL,	r16 ; Init LSB stack pointer
	ldi	r20,	(1<<PB3)
	; Set timer 0 freq to max for SPI + SPI settings
	clr r16
	clr r17
	clr r0
	clr r1
	out OCR0A,	r16	; Max frequency
	out TCCR0B,	r16	; (0<<FOC0A)|(0<<FOC0B)|(0<<WGM02)|(0<<CS0)
	out USISR,	r16	; (0<<USISIF)|(0<<USIOIF)|(0<<USIPF)|(0<<USICNT0)
	out TIFR,	r16	; Clear interrupts
	out PLLCSR,	r16	; Disable the PLL
	inc r16			; Reset T/C1 prescaler
	out GTCCR,	r16
	inc r16			; (0<<COM0A0)|(0<<COM0B0)|(2<<WGM00)
	out TCCR0A,	r16
	ldi r16, 	(0<<USISIE)|(0<<USIOIE)|(1<<USIWM0)|(2<<USICLK)|(0<<USITC)
	out USICR,	r16
	
	; Set timer 1 to count 256 cycles
	ldi r16,	(1<<TOIE1)	;	Enable overflow interrupt
	out TIMSK,	r16		;__
	ldi r16,	(1<<CS10)	;	Start clock at the rate of CK
	out TCCR1,	r16		;__

	; Set Port modes
	ldi r18,	(1<<PB3)|(1<<PB4)|(0<<PB0)|(1<<PB1)|(1<<PB2)
	out DDRB,	r18
	ldi r18,	(1<<PB3)|(1<<PB4)
	sei
Forever:
	rjmp Forever

Cycle:
	dec r0
	brne End

	in 	r19,	PortB
	eor	r19,	r20
	out	PortB,	r19
; 	sbis PINB,	PINB0	; If no input pending, skip this
; 	rjmp End

; 	; Enable Timer 0 at max freq
; 	out USIDR,	r17	; Clear SPI Data register
; 	ldi r16,	(0<<FOC0A)|(0<<FOC0B)|(0<<WGM02)|(1<<USICS0)
; 	out TCCR0B,	r16
; 	; Wait, somehow
; WaitLoop:
; 	in	r16,	USISR
; 	sbrs r16,	USIOIF
; 	rjmp WaitLoop
	
; 	out	TCCR0B,	r17		; Disable clock
	
; 	; Get output
; 	in	r16,	USIDR
; 	cpi r16,	1	; Check address
; 	brne End

; 	; The address is right
; 	; Enable Timer 0 at max freq
; 	out USIDR,	r17	; Clear SPI Data register
; 	ldi r16,	(0<<FOC0A)|(0<<FOC0B)|(0<<WGM02)|(1<<USICS0)
; 	out TCCR0B,	r16
; 	; Wait, somehow
; WaitLoop2:
; 	in	r16,	USISR
; 	sbrs r16,	USIOIF
; 	rjmp WaitLoop2

; 	out	TCCR0B,	r17		; Disable clock
; 	; Get output, Toggle LED
; 	in	r16,	USIDR
; 	and	r16,	r18
; 	out	PortB,	r16

End:
	reti
