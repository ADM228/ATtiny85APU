/* 
t85apu.hpp
Part of the ATtiny85APU emulation library
Written by alexmush
2024-2024
*/


#ifndef __cplusplus
#error "This is a C++ wrapper, meant only for C++"
#endif


#ifndef __T85APU_HPP__
#define __T85APU_HPP__

#include "t85apu.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

class t85APUHandle {
	public:
		/**
		 * @brief Constructs a new empty t85APUHandle object
		 * 
		 */
		t85APUHandle() { apu = nullptr; }
		#ifdef T85APU_SHIFT_REGISTER_SIZE
		/**
		 * @brief Constructs a new t85APUHandle object
		 * 
		 * @param clock The master clock speed of the t85APUHandle, in Hz. If not set (i.e. 0), will default to 8000000 - 8MHz. 
		 * @param rate The output sample rate of the t85APUHandle, in Hz. If not set (i.e. 0), will default to (clock / 512).
		 * @param outputType The output type of the t85APUHandle. Use the @c T85APU_OUTPUT_XXX defines in t85apu.h to select the output type.
		 */
		inline t85APUHandle(double clock, double rate, uint_fast8_t outputType) { apu = t85APU_new(clock, rate, outputType); }
		#else
		/**
		 * @brief Constructs a new t85APUHandle object
		 * 
		 * @param clock The master clock speed of the t85APUHandle, in Hz. If not set (i.e. 0), will default to 8000000 - 8MHz.
		 * @param rate The output sample rate of the t85APUHandle, in Hz. If not set (i.e. 0), will default to (clock / 512).
		 * @param outputType The output type of the t85APUHandle. Use the @c T85APU_OUTPUT_XXX defines in t85apu.h to select the output type.
		 * @param shiftRegisterSize The size of the register write buffer. Has to be at least 1.
		 */
		inline t85APUHandle(double clock, double rate, uint_fast8_t outputType, size_t shiftRegisterSize = 1) { apu = t85APU_new(clock, rate, outputType, shiftRegisterSize); }
		#endif
		/**
		 * @brief Destroy thet85APUHandle object
		 * 
		 */
		inline ~t85APUHandle() { t85APU_delete(apu); }

		/**
		 * @brief Resets all internal variables of the t85APU to their default initalization state - effectively the same as pulling the @c /RESET pin low on real hardware.
		 * @note This does NOT clear the register write buffer as it is considered emulated external hardware. This also does not reset the settings of the t85APU (clock speed, sample rate, output type, quality and muting).
		 * 
		 */
		inline void reset() { t85APU_reset(apu); }

		/**
		 * @brief Sets clock speed and sample rate of the t85APU.
		 * 
		 * @param clock The master clock speed of the t85APU, in Hz. If not set (i.e. 0), will default to 8000000 - 8MHz. 
		 * @param rate The output sample rate of the t85APU, in Hz. If not set (i.e. 0), will default to (clock / 512).
		 */
		inline void setClocknRate(double clock, double rate) { t85APU_setClocknRate(apu, clock, rate); }
		/**
		 * @brief Sets the output type of the t85APU.
		 * 
		 * @param outputType The output type of the t85APU. Use the @c T85APU_OUTPUT_XXX defines in t85 to select the output type.
		 */
		inline void setOutputType(uint_fast8_t outputType) { t85APU_setOutputType(apu, outputType); }
		/**
		 * @brief Sets the sample rate converter quality.
		 * 
		 * @param quality The quality setting
		 * @li 0 makes the immediate output of the t85APU the final output. Takes less CPU time, but has alialising issues.
		 * @li 1 averages all of the outputs in that tick and makes that the final output. Takes more CPU time, but doesn't have alialising issues. 
		 */
		inline void setQuality(uint_fast8_t quality) { t85APU_setQuality(apu, quality); }

		/**
		 * @brief Pushes data onto the register write buffer of the t85APU.
		 * 
		 * @param addr The register number to write to.
		 * @param data The data to write to the register.
		 */
		inline void writeReg(uint8_t addr, uint8_t data) {t85APU_writeReg(apu, addr, data); }

