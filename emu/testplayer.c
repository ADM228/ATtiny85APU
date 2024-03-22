#include <stdint.h>
#include <stdio.h>
#define T85APU_SHIFT_REGISTER_SIZE 16

#include "ATtiny85APU.c"
#include "ATtiny85APU.h"

static const uint8_t megalovania[] = {
    0x27, 0x27, 0x4e, 0x00, 0x3a, 0x00, 0x00, 0x37, 0x00, 0x33, 0x00, 0x2e, 0x00, 0x27, 0x2e, 0x33, 0x23, 0x23, 0x4e, 0x00, 0x3a, 0x00, 0x00, 0x37, 0x00, 0x33, 0x00, 0x2e, 0x00, 0x27, 0x2e, 0x33, 0x21, 0x21, 0x4e, 0x00, 0x3a, 0x00, 0x00, 0x37, 0x00, 0x33, 0x00, 0x2e, 0x00, 0x27, 0x2e, 0x33, 0x1f, 0x1f, 0x4e, 0x00, 0x3a, 0x00, 0x00, 0x37, 0x00, 0x33, 0x00, 0x2e, 0x00, 0x27, 0x2e, 0x33
};

uint16_t sampleBuffer[960];
t85APU * apu;
FILE * file;

void writeShit() {
    for (int j = 0; j < 960; j++) {
            sampleBuffer[j] = t85APU_calc(apu);
            // t85APU_cycle(apu);
            // sampleBuffer[j] = apu->channelOutput[0] << 5; 
        }
    fwrite(sampleBuffer, sizeof(sampleBuffer), 1, file);
}

int main () {
    apu = t85APU_new(8000000, 48000, T85APU_OUTPUT_PB4);
    t85APU_setQuality(apu, 1);

    file = fopen("test.raw", "wb");

    t85APU_writeReg(apu, 0x06, 0x0B);
    t85APU_writeReg(apu, 0x09, 0x20);

    uint32_t output;

    for (int i = 0; i < sizeof(megalovania); i++) {
        t85APU_writeReg(apu, 0x00, megalovania[i]);
        t85APU_writeReg(apu, 0x10, 0xFF);
        writeShit(); writeShit();
        t85APU_writeReg(apu, 0x10, 0xC0);
        writeShit(); writeShit();
        t85APU_writeReg(apu, 0x10, 0x80);
        writeShit();
        t85APU_writeReg(apu, 0x10, 0x40);
        writeShit();
        t85APU_writeReg(apu, 0x10, 0x00);
        writeShit();
        printf("Playback: %2X \n", i);
    }
    fclose(file);

}