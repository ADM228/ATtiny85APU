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
;	0x1n	| CFGX	|NoiseEn| EnvEn |Env/Smp| Slot# | Right volume	|  Left volume	|	n = 5..9, X = A..E
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;	0x1A	| ELLO	|			Low byte of envelope phase load value				|
;	0x1B	| ELHI	|			High byte of envelope phase load value				|
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;	0x1C	| ESHP	|EnvB PR|	Envelope B shape	|EnvA PR|	Envelope A shape	|
;					|=======|=======|=======|=======|=======|=======|=======|=======|
;	0x1D	| EPLA	|				Pitch increment value for envelope A			|
;	0x1E	| EPLB	|				Pitch increment value for envelope B			|
;	0x1F	| EPH	|		Envelope B octave num	|		Envelope A octave num	|
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

.def	EnvAVolume	= r20
.def	EnvBVolume	= r21
.def	SmpAVolume	= r22
.def	SmpBVolume	= r23

.def	NoiseMask	= r24
.def	EnvZeroFlg	= r25
.def	LOut_L		= r26
.def	LOut_H		= r27

.equ	EnvAZero	= 0
.equ	SmpAZero	= 1
.equ	EnvASlope	= 2
.equ	EnvBZero	= 4
.equ	SmpBZero	= 5
.equ	EnvBSlope	= 6

.dseg
NoiseLFSR:			.byte 2
EnvPhaseAccs:		.byte 4
SmpPhaseAccs:		.byte 4
EnvStates:			.byte 2
EnvShape:			.byte 1

DutyCycles:			.byte 5
NoiseXOR:			.byte 2
Volumes:			.byte 5
ChannelConfigs:		.byte 5
EnvLdBuffer:		.byte 2

Increments:			.byte 8
ShiftedIncrementsL:	.byte 8
ShiftedIncrementsH:	.byte 8
OctaveValues:		.byte 7

.equ PhaseAccEnvA_L	= EnvPhaseAccs+0
.equ PhaseAccEnvA_H	= EnvPhaseAccs+1
.equ PhaseAccEnvB_L	= EnvPhaseAccs+2
.equ PhaseAccEnvB_H	= EnvPhaseAccs+3

.equ PhaseAccSmpA_L	= SmpPhaseAccs+0
.equ PhaseAccSmpA_H	= SmpPhaseAccs+1
.equ PhaseAccSmpB_L	= SmpPhaseAccs+2
.equ PhaseAccSmpB_H	= SmpPhaseAccs+3

.equ EnvStateA		= EnvStates+0
.equ EnvStateB		= EnvStates+1

.equ DutyCycleA		= DutyCycles+0
.equ DutyCycleB		= DutyCycles+1
.equ DutyCycleC		= DutyCycles+2
.equ DutyCycleD		= DutyCycles+3
.equ DutyCycleE		= DutyCycles+4

.equ VolumeA	= Volumes+0
.equ VolumeB	= Volumes+1
.equ VolumeC	= Volumes+2
.equ VolumeD	= Volumes+3
.equ VolumeE	= Volumes+4

.equ ChannelConfigA	= ChannelConfigs+0
.equ ChannelConfigB	= ChannelConfigs+1
.equ ChannelConfigC	= ChannelConfigs+2
.equ ChannelConfigD	= ChannelConfigs+3
.equ ChannelConfigE	= ChannelConfigs+4

.equ IncrementA		= Increments+0
.equ IncrementB		= Increments+1
.equ IncrementC		= Increments+2
.equ IncrementD		= Increments+3
.equ IncrementE		= Increments+4
.equ IncrementN		= Increments+5
.equ IncrementEA	= Increments+6
.equ IncrementEB	= Increments+7

.equ ShiftedIncrementA_L	= ShiftedIncrementsL+0
.equ ShiftedIncrementB_L	= ShiftedIncrementsL+1
.equ ShiftedIncrementC_L	= ShiftedIncrementsL+2
.equ ShiftedIncrementD_L	= ShiftedIncrementsL+3
.equ ShiftedIncrementE_L	= ShiftedIncrementsL+4
.equ ShiftedIncrementN_L	= ShiftedIncrementsL+5
.equ ShiftedIncrementEA_L	= ShiftedIncrementsL+6
.equ ShiftedIncrementEB_L	= ShiftedIncrementsL+7

