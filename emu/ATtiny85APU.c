#include "ATtiny85APU.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SHIFT_REG 0
#define STACK_REG 1

t85APU * t85APU_new (double clock, double rate, uint_fast8_t outputType) {
	t85APU * apu = (t85APU *) calloc(1, sizeof(t85APU));
	t85APU_setClocknRate(apu, clock, rate);
	t85APU_setOutputType(apu, outputType);
	t85APU_setQuality(apu, apu->ticksPerClockCycle == 512.0 ? 0 : 1);
	t85APU_reset(apu);
	apu->shiftRegister[0] = 0;
	apu->ticks = 0;
	apu->shiftRegMode = STACK_REG;
	apu->shiftRegPtr = 0;
	memset(apu->shiftRegister, 0, sizeof(uint16_t)*T85APU_SHIFT_REGISTER_SIZE);
	return apu;
}

void t85APU_reset (t85APU * apu) {
	memset(apu->dutyCycles,		0,	sizeof(uint8_t)*5);
	memset(apu->volumes,		0,	sizeof(uint8_t)*5);
	memset(apu->channelConfigs,	0,	sizeof(uint8_t)*5);

	memset(apu->tonePhaseAccs, 	0, 	sizeof(uint16_t)*(5+1+2+2));

	memset(apu->increments,			0,	sizeof(uint8_t)*6);
	memset(apu->shiftedIncrements,	0,	sizeof(uint16_t)*6);
	memset(apu->octaveValues,		0,	sizeof(uint8_t)*6);
	
	memset(apu->channelOutput,		0,	sizeof(uint8_t)*5);
	apu->noiseMask = 0;

	apu->clockCycle = 511;	// technically simplified

	apu->noiseXOR	= 0x2400;
	apu->noiseLFSR	= 0;
	
}

void t85APU_delete (t85APU * apu) {
	if (!apu) return;

	free(apu);
}


void t85APU_setClocknRate (t85APU * apu, double clock, double rate) {
	// Clock is the clock rate of the ATtiny85 itself in Hz, default 8000000Hz
	// Rate is the sample rate of the output, default is clock/512
	if (!clock)	clock = 8000000;
	if (!rate)	rate = clock / 512;

	apu->ticksPerClockCycle = clock / rate; 
};

static const uint_fast8_t outputTypesBitdepths[] = {
	8	// T85APU_OUTPUT_PB4
};

static const uint_fast16_t outputTypesDelays[] = {
	256	// T85APU_OUTPUT_PB4, the output is changed only on the next PWM cycle after the write, i.e. 256
};

void t85APU_setOutputType (t85APU * apu, uint_fast8_t outputType) {
	apu->outputType = outputType;
	
	if (outputType > sizeof(outputTypesBitdepths))
		outputType = T85APU_OUTPUT_PB4;
	
	apu->outputBitdepth	= outputTypesBitdepths	[outputType];
	apu->outputDelay	= outputTypesDelays		[outputType];
}

uint16_t t85APU_shiftReg (t85APU * apu, uint16_t newData) {
	uint16_t out = apu->shiftRegister[0];
	#if T85APU_SHIFT_REGISTER_SIZE > 1
	uint16_t buffer[T85APU_SHIFT_REGISTER_SIZE-1];
	memcpy (buffer, apu->shiftRegister+1, sizeof(uint16_t) * (T85APU_SHIFT_REGISTER_SIZE - 1));
	memcpy (apu->shiftRegister, buffer, sizeof(uint16_t) * (T85APU_SHIFT_REGISTER_SIZE - 1));
	#endif
	apu->shiftRegister[T85APU_SHIFT_REGISTER_SIZE-1] = newData;
	if (apu->shiftRegMode && apu->shiftRegPtr > 0) apu->shiftRegPtr--;
	return out;
}

void t85APU_writeReg (t85APU * apu, uint8_t addr, uint8_t data) {
	uint16_t shiftRegData = (addr << 8) | data | 0x8000;
	if (apu->shiftRegMode && apu->shiftRegPtr < T85APU_SHIFT_REGISTER_SIZE)	// == STACK_REG
		apu->shiftRegister[apu->shiftRegPtr++] = shiftRegData;
	else
		t85APU_shiftReg(apu, shiftRegData);
}

