/* 
t85apu.c
Part of the ATtiny85APU emulation library
Written by alexmush
2024-2024
*/

#include "t85apu.h"
#include <stdio.h>
#include <math.h>
#include <tgmath.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SHIFT_REG 0
#define STACK_REG 1

#define EnvAZero 0
#define SmpAZero 1
#define EnvASlope 2
#define EnvBZero 4
#define SmpBZero 5
#define EnvBSlope 6

#define ENV_A_HOLD	0
#define ENV_A_ALT	1
#define ENV_A_ATT	2
#define ENV_A_RST	3
#define ENV_B_HOLD	4
#define ENV_B_ALT	5
#define ENV_B_ATT	6
#define ENV_B_RST	7

#define member_sizeof(type, member) sizeof(((type *)0)->member)

static const uint_fast8_t outputTypesBitdepths[] = {
	8,	// T85APU_OUTPUT_PB4
	8,	// T85APU_OUTPUT_PB4_EXACT
};

static const uint_fast16_t outputTypesDelays[] = {
	512,	// T85APU_OUTPUT_PB4, the output is changed only on the next PWM cycle after the write, i.e. 512
	512,	// T85APU_OUTPUT_PB4_EXACT, same thing
};

static const double outputQualityThreshold[] = {
	512.0,	// T85APU_OUTPUT_PB4, the output is only changed every 512 cycles (as most are)
	1.0,	// T85APU_OUTPUT_PB4_EXACT, PWM is a bitch and changes every cycle
};