.equ ShiftedIncrementA_H	= ShiftedIncrementsH+0
.equ ShiftedIncrementB_H	= ShiftedIncrementsH+1
.equ ShiftedIncrementC_H	= ShiftedIncrementsH+2
.equ ShiftedIncrementD_H	= ShiftedIncrementsH+3
.equ ShiftedIncrementE_H	= ShiftedIncrementsH+4
.equ ShiftedIncrementN_H	= ShiftedIncrementsH+5
.equ ShiftedIncrementEA_H	= ShiftedIncrementsH+6
.equ ShiftedIncrementEB_H	= ShiftedIncrementsH+7

.equ OctaveValueA	= OctaveValues+0
.equ OctaveValueB	= OctaveValues+1
.equ OctaveValueC	= OctaveValues+2
.equ OctaveValueD	= OctaveValues+3
.equ OctaveValueE	= OctaveValues+4
.equ OctaveValueN	= OctaveValues+5
.equ OctaveValueEnv	= OctaveValues+6

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

	.equ ENVELOPE_LD_LO	= 0x1A
	.equ ENVELOPE_LD_HI	= 0x1B
	.equ ENVELOPE_SHAPE	= 0x1C

	.equ PITCH_LO_ENV_A	= 0x1D
	.equ PITCH_LO_ENV_B	= 0x1E
	.equ PITCH_HI_ENV	= 0x1F
	
	.equ PITCH_HI_AB_D	= PITCH_HI_AB*2	;
	.equ PITCH_HI_CD_D	= PITCH_HI_CD*2	;	D stands for Double offset
	.equ PITCH_HI_EN_D	= PITCH_HI_EN*2	;__

	.equ ENV_A_HOLD	= 0
	.equ ENV_A_ALT	= 1
	.equ ENV_A_ATT	= 2
	.equ ENV_A_RST	= 3
	.equ ENV_B_HOLD	= 4
	.equ ENV_B_ALT	= 5
	.equ ENV_B_ATT	= 6
	.equ ENV_B_RST	= 7

	; CFGX
	.equ ENV_EN		= 6
	;	0x1n	| CFGX	|NoiseEn| EnvEn |Env/Smp| Slot# | Right volume	|  Left volume	|	n = 5..9, X = A..E
	.equ ENV_SMP	= 5
	.equ SLOT_NUM	= 4


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
	; Set timer 0 freq to max for SPI + SPI settings
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
	; Enable T1 at the rate of CK (256 cycles)
	ldi r16,	(0<<CTC1)|(0<<PWM1A)|(0b00<<COM1A0)|(1<<CS10)
	out TCCR1,	r16			;__
	; Set Port modes
	ldi r16,	(1<<PB3)|(1<<PWM_OUT)|(0<<DI)|(1<<DO)|(1<<USCK)
	out DDRB,	r16
	ldi r16,	(1<<PB3)|(1<<PWM_OUT)
	out PortB,	r16

	ldi r16,	(1<<CLKPCE)	;
	ldi	r17,	(0<<CLKPS0)	;	Set to 8MHz
	out	CLKPR,	r16			;
	out	CLKPR,	r17			;__

	ldi r16,	0xA5
	out USIDR,	r16

	movw PhaseAccA_L, 	r0
	movw PhaseAccB_L, 	r0
	movw PhaseAccC_L, 	r0
	movw PhaseAccD_L, 	r0
	movw PhaseAccE_L, 	r0
	movw PhaseAccN_L, 	r0
	movw EnvAVolume,	r0
	movw SmpAVolume,	r0

	ldi r16,	0x5F
	ldi	r17,	0x0F

	ldi YL,		0x67
	clr YH

	ClearOscLoop:
		std Y+Increments-RamOff,		r0
		std Y+ShiftedIncrementsL-RamOff,r0
		std Y+ShiftedIncrementsH-RamOff,r0
		std Y+OctaveValues-RamOff,		r0
		dec YL
		cpse YL,	r16
		rjmp ClearOscLoop
	
	ldi YL,		0x64

	Clear5Loop:
		std Y+DutyCycles-RamOff,	r0
		std Y+Volumes-RamOff,		r0
		std	Y+ChannelConfigs-RamOff,r17
		dec	YL
		cpse YL,	r16
		rjmp Clear5Loop


	ldi YL,		0x61
	
	ClearEnvLoop:
		std	Y+EnvPhaseAccs-RamOff,		r0
		std	Y+EnvPhaseAccs+2-RamOff,	r0
		std	Y+SmpPhaseAccs-RamOff,		r0
		std	Y+SmpPhaseAccs+2-RamOff,	r0
		std Y+EnvStates-RamOff,			r0
		std Y+EnvLdBuffer-RamOff,		r0
		dec	YL
		cpse YL,	r16
		rjmp ClearEnvLoop

	sts	NoiseLFSR+0,	r0
	sts	NoiseLFSR+1,	r0
	ldi	r16,	0x24
	sts	NoiseXOR+0,		r0
	sts	NoiseXOR+1,		r16
	ldi NoiseMask,		0x7F

	sts EnvShape,		r0
	ldi	EnvZeroFlg,		1<<EnvAZero|1<<EnvBZero|1<<SmpAZero|1<<SmpBZero

	sei
