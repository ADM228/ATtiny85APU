
# Only intended for linux.
build: main.hex
debug: main.elf

main.hex main.map: main.asm
	avra -m main.map main.asm

main.elf: main.hex main.map
	avr-objcopy -v -I ihex -O elf32-avr main.hex main.elf

flash: main.hex
	avrdude -v -p attiny85 -c stk500v1 -b19200 -P /dev/ttyUSB1 -U flash:w:main.hex:i


.PHONY: build flash debug