		/**
		 * @brief Calculates 1 sample and return its raw value.
		 * 
		 * @return The raw value of the sample, depends on the bitdepth of the output mode.
		 */
		inline uint32_t calc() { return t85APU_calc(apu); }
		/**
		 * @brief Calculates 1 sample and return its sample value mapped to unsigned 16-bit limits.
		 * 
		 * @return The sample value, mapped from its raw value to 0..65535.
		 */
		inline uint16_t calcU16 () { return t85APU_calcU16(apu); }
		/**
		 * @brief Calculates 1 sample and return its sample value mapped to unsigned 15-bit limits.
		 * 
		 * @return The sample value, mapped from its raw value to 0..32767.
		 */
		inline int16_t calcS16 () { return t85APU_calcS16(apu); }
		/**
		 * @brief Calculates 1 sample and return its sample value mapped to unsigned 16-bit limits.
		 * 
		 * @return The sample value, mapped from its raw value to 0..4294967295.
		 */
		inline uint32_t calcU32 () { return t85APU_calcU32(apu); }
		/**
		 * @brief Calculates 1 sample and return its sample value mapped to unsigned 31-bit limits.
		 * 
		 * @return The sample value, mapped from its raw value to 0..2147483647.
		 */
		inline int32_t calcS32 () { return t85APU_calcS32(apu); }

		/**
		 * @brief Tells you whether the register write buffer has at least one write pending.
		 * 
		 * @return true if there is at least one write pending.
		 * @return false if there are no writes pending (aka the buffer is completely empty).
		 */
		inline bool shiftRegisterPending() { return t85APU_shiftRegisterPending(apu); }

		/**
		 * @brief Enables or disables channel muting in the total output of @c t85APU_calcXXX functions.
		 * 
		 * @param channel The channel to mute/unmute.
		 * @param mute The mute setting. @c true means to mute the channel, @c false means to unmute it.
		 */
		inline void setMute(uint_fast8_t channel, bool mute) { return t85APU_setMute(apu, channel, mute); }

		/**
		 * @brief Provides raw access to the t85APU struct.
		 * 
		 * @return The reference to the t85APU struct.
		 */
		inline const t85APU & operator() () { return *apu; }

		/**
		 * @brief Copy constructor.
		 * 
		 * @param __apu The t85APU struct to copy the data of.
		 */
		inline void operator= (const t85APU * __apu) {
			this->apu = (t85APU *)calloc(1, sizeof(t85APU));
			if (!this->apu) {fprintf(stderr, "Could not allocate t85APU\n"); return;}
			memcpy(apu, __apu, sizeof(t85APU));
			#ifndef T85APU_SHIFT_REGISTER_SIZE
			apu->shiftRegister = (uint16_t *)calloc(__apu->shiftRegSize, sizeof(uint16_t));
			if (!apu->shiftRegister) {
				fprintf(stderr, "Could not allocate t85apu shift register, deleting the t85APU\n");
				if (apu->resamplingBuffer) free(apu->resamplingBuffer);
				free(apu);
				apu = nullptr;
				return;
			}
			memcpy(apu->shiftRegister, __apu->shiftRegister, apu->shiftRegSize);
			#endif
		}
		/**
		 * @brief Move constructor.
		 * 
		 * @param __apu The t85APU struct to swap the data with.
		 */
		inline void operator= (t85APU * & __apu) { auto tmp = this->apu; this->apu = __apu; __apu = tmp; } //move constructor
		/**
		 * @brief Move constructor.
		 * 
		 * @param __apu The t85APU struct to swap the data with.
		 */
		inline void operator= (t85APU && __apu) { auto tmp = this->apu; this->apu = &__apu; __apu = *tmp; } //move constructor
	private:
		/**
		 * @brief The t85APU struct powering all of this.
		 * 
		 */
		t85APU * apu;
};

#endif	// __T85APU_HPP__