#ifdef T85APU_SHIFT_REGISTER_SIZE
t85APU * t85APU_new (double clock, double rate, uint_fast8_t outputType) {
#else
t85APU * t85APU_new (double clock, double rate, uint_fast8_t outputType, size_t shiftRegisterSize) {
#endif
	t85APU * apu = (t85APU *) calloc(1, sizeof(t85APU));
	if (!apu) {
		fprintf(stderr, "Could not allocate t85APU\n");
		return NULL;
	}
	apu->resamplingBuffer = NULL;
	t85APU_setClocknRate(apu, clock, rate);
	t85APU_setOutputType(apu, outputType);
	double tmp;
	t85APU_setQuality(apu, 
	 modf(log2(apu->ticksPerClockCycle), &tmp) == 0.0 && apu->ticksPerClockCycle <= outputQualityThreshold[apu->outputType]
	 ? 0 : 1);
	t85APU_reset(apu);
	apu->shiftRegister[0] = 0;
	apu->ticks = 0;
	apu->shiftRegCurIdx = 0;
	#ifdef T85APU_SHIFT_REGISTER_SIZE
	memset(apu->shiftRegister, 0, sizeof(uint16_t)*T85APU_SHIFT_REGISTER_SIZE);
	#else
	apu->shiftRegister = calloc(shiftRegisterSize, sizeof(uint16_t));
	if (!apu->shiftRegister){
		fprintf(stderr, "Could not allocate t85apu shift register, deleting the t85APU\n");
		if (apu->resamplingBuffer) free(apu->resamplingBuffer);
		free(apu);
		return NULL;
	}
	#endif
	memset(apu->channelMute,	false,	sizeof(bool)*5);
	return apu;
}

void t85APU_reset (t85APU * apu) {
	if (!apu) return;
	memset(apu->tonePhaseAccs, 	0, 	sizeof(uint16_t)*(5+1));
	memset(apu->envSmpVolume,	0,	sizeof(uint8_t)*(4));

	memset(apu->dutyCycles,		0,	sizeof(uint8_t)*5);
	memset(apu->volumes,		0,	sizeof(uint8_t)*5);
	memset(apu->channelConfigs,	0x0F,	sizeof(uint8_t)*5);

	memset(apu->increments,			0,	sizeof(uint8_t)*8);
	memset(apu->shiftedIncrements,	0,	sizeof(uint16_t)*8);
	memset(apu->octaveValues,		0,	sizeof(uint8_t)*7);

	memset(apu->envPhaseAccs,		0,	sizeof(uint16_t)*2);
	memset(apu->smpPhaseAccs,		0,	sizeof(uint16_t)*2);
	memset(apu->envStates,			0,	sizeof(uint8_t)*2);
	apu->envLdBuffer = 0;
	
	memset(apu->channelOutput,		0,	sizeof(uint8_t)*5);
	apu->noiseMask = 0x7F;

	apu->clockCycle = 0;	// technically simplified

	apu->noiseXOR	= 0x2400;
	apu->noiseLFSR	= 0;

	apu->envShape = 0;
	apu->envZeroFlg = (1<<EnvAZero|1<<EnvBZero|1<<SmpAZero|1<<SmpBZero);
}

void t85APU_delete (t85APU * apu) {
	if (!apu) return;

	free(apu);
}


void t85APU_setClocknRate (t85APU * apu, double clock, double rate) {
	if (!apu) return;
	// Clock is the clock rate of the ATtiny85 itself in Hz, default 8000000Hz
	// Rate is the sample rate of the output, default is clock/512
	if (!clock)	clock = 8000000;
	if (!rate)	rate = clock / 512;

	apu->ticksPerClockCycle = clock / rate; 

	if (apu->resamplingBuffer) free(apu->resamplingBuffer);
	apu->resamplingBuffer = calloc(ceil(apu->ticksPerClockCycle), sizeof(uint32_t));
	if (!apu->resamplingBuffer) {
		fprintf(stderr, "Could not allocate t85APU resampling buffer, quality will be forced to be 0\n");
		apu->quality = 0;
	}
};

void t85APU_setOutputType (t85APU * apu, uint_fast8_t outputType) {
	if (!apu) return;
	apu->outputType = outputType;
	
	if (outputType > sizeof(outputTypesBitdepths))
		outputType = T85APU_OUTPUT_PB4;
	
	apu->outputBitdepth	= outputTypesBitdepths	[outputType];
	apu->outputDelay	= outputTypesDelays		[outputType];
}

uint16_t t85APU_shiftReg (t85APU * apu, uint16_t newData) {
	if (!apu) return 0;
	uint16_t out = apu->shiftRegister[0];
	#if T85APU_SHIFT_REGISTER_SIZE > 1
	uint16_t buffer[T85APU_SHIFT_REGISTER_SIZE-1];
	memcpy (buffer, apu->shiftRegister+1, sizeof(uint16_t) * (T85APU_SHIFT_REGISTER_SIZE - 1));
	memcpy (apu->shiftRegister, buffer, sizeof(uint16_t) * (T85APU_SHIFT_REGISTER_SIZE - 1));
	apu->shiftRegister[T85APU_SHIFT_REGISTER_SIZE-1] = newData;
	#elif T85APU_SHIFT_REGISTER_SIZE == 1
	apu->shiftRegister[0] = newData;
	#elif !defined(T85APU_SHIFT_REGISTER_SIZE)
	for (size_t i = 0; i < apu->shiftRegSize-1; i++) {
		apu->shiftRegister[i] = apu->shiftRegister[i+1];
	}
	apu->shiftRegister[apu->shiftRegSize-1] = newData;
	#endif
	if (apu->shiftRegCurIdx > 0) apu->shiftRegCurIdx--;
	return out;
}

void t85APU_writeReg (t85APU * apu, uint8_t addr, uint8_t data) {
	if (!apu) return;
	uint16_t shiftRegData = (addr << 8) | data | 0x8000;
#ifdef T85APU_SHIFT_REGISTER_SIZE
	if (apu->shiftRegCurIdx < T85APU_SHIFT_REGISTER_SIZE)	// == STACK_REG
#else
	if (apu->shiftRegCurIdx < apu->shiftRegSize)
#endif
		apu->shiftRegister[apu->shiftRegCurIdx++] = shiftRegData;
}

void t85APU_handleReg (t85APU * apu, uint8_t addr, uint8_t data) {
	if (!apu) return;

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
			apu->increments[addr] = r1;
			r2 = apu->octaveValues[addr];

			// This is what the real AVR ASM code looks like:
			// uint16_t shiftedData = data << 8;	// r1:r0
			// if (!(r2 & 1<<2))	shiftedData >>= 4;
			// if (!(r2 & 1<<1))	shiftedData >>= 2;
			// if (!(r2 & 1<<0))	shiftedData >>= 1;
			// Absurdly long, isn't it? well, that's why the
			// following implementation is simplified:
			apu->shiftedIncrements[addr] = data << (1+(r2 & 0x07));
			
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
				apu->shiftedIncrements[addr] = r2 << (1+ZL);
			}
			ZL = (data >> 4) & 0x07;
			if (apu->octaveValues[addr+1] != ZL) {
				apu->octaveValues[addr+1] = ZL;
				r2 = apu->increments[addr+1];
				apu->shiftedIncrements[addr+1] = r2 << (1+ZL);
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
			break;
		case 26:
			// Envelope load value (low)
			apu->envLdBuffer = (apu->envLdBuffer & 0xFF00) | data;
			break;
		case 27:
			// Envelope load value (high)
			apu->envLdBuffer = (apu->envLdBuffer & 0xFF) | (data << 8);
			break;
		case 28:
			// Envelope shape
			r3 = apu->envShape;
			r3 ^= data;
			r3 &= (1<<ENV_A_ATT)|(1<<ENV_B_ATT);
			apu->envZeroFlg ^= r3;
			if (data & 1<<ENV_A_RST) {
				apu->envPhaseAccs[0] = ((apu->envLdBuffer << 8) & 0xFF00);
				apu->envStates[0] = ((apu->envLdBuffer >> 8) & 0xFF);
				apu->envZeroFlg &= ~((1<<EnvAZero)|(1<<EnvASlope));
				apu->envZeroFlg |= data & 1<<ENV_A_ATT;
			}
			if (data & 1<<ENV_B_RST) {
				apu->envPhaseAccs[1] = ((apu->envLdBuffer << 8) & 0xFF00);
				apu->envStates[1] = ((apu->envLdBuffer >> 8) & 0xFF);
				apu->envZeroFlg &= ~((1<<EnvBZero)|(1<<EnvBSlope));
				apu->envZeroFlg |= data & 1<<ENV_B_ATT;
			}
			apu->envShape = data;
			break;
		case 29:
		case 30:
			// Envelope low pitch
			apu->increments[addr-23] = r1;
			r2 = apu->octaveValues[6];
			if (addr == 30) r2 >>= 4;

			apu->shiftedIncrements[addr-23] = data << (1+(r2 & 0x07));
			
			break;
		case 31:
			// Envelope high pitch
			ZL = apu->octaveValues[6];
			ZH = ZL;
			ZL = (ZL ^ data) & 0x07;
			if (ZL) {
				r2 = apu->increments[6];
				apu->shiftedIncrements[6] = r2 << (1+(data & 0x07));
			}
			ZH = (ZH ^ data) & 0x70;
			if (ZH) {
				r2 = apu->increments[7];
				apu->shiftedIncrements[7] = r2 << (1+((data >> 4) & 0x07));
			}
			apu->octaveValues[6] = data;
			break;
		default:
			break;
	}
}

