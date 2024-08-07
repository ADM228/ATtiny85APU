ATTINY := 85

# Only intended for linux.
firmware: bin/avr/main.hex
avr: bin/avr/main.hex
libt85apu: bin/emu/libt85apu.a bin/emu/libt85apu.so

bin/avr/main.hex bin/avr/main.map: bin/avr avr/main.asm
	avra -D ATTINY=$(ATTINY) -I avr -m bin/avr/main.map -e bin/avr/main.eep.hex avr/main.asm -o bin/avr/main.hex

bin/avr/main.elf: bin/avr bin/avr/main.hex bin/avr/main.map
	avr-objcopy -v -I ihex -O elf32-avr bin/avr/main.hex bin/avr/main.elf

flash: bin/avr bin/avr/main.hex
	# DEV := "/dev/ttyUSB1"
	avrdude -v -p attiny85 -c stk500v1 -b19200 -P /dev/ttyUSB1 -U flash:w:bin/avr/main.hex:i -U lfuse:w:0xE2:m

bin/emu:
	mkdir -p bin/emu

bin/avr:
	mkdir -p bin/avr

bin/emu/libt85apu.a: bin/emu emu/libt85apu/t85apu.h emu/libt85apu/t85apu.c 
	gcc -O2 -std=c99 -c emu/libt85apu/t85apu.c -lm -o bin/emu/libt85apu.o
	ar rs bin/emu/libt85apu.a bin/emu/libt85apu.o

bin/emu/libt85apu.so: 
	gcc -O2 -std=c99 -fPIC -c emu/libt85apu/t85apu.c -lm -o bin/emu/libt85apu.o
	gcc -shared -o bin/emu/libt85apu.so bin/emu/libt85apu.o

clean:
	rm -r bin

.PHONY: avr firmware flash libt85apu t85play clean
