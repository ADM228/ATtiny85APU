
# Only intended for linux.
build: main.hex
main.hex: main.asm
	avra main.asm

flash: main.hex
	avrdude -v -p attiny85 -c stk500v1 -b19200 -P /dev/ttyUSB0 -U flash:w:main.hex:i


.PHONY: build flash
