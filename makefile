
# Only intended for linux.
firmware: bin/avr/main.hex
t85play: bin/t85play

bin/avr/main.hex bin/avr/main.map: bin/avr avr/main.asm
	avra -m bin/avr/main.map -e bin/avr/main.eep.hex avr/main.asm -o bin/avr/main.hex

bin/avr/main.elf: bin/avr bin/avr/main.hex bin/avr/main.map
	avr-objcopy -v -I ihex -O elf32-avr bin/avr/main.hex bin/avr/main.elf

flash: bin/avr bin/avr/main.hex
	# DEV := "/dev/ttyUSB0"
	avrdude -v -p attiny85 -c stk500v1 -b19200 -P /dev/ttyUSB0 -U flash:w:bin/avr/main.hex:i

bin/t85play: bin/emu bin/libt85apu.a emu/t85play.cpp emu/NotchFilter.cpp emu/BitConverter.cpp
	g++ -g -O2 -c emu/t85play.cpp -o bin/emu/t85play.o
	g++ -g -o bin/emu/t85play -L bin/emu bin/emu/t85play.o -lt85apu -lsndfile -lsoundio

bin/emu:
	mkdir -p bin/emu

bin/avr:
	mkdir -p bin/avr

bin/libt85apu.o: bin/emu emu/libt85apu/t85apu.h emu/libt85apu/t85apu.c 
	gcc -O2 -std=c99 -c emu/libt85apu/t85apu.c -lm -o bin/emu/libt85apu.o

bin/libt85apu.a: bin/emu bin/libt85apu.o
	ar rs bin/emu/libt85apu.a bin/emu/libt85apu.o

clean:
	rm -r bin

.PHONY: firmware flash t85play clean
