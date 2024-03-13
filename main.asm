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


;	Registers:

;					 _______________________________________________________________
;	Bit ->			|	7	|	6	|	5	|	4	|	3	|	2	|	1	|	0	|
;	Index	|  Name |=======|=======|=======|=======|=======|=======|=======|=======|
;	0x00	| PILOA	|		Pitch increment value for tone on channel A				|
;	0x01	| PILOB	|		Pitch increment value for tone on channel B				|
;	0x02	| PILOC	|		Pitch increment value for tone on channel C				|
;	0x03	| PILOD	|		Pitch increment value for tone on channel D				|
;	0x04	| PILOE	|		Pitch increment value for tone on channel E				|
;	0x05	| PILON	|		Pitch increment value for noise generator				|
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;	0x06	| PHIAB	|Ch.B PR|Channel B octave number|Ch.A PR|Channel A octave number|	PR = Phase reset
;	0x07	| PHICD	|Ch.D PR|Channel D octave number|Ch.C PR|Channel C octave number|
;	0x08	| PHIEN	|NoisePR|Noise gen octave number|Ch.E PR|Channel E octave number|
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;	0x09	| DUTYA	|					Channel A tone duty cycle					|
;	0x0A	| DUTYB	|					Channel B tone duty cycle					|
;	0x0B	| DUTYC	|					Channel C tone duty cycle					|
;	0x0C	| DUTYD	|					Channel D tone duty cycle					|
;	0x0D	| DUTYE	|					Channel E tone duty cycle					|
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;	0x0E	| NTPLO	|			Noise LFSR inversion value (low byte)				|
;	0x0F	| NTPHI	|			Noise LFSR inversion value (high byte)				|
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;					|		When UseEnvX == 0, Channel X uses static volume			|
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;	0x1n	| VOLX	|UseEnvX|				Channel X static volume					|	n = 0..4, X = A..E
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;					|	When UseEnvX == 1, Channel X uses an envelope or sample		|
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;	0x1n	| VOLX	|UseEnvX|Env/Smp| Slot# |
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;	0x15	| MIX0	|NoiEnD	|ToneEnD|NoiEnC	|ToneEnC|NoiEnB	|ToneEnB|NoiEnA	|ToneEnA|
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;	0x16	| PANAB	|Right vol ch. B|Left vol ch. B |Right vol ch. A|Left vol ch. A |
;	0x17	| PANCD	|Right vol ch. D|Left vol ch. D |Right vol ch. C|Left vol ch. C |
;	0x18	| MXPNE	|Right vol ch. E|Left vol ch. E | ------------	|NoiEnE	|ToneEnE|
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;	0x1D	| EPLA	|				Pitch increment value for envelope A			|
;	0x1E	| EPLB	|				Pitch increment value for envelope B			|
;	0x1F	| EPH	|EnvB PR| Envelope B octave num |EnvA PR| Envelope A octave num |
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;	0x20	| SPLA	|					Low byte of sample A pointer				|	
;	0x21	| SPLB	|					Low byte of sample B pointer				|
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;	0x22	| SPMA	|					Mid byte of sample A pointer				|
;	0x23	| SPMB	|					Mid byte of sample B pointer				|
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;	0x24	| SPHA	|					High byte of sample A pointer				|
;	0x25	| SPHA	|					High byte of sample B pointer				|
;					|=======|=======|=======|=======|=======|=======|=======|=======|

;	0x26	| SLLA	|					LENGTH
;	0x27	| SLMA	|
;	0x28	| SLLB	|
;	0x29	| SLMB	|
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;					|=======|=======|=======|=======|=======|=======|=======|=======|

; ENVELOPES????????? HOW????
;			|_______________________________________________________________|

.define OUTPUT_PB4

.include "./tn85def.inc"

