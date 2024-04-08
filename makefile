
# Only intended for linux.
build: main.hex
debug: main.elf
t85play: bin/t85play

main.hex main.map: main.asm
	avra -m main.map main.asm

main.elf: main.hex main.map
	avr-objcopy -v -I ihex -O elf32-avr main.hex main.elf

flash: main.hex
	avrdude -v -p attiny85 -c stk500v1 -b19200 -P /dev/ttyUSB0 -U flash:w:main.hex:i

bin/t85play: bin bin/libt85apu.a emu/t85play.cpp
	g++ -O2 -c emu/t85play.cpp -o bin/t85play.o
	g++ -o bin/t85play -L bin bin/t85play.o -lt85apu -lsndfile

bin:
	mkdir bin

bin/libt85apu.a: bin emu/ATtiny85APU.h emu/ATtiny85APU.c
	gcc -O2 -std=c99 -c emu/ATtiny85APU.c -lm -o bin/libt85apu.o
	ar rs bin/libt85apu.a bin/libt85apu.o


.PHONY: build flash debug t85play
