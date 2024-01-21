;hello.asm
;  turns on an LED which is connected to PB5 (digital out 13)

.include "./tn85def.inc"

    ldi r16,0
    mov r1,r16
    mov r0,r16
	ldi r16,1<<PB2
    mov r17,r16
	out DDRB,r16
	out PortB,r16
Start:
    dec r1
    brne Start
    dec r0
    brne Start
    ; blink
    eor r16,r17
    out PortB,r16
	rjmp Start