; Internal configuration - DO NOT TOUCH
.if !defined(STEREO) || !defined(MONO)
	.if defined(OUTPUT_PB4) || defined(OUTPUT_DAC7311) || defined(OUTPUT_DAC6311) || defined(OUTPUT_DAC5311) || defined(OUTPUT_MCP4801) || defined(OUTPUT_MCP4811) || defined(OUTPUT_MCP4821)
		.define MONO 1
	.elseif defined (OUTPUT_DAC7612) || defined(OUTPUT_MCP4802) || defined(OUTPUT_MCP4812) || defined(OUTPUT_MCP4822)
		.define STEREO 1
	.endif
.elseif defined(OUTPUT_PB4) && defined(STEREO)
	.error "PB4 cannot be stereo (it's a single pin, you can't output multiple channels on one analog pin)"
.elseif defined(MONO) && defined(STEREO) && MONO && STEREO
	.error "Select stereo or mono output"
.endif

.if defined(OUTPUT_PB4) || defined(OUTPUT_DAC5311) || defined(OUTPUT_MCP4801)
	.define BITDEPTH 8
.elseif defined(OUTPUT_DAC6311) || defined(OUTPUT_MCP4811)
	.define BITDEPTH 10
.elseif defined(OUTPUT_DAC7311) || defined(OUTPUT_MCP4821)
	.define BITDEPTH 12
.else	
	.error "Unsupported DAC type"
.endif



.equ	USCK	= PB2
.equ	DO		= PB1
.equ	DI		= PB0

.equ	PWM_OUT	= PB4

; r0..4 are temp regs

.def	PHASE_ACC_A_LO	= r4
.def	PHASE_ACC_A_HI	= r5
.def	PHASE_ACC_B_LO	= r6
.def	PHASE_ACC_B_HI	= r7
.def	PHASE_ACC_C_LO	= r8
.def	PHASE_ACC_C_HI	= r9
.def	PHASE_ACC_D_LO	= r10
.def	PHASE_ACC_D_HI	= r11
.def	PHASE_ACC_E_LO	= r12
.def	PHASE_ACC_E_HI	= r13
.def	PHASE_ACC_N_LO	= r14
.def	PHASE_ACC_N_HI	= r15

.dseg

ShiftedCMPValues:	.byte 6
Increments:			.byte 6
NoiseLFSR:			.byte 2
NoiseXOR:			.byte 2


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
	clr r31
	clr r19
	clr r0
	clr r1
	clr r16
	out TCCR0B,	r16	; (0<<FOC0A)|(0<<FOC0B)|\	; No force strobe
					; (0<<WGM02)|\				; Total wavegen mode = 000 (normal)
					; (0<<CS00)					; No clock source
	out USISR,	r16	; (0<<USISIF)|(0<<USIOIF)|\	; No interrupts
					; (0<<USIPF)|\				; No stop condition
					; (0<<USICNT0)				; Counter at 0
	out TCCR0A,	r16	; (0<<COM0A0)|(0<<COM0B0)|\	; Normal port operation
					; (0<<WGM00)				; Total wavegen mode = 000 (normal)
	out TIFR,	r16	; Clear interrupts
	out PLLCSR,	r16	; Disable the PLL
	out USICR,	r16	; (0<<USISIE)|(0<<USIOIE)|\	; No interrupts
					; (0<<USIWM0)|\				; USI disabled
					; (0<<USICLK)|\				; No clock source
					; (0<<USITC)				; Don't toggle anything
	; Enable T1's PWM B, output the positive OCR1B,
	; Force no output compares,
	; Reset prescalers just in case
	ldi	r16,	(0<<TSM)|(1<<PWM1B)|(0b10<<COM1B0)|(0<<FOC1B)|(0<<FOC1A)|(1<<PSR1)|(1<<PSR0)
	out	GTCCR,	r16

	; Set timer 1 to count 256 cycles
	ldi r16,	(1<<TOIE1)	;	Enable overflow interrupt
	out TIMSK,	r16			;__
	; Don't reset T1 on CMP match with OCR1C,
	; Disable T1's PWM A, output nothing from OCR1A,
	; Enable T1 at the rate of CK
	ldi r16,	(0<<CTC1)|(0<<PWM1A)|(0b00<<COM1A0)|(1<<CS10)
	out TCCR1,	r16			;__
	; Set Port modes
	ldi r18,	(1<<PB3)|(1<<PWM_OUT)|(0<<DI)|(1<<DO)|(1<<USCK)
	out DDRB,	r18
	ldi r18,	(1<<PB3)|(1<<PWM_OUT)
	out PortB,	r18

	ldi r16,	(1<<CLKPCE)	;
	ldi	r17,	(0<<CLKPS0)	;	Set to 8MHz
	out	CLKPR,	r16			;
	out	CLKPR,	r17			;__

	ldi r26,	0xA5
	out USIDR,	r26
	ldi r27,	0x01

	movw PHASE_ACC_A_LO, r0
	; movw PHASE_ACC_B_LO, r0
	; movw PHASE_ACC_C_LO, r0
	; movw PHASE_ACC_D_LO, r0
	; movw PHASE_ACC_E_LO, r0
	; movw PHASE_ACC_N_LO, r0

	sts ShiftedCMPValues+0,	r27
	; all others
	sts Increments+0,	r0
	; all others




	sei
