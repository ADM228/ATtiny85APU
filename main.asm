; APU pinout:

;			   __ __
;			-1|° U  |8- GND
;		CS0	-2|		|7- SPI CLK
;	OUT/CS1	-3|		|6- SPI DO (MOSI)
;		GND	-4|_____|5- SPI DI (MISO)
;
;	The APU is the master of SPI
;	CS0 and CS1 connect to multiplexers to accept 4 targets:
;		0. Incoming register write buffer
;		1. ROM/RAM with wave data
;		2. DAC (Left if stereo enabled)
;		3. DAC (Right) (if stereo enabled)
;	
;	The primary DAC im gonna use is DAC7311 by TI. It's smol af
;	DACs that will be supported:
;		Name				| Manufacturer	| Stereo support
;		DAC7311/6311/5311 	| TI			| Yes (needs 2x)
;		DAC7612				| TI			| Yes
;		MCP4801/4811/4821	| Microchip		| Yes (needs 2x)
;		MCP4802/4812/4822	| Microchip		| Yes
;		PWM output on pin 3	| Microchip 	| No
;	Feel free to request any other DACs to support

;	Commands are put in via SPI from a shift register/serial
;	buffer IC. An example can be the 74'165.
;		Note: if the IC shifts out signals on a rising clock
;		pulse, an inverter needs to be connected between its
;		clock pin and the APU's SPI CLK pin. An example with TI
;		chips:

;			.-----------------------.
;		  .-|-----..------.    		|
;		 8765    4321098  |  65432109
;		[APU ]  [ 74'04 ] | [ 74'165 ]
;		 1234    1234567  |  12345678
;						  '---'


.include "./tn85def.inc"

.cseg

.org 0
rjmp Init
.org OVF1addr
rjmp Cycle

.equ	USCK	= PB2
.equ	DO		= PB1
.equ	DI		= PB0

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
	clr r31
	clr r19
	clr r0
	clr r1
	out TCCR0B,	r16	; (0<<FOC0A)|(0<<FOC0B)|\	; No force strobe
					; (0<<WGM02)|\				; Total wavegen mode = 010 (CTC)
					; (0<<CS00)					; No clock source
	out USISR,	r16	; (0<<USISIF)|(0<<USIOIF)|\	; No interrupts
					; (0<<USIPF)|\				; No stop condition
					; (0<<USICNT0)				; Counter at 0
	out TIFR,	r16	; Clear interrupts
	out PLLCSR,	r16	; Disable the PLL
	inc r16			; Reset T/C1 prescaler
	out OCR0A,	r16	; Max frequency
	out GTCCR,	r16
	inc r16			; (0<<COM0A0)|(0<<COM0B0)|\	; Normal port operation
					; (2<<WGM00)				; Total wavegen mode = 010 (CTC)
	out TCCR0A,	r16
	; No interrupts, 3-wire mode (SPI),
	; CLK src: Timer0 CMP match, No need to toggle pin	
	ldi r16, 	(0<<USISIE)|(0<<USIOIE)|(1<<USIWM0)|(2<<USICLK)|(0<<USITC)
	out USICR,	r16
	
	; Set timer 1 to count 256 cycles
	ldi r16,	(1<<TOIE1)	;	Enable overflow interrupt
	out TIMSK,	r16			;__
	ldi r16,	(1<<CS10)	;	Start clock at the rate of CK
	out TCCR1,	r16			;__
	; Set Port modes
	ldi r18,	(1<<PB3)|(1<<PB4)|(0<<DI)|(1<<DO)|(1<<USCK)
	out DDRB,	r18
	ldi r18,	(1<<PB3)|(1<<PB4)
	out PortB,	r18
	ldi r16,	0x05
	out USIDR,	r16
	sei
Forever:
	rjmp Forever

Cycle:
	cbi	PortB,	PB4
	sbis PINB,	PINB0	; If no input pending, skip this
	rjmp End

	sbi	PortB,	PB4
	cbi	PortB,	PB3

	; Copied directly from Microchip's docs

	ldi	r16,	(1<<USIOIF)|(8<<USICNT0)
	out	USISR,	r16
	ldi	r16,	(1<<USIWM0)|(0<<USICS1)|(1<<USICLK)|(1<<USITC)
	ldi r18,	(1<<USIWM0)|(0<<USICS1)|(0<<USICLK)|(1<<USITC)
	SPITransfer_loop0:
	out	USICR,	r18	; make rising edge
	; rcall	Delay
	out USICR,	r16	; shift data out on falling edge, in right before
	; rcall	Delay
	in	r17, 	USISR
	sbrs r17, 	USIOIF
	rjmp SPITransfer_loop0

	sbi PortB,	PB3
	; rcall	Delay
	cbi	PortB, 	PB3
	; ldi	r17,	(1<<USIOIF)
	; out	USISR,	r17
	; SPITransfer_loop1:
	; out	USICR,	r16
	; rcall	Delay
	; in	r17, 	USISR
	; sbrs r17, 	USIOIF
	; rjmp SPITransfer_loop1

	cbi	PortB,	PB4

	in	r16,	USIBR
	cpi	r16,	0x80
	brne End
	sbi PortB,	PB3

	; in	r16,	USIDR

; 	; Enable Timer 0 at max freq
; 	out USIDR,	r31	; Clear SPI Data register
; 	; No force strobe, Total wavegen mode = 010 (CTC),
; 	; Clock source = clkIO/1024 (no prescaling)
; 	ldi r16,	(0<<FOC0A)|(0<<FOC0B)|(0<<WGM02)|(5<<CS00)			
; 	out TCCR0B,	r16
; 	; Wait, somehow
; WaitLoop:
; 	in	r16,	USISR							;	Has counter overflowed yet?
; 	sbrs r16,	USIOIF							;__ If no, jump back
; 	rjmp WaitLoop
	
	; out	TCCR0B,	r31		; Disable clock
	
	; Get output

; 	; The address is right
; 	; Enable Timer 0 at max freq
; 	out USIDR,	r31	; Clear SPI Data register
; 	ldi r16,	(0<<FOC0A)|(0<<FOC0B)|(0<<WGM02)|(1<<USICS0)
; 	out TCCR0B,	r16
; 	; Wait, somehow
; WaitLoop2:
; 	in	r16,	USISR
; 	sbrs r16,	USIOIF
; 	rjmp WaitLoop2

; 	out	TCCR0B,	r31		; Disable clock
; 	; Get output, Toggle LED
	; in	r16,	USIDR
	; and	r16,	r18
	; out	PortB,	r16
	; eor r19,	r18
	; out PortB,	r19

End:
	reti



; Delay:
; 	clr	r30
; 	ldi r29,	0x00
; 	delay_loop:
; 		dec r30
; 		brne delay_loop

; 			dec r29
; 			brne delay_loop
	
; 	ret