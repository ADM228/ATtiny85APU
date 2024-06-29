/* 
t85apu.h
Part of the ATtiny85APU emulation library
Written by alexmush
2024-2024
*/

#ifndef __T85APU_H__
#define __T85APU_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
	The T85APU_SHIFT_REGISTER_SIZE define sets the size of the register write buffer. If it is 0 or less (or undefined), then the register write buffer size is decided when initializing the t85APU.
*/

#if T85APU_SHIFT_REGISTER_SIZE < 1
#undef T85APU_SHIFT_REGISTER_SIZE
#endif

typedef struct __t85apu {
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
	uint32_t * resamplingBuffer;
	
	// Output
	uint16_t channelOutput[5];
	uint32_t currentOutput;
	uint32_t outputQueue[3];	// Only really applies to the PWM output

	// Emulator-only options
	bool channelMute[5];

	// Shift register emulation
	#ifdef T85APU_SHIFT_REGISTER_SIZE
	uint16_t shiftRegister[T85APU_SHIFT_REGISTER_SIZE];
	#else
	uint16_t * shiftRegister;
	size_t shiftRegSize;
	#endif
	size_t shiftRegCurIdx;
} t85APU;

/**
 *  @brief Output type for t85APU: emulates the PWM output from the @c OUT pin as an 8-bit DAC.
 */
#define T85APU_OUTPUT_PB4 0
/**
 * @brief Output type for t85APU: emulates the PWM output from the @c OUT pin as the exact PWM (at the rate of (t85APU's clock / 256)). Can take more CPU time.
 */
#define T85APU_OUTPUT_PB4_EXACT 1

#ifdef T85APU_SHIFT_REGISTER_SIZE
/**
 * @brief Creates a new instance of t85APU
 * 
 * @param clock The master clock speed of the t85APU, in Hz. If not set (i.e. 0), will default to 8000000 - 8MHz. 
 * @param rate The output sample rate of the t85APU, in Hz. If not set (i.e. 0), will default to (clock / 512).
 * @param outputType The output type of the t85APU. Use the @c T85APU_OUTPUT_XXX defines above to select the output type.
 * @return The pointer to the newly created instance of t85APU. Returns a null pointer if an error has occured.
 */
t85APU * t85APU_new (double clock, double rate, uint_fast8_t outputType);	// Automatically resets it
#else
/**
 * @brief Creates a new instance of t85APU
 * 
 * @param clock The master clock speed of the t85APU, in Hz. If not set (i.e. 0), will default to 8000000 - 8MHz. 
 * @param rate The output sample rate of the t85APU, in Hz. If not set (i.e. 0), will default to (clock / 512).
 * @param outputType The output type of the t85APU. Use the @c T85APU_OUTPUT_XXX defines above to select the output type.
 * @param shiftRegisterSize The size of the register write buffer. Has to be at least 1.
 * @return The pointer to the newly created instance of t85APU. Returns a null pointer if an error has occured.
 */
t85APU * t85APU_new (double clock, double rate, uint_fast8_t outputType, size_t shiftRegisterSize);
#endif
/**
 * @brief Deletes the instance of t85APU from memory.
 * 
 * @param apu The t85APU instance to delete.
 */
void t85APU_delete (t85APU * apu);
/**
 * @brief Resets all internal variables of the t85APU to their default initalization state - effectively the same as pulling the @c /RESET pin low on real hardware.
 * @note This does NOT clear the register write buffer as it is considered emulated external hardware. This also does not reset the settings of the t85APU (clock speed, sample rate, output type, quality and muting).
 * 
 * @param apu The t85APU instance to reset.
 */
void t85APU_reset (t85APU * apu);

/**
 * @brief Sets clock speed and sample rate of the t85APU.
 * 
 * @param apu The t85APU instance to set the clock speed and sample rate for.
 * @param clock The master clock speed of the t85APU, in Hz. If not set (i.e. 0), will default to 8000000 - 8MHz. 
 * @param rate The output sample rate of the t85APU, in Hz. If not set (i.e. 0), will default to (clock / 512).
 */
void t85APU_setClocknRate (t85APU * apu, double clock, double rate);
/**
 * @brief Sets the output type of the t85APU.
 * 
 * @param apu The t85APU instance to set the output type for.
 * @param outputType The output type of the t85APU. Use the @c T85APU_OUTPUT_XXX defines above to select the output type.
 */
void t85APU_setOutputType (t85APU * apu, uint_fast8_t outputType);
/**
 * @brief Sets the sample rate converter quality.
 * 
 * @param apu The t85APU instance to set the quality of the sample rate converter for.
 * @param quality The quality setting
 * @li 0 makes the immediate output of the t85APU the final output. Takes less CPU time, but has alialising issues.
 * @li 1 averages all of the outputs in that tick and makes that the final output. Takes more CPU time, but doesn't have alialising issues. 
 */
void t85APU_setQuality	  (t85APU * apu, uint_fast8_t quality);

/**
 * @brief Pushes data onto the register write buffer of the t85APU.
 * 
 * @param apu The t85APU instance to push the register write onto.
 * @param addr The register number to write to.
 * @param data The data to write to the register.
 */
void t85APU_writeReg (t85APU * apu, uint8_t addr, uint8_t data);

/**
 * @brief Calculates 1 sample and return its raw value.
 * 
 * @param apu The t85APU instance.
 * @return The raw value of the sample, depends on the bitdepth of the output mode.
 */
uint32_t t85APU_calc (t85APU * apu);
/**
 * @brief Calculates 1 sample and return its sample value mapped to unsigned 16-bit limits.
 * 
 * @param apu The t85APU instance.
 * @return The sample value, mapped from its raw value to 0..65535.
 */
uint16_t t85APU_calcU16 (t85APU * apu);
/**
 * @brief Calculates 1 sample and return its sample value mapped to unsigned 15-bit limits.
 * 
 * @param apu The t85APU instance.
 * @return The sample value, mapped from its raw value to 0..32767.
 */
int16_t t85APU_calcS16 (t85APU * apu);
/**
 * @brief Calculates 1 sample and return its sample value mapped to unsigned 16-bit limits.
 * 
 * @param apu The t85APU instance.
 * @return The sample value, mapped from its raw value to 0..4294967295.
 */
uint32_t t85APU_calcU32 (t85APU * apu);
/**
 * @brief Calculates 1 sample and return its sample value mapped to unsigned 31-bit limits.
 * 
 * @param apu The t85APU instance.
 * @return The sample value, mapped from its raw value to 0..2147483647.
 */
int32_t t85APU_calcS32 (t85APU * apu);


/**
 * @brief Tells you whether the register write buffer has at least one write pending.
 * 
 * @param apu The t85APU instance.
 * @return true if there is at least one write pending.
 * @return false if there are no writes pending (aka the buffer is completely empty).
 */
bool t85APU_shiftRegisterPending (t85APU * apu);

/**
 * @brief Enables or disables channel muting in the total output of @c t85APU_calcXXX functions.
 * 
 * @param apu The t85APU instance to change the muting settings of.
 * @param channel The channel to mute/unmute.
 * @param mute The mute setting. @c true means to mute the channel, @c false means to unmute it.
 */
void t85APU_setMute(t85APU * apu, uint_fast8_t channel, bool mute);

#ifdef __cplusplus
}
#endif

#endif