void t85APU_setQuality (t85APU * apu, uint_fast8_t quality) {
	if (!apu) return;
	apu->quality = apu->resamplingBuffer ? quality : 0;	// Force quality to 0 if no resampling buffer
}

bool t85APU_shiftRegisterPending(t85APU * apu) {
	if (!apu) return 0;
	return (apu->shiftRegister[0] & 0x8000) ? true : false;
}

void t85APU_cycle (t85APU * apu) {
	if (!apu) return;

	uint_fast8_t skipCount = 4;
	if (apu->shiftRegister[0] & 0x8000) {
		// TODO handle skip count
		uint16_t data = t85APU_shiftReg(apu, 0);
		t85APU_handleReg(apu, (data >> 8) & 0xFF, data & 0xFF);
	}
	// PhaseAccEnvUpd:
	uint8_t r18 = apu->octaveValues[6];
	if (!(apu->envZeroFlg & 1<<EnvAZero)) {
		uint32_t fakeAcc = apu->envPhaseAccs[0];
		fakeAcc += (apu->shiftedIncrements[6] << ((r18 & 1<<3) ? 8 : 0));
		apu->envPhaseAccs[0] = fakeAcc & 0xFFFF;
		uint8_t r3 = (fakeAcc >> 16) & 0xFF;
		if (r3) {
			apu->envStates[0] += r3;
			apu->envSmpVolume[0] = apu->envStates[0];
			if (apu->envStates[0] < r3) {	// If the envelope overflowed
				if (apu->envShape & 1<<ENV_A_ALT) apu->envZeroFlg ^= 1<<EnvASlope;
				if (apu->envShape & 1<<ENV_A_HOLD) {
					apu->envSmpVolume[0] = 0xFF;
					apu->envZeroFlg |= 1<<EnvAZero;
				}
			}
			if (!(apu->envZeroFlg & 1<<EnvASlope)) apu->envSmpVolume[0] ^= 0xFF;
		}	
	}
	if (!(apu->envZeroFlg & 1<<EnvBZero)) {
		uint32_t fakeAcc = apu->envPhaseAccs[1];
		fakeAcc += (apu->shiftedIncrements[7] << ((r18 & 1<<7) ? 8 : 0));
		apu->envPhaseAccs[1] = fakeAcc & 0xFFFF;
		uint8_t r3 = (fakeAcc >> 16) & 0xFF;
		if (r3) {
			apu->envStates[1] += r3;
			apu->envSmpVolume[1] = apu->envStates[1];
			if (apu->envStates[1] < r3) {	// If the envelope overflowed
				if (apu->envShape & 1<<ENV_B_ALT) apu->envZeroFlg ^= 1<<EnvBSlope;
				if (apu->envShape & 1<<ENV_B_HOLD) {
					apu->envSmpVolume[1] = 0xFF;
					apu->envZeroFlg |= 1<<EnvBZero;
				}
			}
			if (!(apu->envZeroFlg & 1<<EnvBSlope)) apu->envSmpVolume[1] ^= 0xFF;
		}	
	}

	apu->noisePhaseAcc += apu->shiftedIncrements[5];
	if (apu->noisePhaseAcc < apu->shiftedIncrements[5])	{	// If overflowed
		bool carry = apu->noiseLFSR & 1;
		apu->noiseMask = carry ? 0x7F : 0xFF;
		apu->noiseLFSR >>= 1;
		if (!carry) apu->noiseLFSR ^= apu->noiseXOR;
	}
	for (int ch = 0; ch < 5; ch++) {
		uint8_t r1 = apu->channelConfigs[ch] & apu->noiseMask;
		apu->tonePhaseAccs[ch] += apu->shiftedIncrements[ch];
		if (apu->tonePhaseAccs[ch] >> 8 < apu->dutyCycles[ch]) r1 |= 1<<7;	// really another bit set
		if (r1 & 1<<7) {
			uint8_t r0 = apu->volumes[ch];
			if (r1 & 1<<6) {
				uint8_t envVol = apu->envSmpVolume[(r1>>4) & 0x03];
				if (!(r0 & 0x80)) envVol >>= 1;
				r0 = envVol;
			}
			apu->channelOutput[ch] = r0 * (r1 & 0x03);
		} else apu->channelOutput[ch] = 0;
	}
	uint32_t output = 0;
	for (int i = 0; i < 5; i++) {output += apu->channelMute[i] ? 0 : apu->channelOutput[i];}
	output *= 274;	// the Multiply routine
	output >>= 20 - (uint32_t)fmin(apu->outputBitdepth, 20);
	apu->outputQueue[(511+apu->outputDelay)>>9] = output;
}