Forever:
	rjmp Forever

Cycle:
	cbi	PortB,	PB3

	sbis PINB,	PINB0	; If no input pending, skip this
	rjmp AfterSPI

	rcall	SPITransfer

	andi r18,	0x7F	;
	cpi	r18,	(CallTableEnd-CallTable)
	brlo L000			;	Get reg number
	ldi	r18,	(CallTableEnd-CallTable-1)
	L000:				;__
	ldi	YL,		RAMOff	;
	add	YL,		r18		;	Get reg number into Y
	clr	YH				;__

	ldi	ZH,		high(CallTable)
	ldi	ZL,		low(CallTable)	
	add ZL,		r18		;	Get IJMP pointer into Z
	adc	ZH,		YH		;__

	out	USIDR,	ZL
	rcall 	SPITransfer_noOut

	mov	r1,		r18		;__	Reg 1 has data

	ijmp

AfterSPI:
	sbi PortB,	PB3
	clr LOut_L
	clr LOut_H
PhaseAccEnvUpd:
	; UP TO 46 CYCLES AAAAAAAAAAAAAAAAAAAA
	; lds env octave (only once)
	lds	r18,	OctaveValueEnv
	lds	r19,	EnvShape
	; skip if inc == 0
	bst	EnvZeroFlg,	EnvAZero
	brts PhaseAccEnvAUpd_Skip
	PhaseAccEnvAUpd:
		lds	r0,		ShiftedIncrementEA_L
		; if octave MSB set, add to high and mid bytes
		clr	r3
		bst	r18,	3
		brtc PhaseAccEnvAUpd_MidLo
	PhaseAccEnvAUpd_MidHi:
		lds	r1,		PhaseAccEnvA_H
		add	r1,		r0
		sts	PhaseAccEnvA_H,	r1
		lds	r0,		ShiftedIncrementEA_H
		adc	r3,		r0
		rjmp PhaseAccEnvAUpd_ValueUpdate
	PhaseAccEnvAUpd_MidLo:
		; else add to mid and low bytes
		lds	r1,		PhaseAccEnvA_L
		add	r1,		r0
		sts	PhaseAccEnvA_L,	r1
		lds	r0,		ShiftedIncrementEA_H
		lds	r1,		PhaseAccEnvA_H
		adc	r1,		r0
		sts	PhaseAccEnvA_H,	r1
		adc	r3,		r3	; r3 is 0, therefore r3 becomes carry
	PhaseAccEnvAUpd_ValueUpdate:
		breq PhaseAccEnvAUpd_Skip	; Z flag set by the last adc with r3
		; inc env pos by high byte
		lds	EnvAVolume,	EnvStateA
		add	EnvAVolume,	r3
		sts	EnvStateA,	EnvAVolume	; doesn't affect carry
		brcc PhaseAccEnvAUpd_End
	PhaseAccEnvAUpd_Overflow:
		bst r19,	ENV_A_ALT
		brtc L00F
			ldi	r16,	1<<EnvASlope	;	Invert slope
			eor	EnvZeroFlg,	r16			;__
		L00F:
		bst r19,	ENV_A_HOLD
		brtc PhaseAccEnvAUpd_End
			ser	EnvAVolume
			sbr	EnvZeroFlg,	1<<EnvAZero
	PhaseAccEnvAUpd_End:
		sbrs EnvZeroFlg, EnvASlope
			com	EnvAVolume
	PhaseAccEnvAUpd_Skip:
	bst	EnvZeroFlg,	EnvBZero
	brts PhaseAccEnvBUpd_End
	PhaseAccEnvBUpd:
		lds	r0,		ShiftedIncrementEB_L
		; if octave MSB set, add to high and mid bytes
		clr	r3
		bst	r18,	7
		brtc PhaseAccEnvBUpd_MidLo
	PhaseAccEnvBUpd_MidHi:
		lds	r1,		PhaseAccEnvB_H
		add	r1,		r0
		sts	PhaseAccEnvB_H,	r1
		lds	r0,		ShiftedIncrementEB_H
		adc	r3,		r0
		rjmp PhaseAccEnvBUpd_ValueUpdate
	PhaseAccEnvBUpd_MidLo:
		; else add to mid and low bytes
		lds	r1,		PhaseAccEnvB_L
		add	r1,		r0
		sts	PhaseAccEnvB_L,	r1
		lds	r0,		ShiftedIncrementEB_H
		lds	r1,		PhaseAccEnvB_H
		adc	r1,		r0
		sts	PhaseAccEnvB_H,	r1
		adc	r3,		r3	; r3 is 0, therefore r3 becomes carry
	PhaseAccEnvBUpd_ValueUpdate:
		breq PhaseAccEnvBUpd_End	; Z flag set by the last adc with r3
		; inc env pos by high byte
		lds	EnvBVolume,	EnvStateA
		add	EnvBVolume,	r3
		sts	EnvStateA,	EnvBVolume	; doesn't affect carry
		brcc PhaseAccEnvBUpd_End
	PhaseAccEnvBUpd_Overflow:
		bst r19,	ENV_B_ALT
		brtc L010
			ldi	r16,	1<<EnvBSlope	;	Invert slope
			eor	EnvZeroFlg,	r16			;__
		L010:
		bst r19,	ENV_B_HOLD
		brtc PhaseAccEnvBUpd_End
			ser	EnvBVolume
			sbr	EnvZeroFlg,	1<<EnvBZero
	PhaseAccEnvBUpd_End:
		sbrs EnvZeroFlg, EnvBSlope
			com	EnvBVolume