Forever:
	rjmp Forever

Cycle:
	out	OCR1B,	r26		; Update sound

	sbis PINB,	PINB0	; If no input pending, skip this
	rjmp PhaseAccumulatorUpdate

	cbi	PortB,	PB3

	; Copied directly from Microchip's docs

	mov	r18,	r26
	rcall	SPITransfer

	andi r18,	0x7F	;	Reg 0 has address
	mov r0,		r18		;__

	rcall 	SPITransfer_noOut

	mov	r1,		r18		;__	Reg 1 has data

	sbi PortB,	PB3		;	Latch the '595
	cbi	PortB, 	PB3		;__

	mov r16,	r0
	cpi	r16,	0x00
	brne CMP2
	sbi PortB,	PB3
	sts	Increments+0,	r1
	rjmp PhaseAccumulatorUpdate
CMP2:
	cpi	r16,	0x06
	brne PhaseAccumulatorUpdate
		ldi	ZH,		(ShiftedTable>>7)&0xFF
		mov	ZL,		r1
		andi ZL,	0x7
		subi ZL,	0x100-((ShiftedTable<<1)&0xFF)	
		lpm r2,		Z
		sts	ShiftedCMPValues+0,	r2
		bld	r1,		4
		brtc PhaseAccumulatorUpdate
			clr PHASE_ACC_A_LO
			clr	PHASE_ACC_A_HI
PhaseAccumulatorUpdate:
	clr	r3
	lds	r0,		Increments+0
	add	PHASE_ACC_A_LO,	r0
	adc	PHASE_ACC_A_HI,	r3
	lds	r0,		ShiftedCMPValues+0
	cp	PHASE_ACC_A_HI,	r0
	brlo RealEnd
		inc r26
		dec	r0
		and	PHASE_ACC_A_HI,	r0
RealEnd:
	reti