void t85APU_tick (t85APU * apu) {
	if (!apu) return;

	if (!apu->clockCycle) {
		t85APU_cycle(apu);
		apu->outPending = 1;
	}
	if (apu->outPending && apu->clockCycle >= (apu->outputDelay & 511)) {
		apu->outPending = 0;
		apu->outputQueue[0] = apu->outputQueue[1];
		apu->outputQueue[1] = apu->outputQueue[2];
	}
	if (apu->outputType == T85APU_OUTPUT_PB4_EXACT)
		apu->currentOutput = (apu->clockCycle & 0xFF) > apu->outputQueue[0] ? 0x00 : 0xFF;
	else apu->currentOutput = apu->outputQueue[0];
	apu->clockCycle++;
	apu->clockCycle &= 511;
}

uint32_t t85APU_calc(t85APU *apu) {
	if (!apu) return 0;

	apu->ticks += apu->ticksPerClockCycle;
	uint32_t output;
	size_t totalSize = floor(apu->ticks);
	for (size_t i = 0; i < totalSize; i++) {
		t85APU_tick (apu);
		if (apu->quality >= 1 && totalSize > 0) apu->resamplingBuffer[i] = apu->currentOutput;
	}
	double totalOutput = 0;
	double tmp;
	apu->ticks = modf(apu->ticks, &tmp);
	switch (apu->quality) {
		case 1:
			for (size_t i = 0; i < totalSize; i++) totalOutput += (double)apu->resamplingBuffer[i];
			totalOutput /= totalSize;
			output = (uint32_t)totalOutput;
			break;
		case 0:
		default:
			output = apu->currentOutput;
			break;
	}
	return output;
}