PhaseAccNoiseUpd:
	lds	r0,		ShiftedIncrementN_L	;
	add PhaseAccN_L,	r0			;	PhaseAcc += shifted inc value
	lds	r0,		ShiftedIncrementN_H	;
	adc	PhaseAccN_H,	r0			;__

	brcc PhaseAccChAUpd
		ldi	NoiseMask,	0x7F
		lds	r0,		NoiseLFSR+0		;
		lds r1,		NoiseLFSR+1		;
		lsr	r1						;	Shift LFSR
		ror	r0						;__
		brcs NoiseLFSRBack			;
			lds	r2,		NoiseXOR+0	;
			eor	r0,		r2			;	XOR if needed
			lds	r2,		NoiseXOR+1	;
			eor	r1,		r2			;__
			ser	NoiseMask			;__	Update noise mask
		NoiseLFSRBack:
		sts	NoiseLFSR+0,	r0		;	Update the LFSR
		sts NoiseLFSR+1,	r1		;__

PhaseAccChAUpd:
	lds	r16,	ChannelConfigA		;	Get noise mask
	and	r16,	NoiseMask			;__

	lds	r0,		ShiftedIncrementA_L	;
	add	PhaseAccA_L,	r0			;	PhaseAcc += shifted inc value
	lds r0,		ShiftedIncrementA_H	;
	adc	PhaseAccA_H,	r0			;__

	lds	r0,		DutyCycleA			;
	cp	PhaseAccA_H,	r0			;	If > duty cycle, then OR it with the noise
	brsh L01C						;__
		sbr	r16,	0x80			;__
	L01C:							;
	clr	r0							;
	sbrs r16,	7					;	Output 0 if nothing is output
	rjmp Multiply					;__
		lds	r0,		VolumeA			;
		sbrs r16,	ENV_EN			;	If envelope/sample disabled, just put the volume
		rjmp L01D					;__
			mov	YL,		r16			;
			swap YL					;	Load the envelope/sample volume
			andi YL,	0x03		;
			ldd	r2,	Y+20			;__
			sbrs r0,	7			;	Shift once if volume is low
				lsr	r2				;__
			mov	r0,		r2			;
		L01D:						;
		sbrs r16,	0				;	Store the upper bit of volume
		rjmp L01E					;
			add	LOut_L,	r0			;
			adc	LOut_H,	YH			;__
		L01E:						;
		sbrs r16,	1				;
		rjmp Multiply				;
			lsl	r0					;	Store the lower bit of volume
			adc	LOut_H,	YH			;
			add	LOut_L,	r0			;
			adc	LOut_H,	YH			;__
			

