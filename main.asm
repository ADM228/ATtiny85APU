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
;	0x1n	| VOLX	|						Channel X volume						|	n = 0..4, X = A..E
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;	0x1n	| CFGX	|NoiseEn|EnvEn	|Env/Smp| Slot# | Right volume	|  Left volume	|	n = 5..9, X = A..E
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;						envelope load values????
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

.def	PhaseAccA_L	= r4
.def	PhaseAccA_H	= r5
.def	PhaseAccB_L	= r6
.def	PhaseAccB_H	= r7
.def	PhaseAccC_L	= r8
.def	PhaseAccC_H	= r9
.def	PhaseAccD_L	= r10
.def	PhaseAccD_H	= r11
.def	PhaseAccE_L	= r12
.def	PhaseAccE_H	= r13
.def	PhaseAccN_L	= r14
.def	PhaseAccN_H	= r15

.dseg
NoiseLFSR:			.byte 2

NoiseXOR:			.byte 2
DutyCycles:			.byte 5
Volumes:			.byte 5

Increments:			.byte 6
ShiftedIncrementsL:	.byte 6
ShiftedIncrementsH:	.byte 6
OctaveValues:		.byte 6

.equ OctaveValueA	= OctaveValues+0
.equ OctaveValueB	= OctaveValues+1
.equ OctaveValueC	= OctaveValues+2
.equ OctaveValueD	= OctaveValues+3
.equ OctaveValueE	= OctaveValues+4
.equ OctaveValueN	= OctaveValues+5

.equ DutyCycleA		= DutyCycles+0
.equ DutyCycleB		= DutyCycles+1
.equ DutyCycleC		= DutyCycles+2
.equ DutyCycleD		= DutyCycles+3
.equ DutyCycleE		= DutyCycles+4

.equ IncrementA		= Increments+0
.equ IncrementB		= Increments+1
.equ IncrementC		= Increments+2
.equ IncrementD		= Increments+3
.equ IncrementE		= Increments+4
.equ IncrementN		= Increments+5

.equ ShiftedIncrementA_L	= ShiftedIncrementsL+0
.equ ShiftedIncrementB_L	= ShiftedIncrementsL+1
.equ ShiftedIncrementC_L	= ShiftedIncrementsL+2
.equ ShiftedIncrementD_L	= ShiftedIncrementsL+3
.equ ShiftedIncrementE_L	= ShiftedIncrementsL+4
.equ ShiftedIncrementN_L	= ShiftedIncrementsL+5

.equ ShiftedIncrementA_H	= ShiftedIncrementsH+0
.equ ShiftedIncrementB_H	= ShiftedIncrementsH+1
.equ ShiftedIncrementC_H	= ShiftedIncrementsH+2
.equ ShiftedIncrementD_H	= ShiftedIncrementsH+3
.equ ShiftedIncrementE_H	= ShiftedIncrementsH+4
.equ ShiftedIncrementN_H	= ShiftedIncrementsH+5

.equ VolumeA	= Volumes+0
.equ VolumeB	= Volumes+1
.equ VolumeC	= Volumes+2
.equ VolumeD	= Volumes+3
.equ VolumeE	= Volumes+4

.equ RAMOff		= 0x60

Registers:

	.equ PITCH_LO_A		= 0x00
	.equ PITCH_LO_B		= 0x01
	.equ PITCH_LO_C		= 0x02
	.equ PITCH_LO_D		= 0x03
	.equ PITCH_LO_E		= 0x04
	.equ PITCH_LO_N		= 0x05

	.equ PITCH_HI_AB	= 0x06
	.equ PITCH_HI_CD	= 0x07
	.equ PITCH_HI_EN	= 0x08

	.equ DUTY_A			= 0x09
	.equ DUTY_B			= 0x0A
	.equ DUTY_C			= 0x0B
	.equ DUTY_D			= 0x0C
	.equ DUTY_E			= 0x0D

	.equ NOISE_TAP_LO	= 0x0E
	.equ NOISE_TAP_HI	= 0x0F

	.equ VOLUME_A		= 0x10
	.equ VOLUME_B		= 0x11
	.equ VOLUME_C		= 0x12
	.equ VOLUME_D		= 0x13
	.equ VOLUME_E		= 0x14

	.equ CONFIG_A		= 0x15
	.equ CONFIG_B		= 0x16
	.equ CONFIG_C		= 0x17
	.equ CONFIG_D		= 0x18
	.equ CONFIG_E		= 0x19
	
	.equ PITCH_HI_AB_D	= PITCH_HI_AB*2	;
	.equ PITCH_HI_CD_D	= PITCH_HI_CD*2	;	D stands for Double offset
	.equ PITCH_HI_EN_D	= PITCH_HI_EN*2	;__


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
	clr r3
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
	; Enable T1 at the rate of CK/2	(512 cycles)
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

	movw PhaseAccA_L, r0
	; movw PhaseAccB_L, r0
	; movw PhaseAccC_L, r0
	; movw PhaseAccD_L, r0
	; movw PhaseAccE_L, r0
	; movw PhaseAccN_L, r0

	; all others
	ldi	r27,	0x04
	sts OctaveValueA,		r27

	ldi r27,	0xFF
	sts	Volumes+0,			r27

	ldi r27,	0x80
	sts	DutyCycleA,			r27

	sts Increments+0,	r27
	; all others




	sei
Forever:
	rjmp Forever

Cycle:
	out	OCR1B,	r26		; Update sound

	sbis PINB,	PINB0	; If no input pending, skip this
	rjmp AfterSPI

	clr	r3

	cbi	PortB,	PB3

	; Copied directly from Microchip's docs

	mov	r18,	r26
	rcall	SPITransfer


	andi r18,	0x7F	;
	cpi	r18,	0x16	;
	brlo L000			;	Get reg number
	ldi	r18,	0x10	;
	L000:				;__
	ldi	YL,		RAMOff	;
	add	YL,		r18		;	Get reg number into Y
	clr	YH				;__

	ldi	ZH,		(CallTable>>7)&0xFF
	ldi	ZL,		((CallTable<<1)&0xFF)	
	add ZL,		r18		;
	adc	ZH,		YH		;
	add	ZL,		r18		;
	adc	ZH,		YH		;
	lpm	r0,		Z+		;	Get IJMP pointer into Z
	lpm	r1,		Z+		;
	movw ZL,	r0		;__

	out	USIDR,	ZL
	rcall 	SPITransfer_noOut

	mov	r1,		r18		;__	Reg 1 has data

	sbi PortB,	PB3		;	Latch the '595
	cbi	PortB, 	PB3		;__

	ijmp

AfterSPI:
	clr r26
	clr	r3
PhaseAccChAUpd:
	lds	r0,		ShiftedIncrementA_L	;
	add	PhaseAccA_L,	r0			;	PhaseAcc += shifted inc value
	lds r0,		ShiftedIncrementA_H	;
	adc	PhaseAccA_H,	r0			;__

	lds	r0,		DutyCycleA			;
	cp	PhaseAccA_H,	r0			;	If > duty cycle, then make it full volume
	brsh RealEnd					;__
		lds	r0,		VolumeA
		mov r26,	r0

	; todo new alg:
	; 1. Get noise enable in tmp reg
	; 2. Mask to correct channel
	; 3. brsh
	;	4. Set flag in tmp reg
	; 5. breq
	;	6. lds volume
RealEnd:
	in	r0,		TIFR
	sbrs r0,	TOV1
	reti
Delay:
	in	r0,		TCNT1
	com	r0
	L00B:
		dec	r0
		brne L00B
	rjmp RealEnd

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

.set REG_OFF	= PITCH_LO_A
PILOX_Routine:
	std	Y+IncrementA-RAMOff-REG_OFF,	r1
	ldd r2,	Y+OctaveValueA-RAMOff-REG_OFF
	;	r1 = hi, r0 = lo, shifting right
	clr	r0
	clc					
	sbrc r2,	2	;
	rjmp L00C		;
		ror r1		;
		ror r0		;
		ror r1		;	Shift increment 4 times if needed
		ror r0		;
		ror r1		;
		ror r0		;
		ror r1		;
		ror r0		;
	L00C:			;__
	sbrc r2,	1	;
	rjmp L00D		;
		ror r1		;
		ror r0		;	Shift increment 2 times if needed
		ror r1		;
		ror r0		;__
	L00D:			;
	sbrc r2,	0	;
	rjmp L00E		;	Shift increment once more if needed
		ror r1		;	
		ror r0		;__
	L00E:			;	Store shifted increment
	std	Y+ShiftedIncrementA_H-RAMOff-REG_OFF,	r1
	std	Y+ShiftedIncrementA_L-RAMOff-REG_OFF,	r0
	rjmp AfterSPI

.set REG_OFF	= DUTY_A
DUTYX_Routine:
LFSR_Routine:
	subi YL,	REG_OFF
	std Y+DutyCycles-RAMOff,	r1
	rjmp AfterSPI

.set REG_OFF	= PITCH_HI_AB_D
PHIXY_Routine:
	; Y has 0x06
	lsl	YL
	subi YL,	RAMOff
	; Y now has 0x0C, 0x0E or 0x10, very handy
	mov	ZL,		r1
	andi ZL,	0x7
	ldd	r2,		Y+OctaveValueA-RAMOff-REG_OFF
	cp	r2,		ZL
	breq L001	; If octave not changed, do nothing
		std	Y+OctaveValueA-RAMOff-REG_OFF, ZL	;__	Store octave

		ldd	r2,		Y+IncrementA-RAMOff-REG_OFF	;__	Get raw increment
		;	r2 = high, r0 = low, shifting right
		clr	r0
		clc					
		sbrc r1,	2	;
		rjmp L003		;
			ror r2		;
			ror r0		;
			ror r2		;	Shift increment 4 times if needed
			ror r0		;
			ror r2		;
			ror r0		;
			ror r2		;
			ror r0		;
		L003:			;__
		sbrc r1,	1	;
		rjmp L004		;
			ror r2		;
			ror r0		;	Shift increment 2 times if needed
			ror r2		;
			ror r0		;__
		L004:			;
		sbrc r1,	0	;
		rjmp L005		;	Shift increment once more if needed
			ror r2		;	
			ror r0		;__
		L005:			;	Store shifted increment
		std	Y+ShiftedIncrementA_H-RAMOff-REG_OFF,	r2
		std	Y+ShiftedIncrementA_L-RAMOff-REG_OFF,	r0
		
	L001:
	mov	ZL,		r1
	andi ZL,	0x7
	ldd	r2,		Y+OctaveValueB-RAMOff-REG_OFF
	cp	r2,		ZL
	breq L002
		std	Y+OctaveValueB-RAMOff-REG_OFF, ZL			;__	Store octave

		ldd	r2,		Y+IncrementB-RAMOff-REG_OFF	;__	Get raw increment
		;	r2 = high, r0 = low, shifting right
		clr	r0
		clc	
		sbrc r1,	6	;
		rjmp L006		;
			ror r2		;
			ror r0		;
			ror r2		;	Shift increment 4 times if needed
			ror r0		;
			ror r2		;
			ror r0		;
			ror r2		;
			ror r0		;
		L006:			;__
		sbrc r1,	5	;
		rjmp L007		;
			ror r2		;
			ror r0		;	Shift increment 2 times if needed
			ror r2		;
			ror r0		;__
		L007:			;
		sbrc r1,	4	;
		rjmp L008		;	Shift increment once more if needed
			ror r2		;	
			ror r0		;__
		L008:			;	Store shifted increment
		std	Y+ShiftedIncrementB_H-RAMOff-REG_OFF,	r2
		std	Y+ShiftedIncrementB_L-RAMOff-REG_OFF,	r0

	L002:				;
	sbrs r1,	3		;
	rjmp L009			;	Reset PhaseAcc A if told so
		clr PhaseAccA_L	;	TODO indexing in some way
		clr	PhaseAccA_H	;__
	L009:				;
	sbrs r1,	7		;
	rjmp L00A			;	Reset PhaseAcc B if told so
		clr PhaseAccB_L	;	TODO indexing in some way
		clr	PhaseAccB_H	;__
	L00A:
	rjmp AfterSPI

.set REG_OFF	= VOLUME_A

VOLX_Routine:
	subi YL,	REG_OFF
	std Y+Volumes-RAMOff,	r1
	rjmp AfterSPI

Dummy_Routine:
CFGX_Routine:
	rjmp AfterSPI

CallTable:
	; 0x00..05		(Pitch Lo X)
	.dw PILOX_Routine, PILOX_Routine, PILOX_Routine, PILOX_Routine, PILOX_Routine, PILOX_Routine
	; 0x06..08		(Pitch Hi XY)
	.dw PHIXY_Routine, PHIXY_Routine, PHIXY_Routine
	; 0x09..0D		(Duty Cycle X)
	.dw DUTYX_Routine, DUTYX_Routine, DUTYX_Routine, DUTYX_Routine, DUTYX_Routine
	.dw LFSR_Routine, LFSR_Routine
	; 0x10..15
	.dw VOLX_Routine, VOLX_Routine, VOLX_Routine, VOLX_Routine, VOLX_Routine
	; 0x16..19
	.dw CFGX_Routine, CFGX_Routine, CFGX_Routine, CFGX_Routine, CFGX_Routine