uint16_t t85APU_calcU16 (t85APU * apu) {
	if (!apu) return 0;

	apu->ticks += apu->ticksPerClockCycle;
	uint16_t output;
	size_t totalSize = floor(apu->ticks);
	for (size_t i = 0; i < totalSize; i++) {
		t85APU_tick (apu);
		if (apu->quality >= 1 && totalSize > 0) apu->resamplingBuffer[i] = (apu->currentOutput)<<(16-apu->outputBitdepth);
	}
	double totalOutput = 0;
	double tmp;
	apu->ticks = modf(apu->ticks, &tmp);
	switch (apu->quality) {
		case 1:
			for (size_t i = 0; i < totalSize; i++) totalOutput += (double)apu->resamplingBuffer[i];
			totalOutput /= totalSize;
			output = (uint16_t)totalOutput;
			break;
		case 0:
		default:
			output = (apu->currentOutput)<<(16-apu->outputBitdepth);
			break;
	}
	return output;
}

int16_t t85APU_calcS16 (t85APU * apu) {
	if (!apu) return 0;

	apu->ticks += apu->ticksPerClockCycle;
	uint16_t output;
	size_t totalSize = floor(apu->ticks);
	for (size_t i = 0; i < totalSize; i++) {
		t85APU_tick (apu);
		if (apu->quality >= 1 && totalSize > 0) apu->resamplingBuffer[i] = (apu->currentOutput)<<(15-apu->outputBitdepth);
	}
	double totalOutput = 0;
	double tmp;
	apu->ticks = modf(apu->ticks, &tmp);
	switch (apu->quality) {
		case 1:
			for (size_t i = 0; i < totalSize; i++) totalOutput += (double)apu->resamplingBuffer[i];
			totalOutput /= totalSize;
			output = (uint16_t)totalOutput;
			break;
		case 0:
		default:
			output = (apu->currentOutput)<<(15-apu->outputBitdepth);
			break;
	}
	return (int16_t)output;
}

uint32_t t85APU_calcU32 (t85APU * apu) {
	if (!apu) return 0;

	apu->ticks += apu->ticksPerClockCycle;
	uint32_t output;
	size_t totalSize = floor(apu->ticks);
	for (size_t i = 0; i < totalSize; i++) {
		t85APU_tick (apu);
		if (apu->quality >= 1 && totalSize > 0) apu->resamplingBuffer[i] = (apu->currentOutput)<<(32-apu->outputBitdepth);
	}
	double totalOutput = 0;
	double tmp;
	apu->ticks = modf(apu->ticks, &tmp);
	switch (apu->quality) {
		case 1:
			for (size_t i = 0; i < totalSize; i++) totalOutput += (double)apu->resamplingBuffer[i];
			totalOutput /= totalSize;
			output = (uint32_t)totalOutput;
			break;
		case 0:
		default:
			output = (apu->currentOutput)<<(32-apu->outputBitdepth);
			break;
	}
	return output;
}

int32_t t85APU_calcS32 (t85APU * apu) {
	if (!apu) return 0;

	apu->ticks += apu->ticksPerClockCycle;
	uint32_t output;
	size_t totalSize = floor(apu->ticks);
	for (size_t i = 0; i < totalSize; i++) {
		t85APU_tick (apu);
		if (apu->quality >= 1 && totalSize > 0) apu->resamplingBuffer[i] = (apu->currentOutput)<<(31-apu->outputBitdepth);
	}
	double totalOutput = 0;
	double tmp;
	apu->ticks = modf(apu->ticks, &tmp);
	switch (apu->quality) {
		case 1:
			for (size_t i = 0; i < totalSize; i++) totalOutput += (double)apu->resamplingBuffer[i];
			totalOutput /= totalSize;
			output = (uint16_t)totalOutput;
			break;
		case 0:
		default:
			output = (apu->currentOutput)<<(31-apu->outputBitdepth);
			break;
	}
	return (int32_t)output;
}

void t85APU_setMute(t85APU * apu, uint_fast8_t channel, bool mute){
	if (!apu) return;

	if (channel > 4) return;
	apu->channelMute[channel] = mute;
}