Multiply:	; 27 cycles  
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

	movw r16,	LOut_L

	movw r0,	r16	; High:Mid is now 3X, '' 00110110
	lsl	r16
	rol	r17
	mov	r2,		r16		;
	add	r0,		r17		;	.. 00110''0
	adc	r1,		YH		;__
	; Shift further by 3
	lsl r16				;	Total shift: 2
	rol r17				;__
	lsl r16				;	Total shift: 3
	rol r17				;__
	lsl r16				;	Total shift: 4
	rol r17				;__
	add	r2,		r16		;
	adc	r0,		r17		;	.. 00''0..0
	adc	r1,		YH		;__

	.if BITDEPTH == 8
						; r0	r1		total
						; YZ	0X		0XYZ
	swap r1				; YZ	X0		X0YZ
	swap r0				; ZY	X0		X0ZY
	ldi	r16,	$0F		; 
	and	r0,		r16		; 0Y	X0		X00Y
	or	r0,		r1		; XY	X0		--XY
	; BAM r0 now has the output value

	.elseif BITDEPTH > 8 && BITDEPTH <= 12
	.if defined(OUTPUT_DAC7311) || defined(OUTPUT_DAC6311) || defined(OUTPUT_DAC5311) || defined(OUTPUT_DAC7612)
	; TI DACs have a 2-bit prefix before the data
						;__	r1:r0 = 0000xxxx yyyyzzzz
	lsl	r1				;	r1:r0 = 000xxxxy yyyzzzz0
	rol	r0				;__
	lsl r1				;	r1:r0	= 00xxxxyy yyzzzz00
	rol r0				;__
	; Bam the output is now correct
	.elseif defined(OUTPUT_MCP4801) || defined(OUTPUT_MCP4811) || defined(OUTPUT_MCP4821) || defined(OUTPUT_MCP4802) || defined(OUTPUT_MCP4812) || defined(OUTPUT_MCP4822)
	; Microchip DACs have a 4-bit prefix, but the control bits gotta be configured
	ldi	r16,	$30		; r1:r0	= 0011xxxx yyyyzzzz
	or	r1,		r16		;__		
	; Done, it's 1x and not shutting down
	.endif
	.endif

RealEnd:
	out	OCR1B,	r0
	in	r0,		TIFR
	bst	r0,		TOV1
	brtc Delay
	ldi r16,	1<<TOV1	;	Acknowledge second interrupt
	out TIFR,	r16		;__
	reti
Delay:
	in	r0,		TCNT1
	com	r0
	lsr r0
	breq RealEnd
	L00B:
		dec	r0
		brne L00B
	rjmp RealEnd


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
EPLA_RegHndl:
	ldi	YL,		6+RAMOff