Multiply:	; 28 cycles  
	; needs 3 registers

	; Constant multiplier of 822,
	; which is 11 00110110 in binary,
	; is abused to shift less

	; Specifically, stuff is multiplied by 3,
	; as the bits are always in pairs -
	;	11 00110110
	;   \/   \/ \/
	;
	; Then shift a bunch and add together

	; input low: r16
	; input high: r17

	; output low: r2
	; output mid: r0
	; output high: r1
	; tmp 0:	  r3

	clr r3

	.if MONO
	; 1. Multiply by 3 : 6 cycles
	movw r0,	r16
	clc
	rol	r1
	rol r2
	add	r16,	r1
	adc	r17,	r2
	; r16:17 now contains the value multiplied by 3
	.endif

	movw r0,	r16	; High:Mid is now 3X, '' 00110110
	; clc	; Never occurs within valid range
	rol	r16
	rol	r17				
	mov	r2,		r16		;
	add	r0,		r17		;	.. 00110''0
	adc	r1,		r3		;__
	; Shift further by 3
	; clc 	; Cannot occur
	rol r16				;	Total shift: 2
	rol r17				;__
	rol r16				;	Total shift: 3
	rol r17				;__
	rol r16				;	Total shift: 4
	rol r17				;__
	add	r2,		r16		;
	adc	r0,		r17		;	.. 00''0..0
	adc	r1,		r3		;__

	.if BITDEPTH == 8
						; r1:r0 = 0XYZ
	swap r1				; 0XZY
	swap r0				; X0ZY
	ldi	r16,	$F0		; X0ZY
	and	r1,		r16		; X00Y
	or	r1,		r0		; XY--
	; BAM r0 now has the output value

	.elseif BITDEPTH > 8 && BITDEPTH <= 12
	.if defined(OUTPUT_DAC7311) || defined(OUTPUT_DAC6311) || defined(OUTPUT_DAC5311) || defined(OUTPUT_DAC7612)
	; TI DACs have a 2-bit prefix before the data
						; r1:r0 = 0000xxxx yyyyzzzz
	clc					;__
	rol	r1				; r1:r0 = 000xxxxy yyyzzzz0
	rol	r0				;__
	rol r1				; r1:r0	= 00xxxxyy yyzzzz00
	rol r0				;__
	; Bam the output is now correct
	.elseif defined(OUTPUT_MCP4801) || defined(OUTPUT_MCP4811) || defined(OUTPUT_MCP4821) || defined(OUTPUT_MCP4802) || defined(OUTPUT_MCP4812) || defined(OUTPUT_MCP4822)
	; Microchip DACs have a 4-bit prefix, but the control bits gotta be configured
	ldi	r16,	$30		; r1:r0	= 0011xxxx yyyyzzzz
	or	r1,		r16		;__		
	; Done, it's 1x and not shutting down
	.endif
	.endif



; Delay:
; 	clr	r30
; 	ldi r29,	0x00
; 	delay_loop:
; 		dec r30
; 		brne delay_loop

; 			dec r29
; 			brne delay_loop
	
; 	ret

SPITransfer:	; 24 cycles + 3 (RCALL)
	ldi	r17,	(1<<USIWM0)|(0<<USICS0)|(1<<USITC)|(1<<USICLK)
	ldi	r16,	(1<<USIWM0)|(0<<USICS0)|(1<<USITC)
SPITransfer_action:	; 22 cycles + 3 (RCALL)
	out	USIDR,	r18
SPITransfer_noOut:	; 21 cycles + 3 (RCALL)

	out	USICR,	r16 ; Rising clock edge					;	MSB
	out	USICR,	r17 ; Falling clock edge, shift data	;__
	out	USICR,	r16 ; Rising clock edge					;	6
	out	USICR,	r17 ; Falling clock edge, shift data	;__
	out	USICR,	r16 ; Rising clock edge					;	5
	out	USICR,	r17 ; Falling clock edge, shift data	;__
	out	USICR,	r16 ; Rising clock edge					;	4
	out	USICR,	r17 ; Falling clock edge, shift data	;__
	out	USICR,	r16 ; Rising clock edge					;	3
	out	USICR,	r17 ; Falling clock edge, shift data	;__
	out	USICR,	r16 ; Rising clock edge					;	2
	out	USICR,	r17 ; Falling clock edge, shift data	;__
	out	USICR,	r16 ; Rising clock edge					;	1
	out	USICR,	r17 ; Falling clock edge, shift data	;__
	out	USICR,	r16 ; Rising clock edge					;	LSB
	out	USICR,	r17 ; Falling clock edge, shift data	;__

	in	r18,	USIDR
ret

ShiftedTable:
.db 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