void t85APU_handleReg (t85APU * apu, uint8_t addr, uint8_t data) {
	addr &= 0x7F;
	uint8_t r0, r1 = data, r2, r3, ZL, ZH;
	switch(addr){
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			// Low pitch
			// addr -= 0;
			
			apu->increments[addr] = r1;
			r2 = apu->octaveValues[addr];

			// This is what the real AVR ASM code looks like:
			// uint16_t shiftedData = data << 8;	// r1:r0
			// if (!(r2 & 1<<2))	shiftedData >>= 4;
			// if (!(r2 & 1<<1))	shiftedData >>= 2;
			// if (!(r2 & 1<<0))	shiftedData >>= 1;
			// Absurdly long, isn't it? well, that's why the
			// following implementation is simplified:
			uint16_t shiftedData = data << (1+(r2 & 0x07));
			apu->shiftedIncrements[addr] = shiftedData;
			
			break;

		case 6:
		case 7:
		case 8:
			// Octave / phase reset
			addr = (addr - 6) << 1;

			ZL = data & 0x07;
			if (apu->octaveValues[addr] != ZL) {
				apu->octaveValues[addr] = ZL;
				r2 = apu->increments[addr];
				uint16_t shiftedData = r2 << (1+ZL);
				apu->shiftedIncrements[addr] = shiftedData;
			}
			ZL = (data >> 4) & 0x07;
			if (apu->octaveValues[addr+1] != ZL) {
				apu->octaveValues[addr+1] = ZL;
				r2 = apu->increments[addr+1];
				uint16_t shiftedData = r2 << (1+ZL);
				apu->shiftedIncrements[addr+1] = shiftedData;
			}
			if (r1 & 1<<3)	apu->tonePhaseAccs[addr] = 0;
			if (r1 & 1<<7)	apu->tonePhaseAccs[addr+1] = 0;

			break;

		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
			// Duty cycles
			apu->dutyCycles[addr-9] = data;

			break;

		case 14:
			// Noise XOR value (low)
			apu->noiseXOR = (apu->noiseXOR & 0xFF00) | data;
			break;
		case 15:
			// Noise XOR value (high)
			apu->noiseXOR = (apu->noiseXOR & 0xFF) | (data << 8);
			break;

		case 16:
		case 17:
		case 18:
		case 19:
		case 20:
			// Volume
			apu->volumes[addr-16] = data;

			break;
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
			// Channel settings
			apu->channelConfigs[addr-21] = data;
		default:
			break;
	}
}

void t85APU_setQuality (t85APU * apu, uint_fast8_t quality) {
	apu->quality = quality;
}

bool t85APU_shiftRegisterPending(t85APU * apu) {
	return (apu->shiftRegister[0] & 0x8000) ? true : false;
}

void t85APU_cycle (t85APU * apu) {
	uint_fast8_t skipCount = 4;
	if (apu->shiftRegister[0] & 0x8000) {
		// TODO handle skip count
		uint16_t data = t85APU_shiftReg(apu, 0);
		t85APU_handleReg(apu, (data >> 8) & 0xFF, data & 0xFF);
	}
	apu->noisePhaseAcc += apu->shiftedIncrements[5];
	if (apu->noisePhaseAcc < apu->shiftedIncrements[5])	{	// If overflowed
		bool carry = apu->noiseLFSR & 1;
		apu->noiseMask = carry ? 0 : 0x80;
		apu->noiseLFSR >>= 1;
		if (!carry) apu->noiseLFSR ^= apu->noiseXOR;
	}
	for (int ch = 0; ch < 5; ch++) {
		uint8_t r1 = apu->channelConfigs[ch] & apu->noiseMask;
		apu->tonePhaseAccs[ch] += apu->shiftedIncrements[ch];
		if (apu->tonePhaseAccs[ch] >> 8 < apu->dutyCycles[ch]) r1++;	// really another bit set
		apu->channelOutput[ch] = r1 ? apu->volumes[ch] : 0;
	}
	apu->nextOutput = apu->channelOutput[0];
}

void t85APU_tick (t85APU * apu) {
	apu->clockCycle++;
	if (apu->clockCycle >= 512) {
		apu->clockCycle -= 512;
		t85APU_cycle(apu);
		apu->outPending = 1;
	}
	if (apu->outPending && apu->clockCycle >= apu->outputDelay) {
		apu->outPending = 0;
		apu->currentOutput = apu->nextOutput;
	}
}

uint32_t t85APU_calc(t85APU *apu) {
	apu->ticks += apu->ticksPerClockCycle;
	uint32_t output;
	uint32_t * array;
	size_t totalSize = floor(apu->ticks);
	if (apu->quality >= 1 && totalSize > 0) array = calloc(totalSize, sizeof(uint32_t)); 
	for (size_t i = 0; i < totalSize; i++) {
		t85APU_tick (apu);
		if (apu->quality >= 1 && totalSize > 0) array[i] = apu->currentOutput;
	}
	double totalOutput = 0;
	apu->ticks -= floor(apu->ticks);
	switch (apu->quality) {
		case 1:
			for (size_t i = 0; i < totalSize; i++) totalOutput += (double)array[i];
			totalOutput /= (totalSize + 1);
			output = (uint32_t)totalOutput;
			break;
		case 0:
		default:
			output = apu->currentOutput;
			break;
	}
	if (apu->quality >= 1 && totalSize > 0) free(array);
	return output;
}