PILOX_RegHndl:
	ldd r2,	Y+OctaveValueA-RAMOff-REG_OFF
	;	r1 = hi, r0 = lo, shifting right
	L01B:
	std	Y+IncrementA-RAMOff-REG_OFF,	r1
	clr	r0
	sbrc r2,	2	;
	rjmp L00C		;
		lsr r1		;
		ror r0		;
		lsr r1		;	Shift increment 4 times if needed
		ror r0		;
		lsr r1		;
		ror r0		;
		lsr r1		;
		ror r0		;
	L00C:			;__
	sbrc r2,	1	;
	rjmp L00D		;
		lsr r1		;
		ror r0		;	Shift increment 2 times if needed
		lsr r1		;
		ror r0		;__
	L00D:			;
	sbrc r2,	0	;
	rjmp L00E		;	Shift increment once more if needed
		lsr r1		;	
		ror r0		;__
	L00E:			;	Store shifted increment
	std	Y+ShiftedIncrementA_H-RAMOff-REG_OFF,	r1
	std	Y+ShiftedIncrementA_L-RAMOff-REG_OFF,	r0
	rjmp AfterSPI

.set REG_OFF	= PITCH_HI_AB_D
PHIXY_RegHndl:
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
		sbrc r1,	2	;
		rjmp L003		;
			lsr r2		;
			ror r0		;
			lsr r2		;	Shift increment 4 times if needed
			ror r0		;
			lsr r2		;
			ror r0		;
			lsr r2		;
			ror r0		;
		L003:			;__
		sbrc r1,	1	;
		rjmp L004		;
			lsr r2		;
			ror r0		;	Shift increment 2 times if needed
			lsr r2		;
			ror r0		;__
		L004:			;
		sbrc r1,	0	;
		rjmp L005		;	Shift increment once more if needed
			lsr r2		;	
			ror r0		;__
		L005:			;	Store shifted increment
		std	Y+ShiftedIncrementA_H-RAMOff-REG_OFF,	r2
		std	Y+ShiftedIncrementA_L-RAMOff-REG_OFF,	r0
		
	L001:
	mov	ZL,		r1
	swap ZL
	andi ZL,	0x7
	ldd	r2,		Y+OctaveValueB-RAMOff-REG_OFF
	cp	r2,		ZL
	breq L002
		std	Y+OctaveValueB-RAMOff-REG_OFF, ZL			;__	Store octave

		ldd	r2,		Y+IncrementB-RAMOff-REG_OFF	;__	Get raw increment
		;	r2 = high, r0 = low, shifting right
		clr	r0
		sbrc r1,	6	;
		rjmp L006		;
			lsr r2		;
			ror r0		;
			lsr r2		;	Shift increment 4 times if needed
			ror r0		;
			lsr r2		;
			ror r0		;
			lsr r2		;
			ror r0		;
		L006:			;__
		sbrc r1,	5	;
		rjmp L007		;
			lsr r2		;
			ror r0		;	Shift increment 2 times if needed
			lsr r2		;
			ror r0		;__
		L007:			;
		sbrc r1,	4	;
		rjmp L008		;	Shift increment once more if needed
			lsr r2		;	
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


.set REG_OFF	= DUTY_A
DUTYX_RegHndl:
LFSR_RegHndl:
VOLX_RegHndl:
CFGX_RegHndl:
ELXX_RegHndl:
	std Y+DutyCycles-RAMOff-REG_OFF,	r1
	rjmp AfterSPI

ESHP_RegHndl:
	lds	r16,	EnvShape
	lds	r2,		EnvLdBuffer+0
	lds	r3,		EnvLdBuffer+1

	eor r16,	r1
	andi r16,	1<<ENV_A_ATT|1<<ENV_B_ATT
	eor	EnvZeroFlg,	r16

	bst	r1,		ENV_A_RST
	brtc L011
		sts	EnvStateA,	r3
		sts	PhaseAccEnvA_L,	r0
		sts	PhaseAccEnvA_H,	r2
		cbr	EnvZeroFlg,	1<<EnvAZero
		bst	r1,		ENV_A_ATT
		bld	EnvZeroFlg,	EnvASlope
	L011:
	bst	r1,		ENV_B_RST
	brtc L012
		sts	EnvStateB,	r3
		sts	PhaseAccEnvB_L,	r0
		sts	PhaseAccEnvB_H,	r2
		cbr	EnvZeroFlg,	1<<EnvBZero
		bst	r1,		ENV_B_ATT
		bld	EnvZeroFlg,	EnvBSlope
	L012:
	sts	EnvShape,	r1
	rjmp AfterSPI

