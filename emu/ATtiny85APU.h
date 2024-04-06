/* ATtiny85APU.h */
#ifndef __ATTINY85APU_H__
#define __ATTINY85APU_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef T85APU_SHIFT_REGISTER_SIZE
#define T85APU_SHIFT_REGISTER_SIZE 1
#endif

typedef struct __ATtiny85APU {
	// Replica of internal RAM
	uint16_t noiseLFSR;
	uint16_t envPhaseAccs[2];
	uint16_t smpPhaseAccs[2];
	uint8_t envStates[2];
	uint8_t envShape;

	uint8_t dutyCycles[5];
	uint16_t noiseXOR;
	uint8_t volumes[5];
	uint8_t channelConfigs[5];
	uint16_t envLdBuffer;

	uint8_t increments[8];
	uint16_t shiftedIncrements[8];	// This is simplified
	uint8_t octaveValues[7];

	// Replica of registers
	// Phase accumulators
	uint16_t tonePhaseAccs[5];
	uint16_t noisePhaseAcc;

	uint8_t envSmpVolume[4];

	uint8_t noiseMask;
	uint8_t envZeroFlg;

	// Shift register emulation
	uint16_t shiftRegister[T85APU_SHIFT_REGISTER_SIZE];
	size_t shiftRegPtr;
	bool shiftRegMode;

	// Compile-time options
	uint_fast8_t outputType;
	uint_fast8_t outputBitdepth;
	uint_fast16_t outputDelay;

	// Sample rate converter
	uint_fast16_t clockCycle;	// 0..511, on 0 an update is invoked
	double ticksPerClockCycle;
	double ticks;	// Reset when updated to keep precision
	uint_fast8_t quality;	// 0 - no interpolation/alialising, 1 - averaging of outputs per sample
	bool outPending;
	
	// Output
	uint16_t channelOutput[5];
	uint32_t currentOutput;
	uint32_t outputQueue[3];	// Only really applies to the PWM output, [0] is current output
} t85APU;

#define T85APU_OUTPUT_PB4 0
#define T85APU_OUTPUT_PB4_EXACT 1

t85APU * t85APU_new (double clock, double rate, uint_fast8_t outputType);	// Automatically resets it
void t85APU_delete (t85APU * apu);
void t85APU_reset (t85APU * apu);

void t85APU_setClocknRate (t85APU * apu, double clock, double rate);
void t85APU_setOutputType (t85APU * apu, uint_fast8_t outputType);
void t85APU_setQuality	  (t85APU * apu, uint_fast8_t quality);

void t85APU_writeReg (t85APU * apu, uint8_t addr, uint8_t data);

uint32_t t85APU_calc (t85APU * apu);

bool t85APU_shiftRegisterPending (t85APU * apu);

#ifdef __cplusplus
}
#endif

#endif