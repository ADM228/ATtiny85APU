/* t85apu.hpp */
#ifndef __ATTINY85APU_HPP__
#define __ATTINY85APU_HPP__

#include "t85apu.h"

class t85APUHandle {
	public:
		t85APUHandle() { apu = nullptr; }
		inline t85APUHandle(double clock, double rate, uint_fast8_t outputType) { apu = t85APU_new(clock, rate, outputType); }
		inline ~t85APUHandle() { t85APU_delete(apu); }

		inline void reset() { t85APU_reset(apu); }

		inline void setClocknRate(double clock, double rate) { t85APU_setClocknRate(apu, clock, rate); }
		inline void setOutputType(uint_fast8_t outputType) { t85APU_setOutputType(apu, outputType); }
		inline void setQuality(uint_fast8_t quality) { t85APU_setQuality(apu, quality); }

		inline void writeReg(uint8_t addr, uint8_t data) {t85APU_writeReg(apu, addr, data); }

		inline uint32_t calc() { return t85APU_calc(apu); }

		inline bool shiftRegisterPending() { return t85APU_shiftRegisterPending(apu); }

		inline const t85APU * operator() () { return apu; }

		inline void operator= (t85APU * apu) { this->apu = apu; }

	private:
		t85APU * apu;
};

#endif	// __ATTINY85APU_HPP__