EPLB_RegHndl:
	ldi	YL,		7+RAMOff
	lds	r2,		OctaveValueEnv
	swap r2
	rjmp L01B

EPH_RegHndl:
	lds	ZL,		OctaveValueEnv	
	mov	ZH,		ZL
	eor	ZL,		r1
	andi ZL,	0x7
	breq L013	; If octave not changed, do nothing
		lds	r2,		IncrementEA	;__	Get raw increment
		;	r2 = high, r0 = low, shifting right
		clr	r0
		sbrc r1,	2	;
		rjmp L014		;
			lsr r2		;
			ror r0		;
			lsr r2		;	Shift increment 4 times if needed
			ror r0		;
			lsr r2		;
			ror r0		;
			lsr r2		;
			ror r0		;
		L014:			;__
		sbrc r1,	1	;
		rjmp L015		;
			ror r2		;
			ror r0		;	Shift increment 2 times if needed
			ror r2		;
			ror r0		;__
		L015:			;
		sbrc r1,	0	;
		rjmp L016		;	Shift increment once more if needed
			ror r2		;	
			ror r0		;__
		L016:			;	Store shifted increment
		sts	ShiftedIncrementEA_H,	r2
		sts ShiftedIncrementEA_L,	r0
		
	L013:
	eor	ZH,		r1
	andi ZH,	0x70
	breq L017
		lds	r2,		IncrementEB	;__	Get raw increment
		;	r2 = high, r0 = low, shifting right
		clr	r0
		sbrc r1,	6	;
		rjmp L018		;
			lsr r2		;
			ror r0		;
			lsr r2		;	Shift increment 4 times if needed
			ror r0		;
			lsr r2		;
			ror r0		;
			lsr r2		;
			ror r0		;
		L018:			;__
		sbrc r1,	5	;
		rjmp L019		;
			lsr r2		;
			ror r0		;	Shift increment 2 times if needed
			lsr r2		;
			ror r0		;__
		L019:			;
		sbrc r1,	4	;
		rjmp L01A		;	Shift increment once more if needed
			lsr r2		;	
			ror r0		;__
		L01A:			;	Store shifted increment
		sts	ShiftedIncrementEB_H,	r2
		sts	ShiftedIncrementEB_L,	r0
	L017:
	sts	OctaveValueEnv,	r1
	
Dummy_RegHndl:
	rjmp AfterSPI

CallTable:
	; 0x00..05		(Pitch Lo X)
	rjmp PILOX_RegHndl
	rjmp PILOX_RegHndl
	rjmp PILOX_RegHndl
	rjmp PILOX_RegHndl
	rjmp PILOX_RegHndl
	rjmp PILOX_RegHndl
	; 0x06..08		(Pitch Hi XY)
	rjmp PHIXY_RegHndl
	rjmp PHIXY_RegHndl
	rjmp PHIXY_RegHndl
	; 0x09..0D		(Duty Cycle X)
	rjmp DUTYX_RegHndl
	rjmp DUTYX_RegHndl
	rjmp DUTYX_RegHndl
	rjmp DUTYX_RegHndl
	rjmp DUTYX_RegHndl
	rjmp LFSR_RegHndl
	rjmp LFSR_RegHndl
	; 0x10..15
	rjmp VOLX_RegHndl
	rjmp VOLX_RegHndl
	rjmp VOLX_RegHndl
	rjmp VOLX_RegHndl
	rjmp VOLX_RegHndl
	; 0x16..19
	rjmp CFGX_RegHndl
	rjmp CFGX_RegHndl
	rjmp CFGX_RegHndl
	rjmp CFGX_RegHndl
	rjmp CFGX_RegHndl
	; 0x1A..1B
	rjmp ELXX_RegHndl
	rjmp ELXX_RegHndl
	; 0x1C
	rjmp ESHP_RegHndl
	; 0x1D..1E
	rjmp EPLA_RegHndl
	rjmp EPLB_RegHndl
	; 0x1F
	rjmp EPH_RegHndl
	; other
	rjmp Dummy_RegHndl
CallTableEnd: