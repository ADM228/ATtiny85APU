/* 
t85apu.hpp
Part of the ATtiny85APU emulation library
Written by alexmush
2024-2024
*/


#ifndef __cplusplus
#error "This is a C++ wrapper, meant only for C++"
#endif


#ifndef __ATTINY85APU_HPP__
#define __ATTINY85APU_HPP__

#include "t85apu.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

class t85APUHandle {
	public:
		t85APUHandle() { apu = nullptr; }
		#ifdef T85APU_SHIFT_REGISTER_SIZE
		inline t85APUHandle(double clock, double rate, uint_fast8_t outputType) { apu = t85APU_new(clock, rate, outputType); }
		#else
		inline t85APUHandle(double clock, double rate, uint_fast8_t outputType, size_t shiftRegisterSize = 1) { apu = t85APU_new(clock, rate, outputType, shiftRegisterSize); }
		#endif
		inline ~t85APUHandle() { t85APU_delete(apu); }

		inline void reset() { t85APU_reset(apu); }

		inline void setClocknRate(double clock, double rate) { t85APU_setClocknRate(apu, clock, rate); }
		inline void setOutputType(uint_fast8_t outputType) { t85APU_setOutputType(apu, outputType); }
		inline void setQuality(uint_fast8_t quality) { t85APU_setQuality(apu, quality); }

		inline void writeReg(uint8_t addr, uint8_t data) {t85APU_writeReg(apu, addr, data); }

		inline uint32_t calc() { return t85APU_calc(apu); }

		inline bool shiftRegisterPending() { return t85APU_shiftRegisterPending(apu); }

		inline const t85APU & operator() () { return *apu; }

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
		}	// copy constructor
		inline void operator= (t85APU * & __apu) { auto tmp = this->apu; this->apu = __apu; __apu = tmp; } //move constructor
		inline void operator= (t85APU && __apu) { auto tmp = this->apu; this->apu = &__apu; __apu = *tmp; } //move constructor
	private:
		t85APU * apu;
};

#endif	// __ATTINY85APU_HPP__