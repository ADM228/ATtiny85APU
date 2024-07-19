/*
	t85APU usage example (C++ edition)
	© alexmush, 2024
	Please scroll down to int main() to start reading the tutorial.
*/

#include <cstdint>
#include <ios>
#include <iostream>
#include <fstream>
#include <cmath>

#include "t85apu.hpp"
#include "t85apu_regdefines.h"

#define clockSpeed 8000000
#define sampleRate 44100
#define frameRate 60
#define samplesPerFrame (sampleRate/frameRate)

#define clamp(var, min, max) { \
	if (var < min) var = min; \
	if (var > max) var = max; \
}

int16_t sampleBuffer[samplesPerFrame];
t85APUHandle apu;
std::fstream file;

void writeFrames(unsigned int frames) {
	for (unsigned int frame = 0; frame < frames; frame++) {
		for (int j = 0; j < samplesPerFrame; j++) {
			sampleBuffer[j] = apu.calcS16();
			// Yields 15-bit values, perfect for signed short buffers
		}
		file.write((const char *)sampleBuffer, sizeof(sampleBuffer));
	}
}

int main (int argc, char ** argv) {
	//* Welcome to the t85APU usage tutorial.
	// This tutorial shows some examples on how to use the ATtiny85APU soundchip,
	// as well as some of the emulator API. I hope you find this understandable!
	// Let's begin!

	// Open the file:
	if (argc < 2) {
		std::cerr << "Usage: example_cpp <output file>" << std::endl;
		return 1;
	}
	file.open(argv[1], std::ios_base::out|std::ios_base::binary);
	if (!file.is_open()) {
		std::cerr << "Failed to open file " << argv[1] << "!" << std::endl;
		return 2;
	}

	// Initialize the APU:
	apu = t85APUHandle(clockSpeed, sampleRate, 0, 16);

	//* Basic initalization of a pulse channel involves 4 steps:
	// The pitch increment,
	apu.writeReg(PILOA, 0x88);
	// The octave (technically optional),
	apu.writeReg(PHIAB, PitchHi_Sq_A(0x3));
	// The duty cycle,
	apu.writeReg(DUTYA, 0x40);	// 0x40 / 0x100 = 25% duty cycle
	// And the channel volume.
	apu.writeReg(VOL_A, 0xFF);	// 0xFF / 0xFF = 100% volume
	
	//* Let this simmer for a bit.
	writeFrames(20);

	//* But, how do we even calculate those mysterious pitch parameters?
	// Essentially, the octave shifts the pitch increment to create a
	// 15-bit total period value (the omitted bit is the lowest).
	// This period value then feeds a phase accumulator to produce the sound.

	//* First, we have to calculate the total 15-bit period.
	// This may either be directly calculated, or converted in some way
	// from other measurements. If we are converting, we will need to know
	// the sample rate of the chip, which is the chip's clock divided by 512.
	double chipSampleRate = clockSpeed / 512.0;
	//* Note: the t85APU struct does not store the clock speed internally.

	// And here's the formula for converting from Hz into APU period:
	double frequency = 440.0;	// In Hertz
	double period = frequency / chipSampleRate * (1<<15);
	// 15 being the tone period width.

	// Protect it from overflows just in case:
	clamp(period, 0, (1<<15)-1);	// (1<<15)-1 being the maximum value held in 15 bits

	//* Next, the tone increment depends on the octave, so let's pick the best one.
	// By "best", I mean the octave where the used pitch range has the most resolution,
	// while not overflowing the 15-bit limit. This calculation is actually quite tricky
	// to pinpoint as exact math, and here it is:
	int octave = floor(log2(period) - 7); 
	// Looks pretty complicated and littered with magic numbers, I know. Here's an explanation:
	// log2(period) calculates the number of the most significant set bit,
	// but the octave number only shifts the number 0..7 times, while the log2
	// goes up to 15. This means that octave 0 will have to account for values
	// 0..127, and only then increment - that's the lower 7 bits not counting.
	
	// Of course, octaves cannot be negative, so let's clamp it:
	clamp(octave, 0, 7);

	//* And finally, let's calculate the pitch increment:
	int increment = round(period / pow(2, octave));
	// And clamp it (and round to the next octave if possible/necessary):
	if (increment > UINT8_MAX && octave < 7) {
		octave++;
		increment /= 2.0;
	}
	// And clamp it again for good measure:
	clamp(increment, 0, UINT8_MAX);

	//* Finally, let's write the data to the actual registers:
	apu.writeReg(PILOA, increment);
	apu.writeReg(PHIAB, PitchHi_Sq_A(octave));

	// And let it simmer for a bit:
	writeFrames(20);

	//* To completely stop the phase accumulator, write 0 as the pitch increment value:
	apu.writeReg(PILOA, 0);
	// The octave does not matter, as 0 × anything is still 0.

	// But, it is unknown whether it is low or high at the moment.
	//* This is where phase resets come into play.
	// It is performed by writing to bits 3 and/or 7 of the octave registers:
	apu.writeReg(PHIAB, bit(PR_SQ_A));
	
	// Now, the pulse generator is guaranteed to be in a high state, with an exception:
	//* Using 0% duty cycle is equivalent to disabling the tone entirely.
	apu.writeReg(DUTYA, 0);
	// The output from this pulse generator is now 0 until
	// the duty cycle is set back to a non-zero value.

	// Let it simmer for a bit, as usual:
	writeFrames(5);

	// That was a demonstration of how the pulse works.
	// But that's not the only waveform that the t85APU can do -
	//* There is also a periodic noise generator.
	// Setting it up consists of several steps:
	// The first 2 are the same as in a pulse generator - the pitch:
	apu.writeReg(PILON, 0x83);
	apu.writeReg(PHIEN, PitchHi_Noise(0x7));
	// Then, we would disable the pulse for better experience, but we just did it
	// After that, you enable the noise on a channel via the CFG_X register:
	apu.writeReg(CFG_A, bit(NOISE_EN)|Pan(3, 3));

	// And we have pure noise!

	// Let it simmer for some time, as usual:
	writeFrames(15);
	
	// Let's revisit the disabling of the pulse, more specifically its reasoning -
	//* The noise is OR'd with the pulse wave.
	// If we enable the pulse back, we might get some interesting effects -
	// like e.g. an open hihat. For that, let's set the noise to a high frequency,
	// and do the same with the pulse:
	apu.writeReg(PILON, 0xFF);
	apu.writeReg(PHIAB, PitchHi_Sq_A(0x6));
	apu.writeReg(PILOA, 0xF9);
	// And enable the pulse back by un-zeroing the duty cycle:
	apu.writeReg(DUTYA, 0x70);

	// And of course, let it simmer:
	writeFrames(15);

	// Now let's turn off the noise:
	apu.writeReg(CFG_A, Pan(3, 3));

	// Make the pulse wave sound more normal:
	apu.writeReg(PHIAB, PitchHi_Sq_A(0x4));
	apu.writeReg(PILOA, 0x79);

	//* And learn about volume control!
	// There's 3 ways to set volume for a channel on the t85APU -
	//* The static volume, the panning volume, and (if enabled) the envelopes A and B
	// The static volume is the most straightforward - goes from 0 to 255:
	for (int i = 0; i <= 255; i++){
		apu.writeReg(VOL_A, i);
		writeFrames(1);
	}

	// Then, the panning volume goes from 0 to 3, per channel per ear (stereo).
	// Currently the t85APU only outputs in mono, so only the left panning
	// bits are used. It is highly recommended to set the right panning to
	// the same value as the left panning for compatibility with future versions.
	for (int i = 3; i >= 0; i--){
		apu.writeReg(CFG_A, Pan(i, i));
		writeFrames(6);
	}
	// Internally, those 2 values are multiplied, so they can be used together.

	// And finally, the envelopes. Yes, plural - there are 2 of them, and any one of
	// them can be used on any channel(s).
	//* Setting up the basic envelopes requires 4 register writes:
	// The envelope pitch increment (works the same way as the tone/noise):
	apu.writeReg(EPLOA, 0x40);
	// The octave (unlike tone/noise, it goes from 0 to 15,
	// with values 8 through 15 corresponding to tone octaves 0 through 7)
	apu.writeReg(EPIHI, PitchHi_Env_A(0x04));
	// Then the envelope shape (this is also where you do phase resets,
	// instead of the octave register):
	apu.writeReg(E_SHP, bit(ENVA_HOLD)|bit(ENVA_RST));
	// A phase reset is required to start an envelope after resetting the chip
	// or finishing a non-repeating envelope.
	// And finally, enable the envelope on the channel (and specify its index):
	apu.writeReg(CFG_A, bit(ENV_EN)|EnvNum(0)|Pan(3, 3));

	// The period of an envelope can be calculated by the following formula:
	// (1 << 23) / (increment << octave) / chipSampleRate,
	// where 23 is the width of the envelope phase accumulator (to accomodate
	// for the 16 octave range). Thus, the envelope that we just set up
	// has a period of (1 << 23) / (64 << 4) / (8000000 / 512) = 0.524288 seconds.
	writeFrames(32);

	//* There are a total of 8 envelope shapes, and they are bit-compatible with the AY-3-8910:
	// Num	|  ATT	|  ALT	|  HOLD	| 	Shape	|
	// 	0	|	0	|	0	|	0	|  \ \ \ \ 	|
	// _____|_______|_______|_______|_  \ \ \ \	|
	// 	1	|	0	|	0	|	1	|  \     	|
	// _____|_______|_______|_______|_  \______	|
	// 	2	|	0	|	1	|	0	|  \  /\  /	|
	// _____|_______|_______|_______|_  \/  \/	|
	// 	3	|	0	|	1	|	1	|  \ |¯¯¯¯¯	|
	// _____|_______|_______|_______|_  \|		|
	// 	4	|	1	|	0	|	0	|   / / / /	|
	// _____|_______|_______|_______|_ / / / /	|
	// 	5	|	1	|	0	|	1	|   /¯¯¯¯¯¯	|
	// _____|_______|_______|_______|_ /		|
	// 	6	|	1	|	1	|	0	|   /\  /\	|
	// _____|_______|_______|_______|_ /  \/  \	|
	// 	7	|	1	|	1	|	1	|   /|		|
	// _____|_______|_______|_______|_ / |_____	|
	// The phase reset bits (bit 3 for envelope A and 7 for envelope B) don't just simply
	// reset the envelope phase to 0 -
	//* they resets the envelopes' phase to the state of the envelope load registers.
	// They are set to 0 on chip reset, so by default there is no need to initialize them.
	// E.g. if you want to start the envelope at 25% of its way through a slope, you do this:
	apu.writeReg(ELDHI, 0x40);
	//* Note that on looping envelopes it will loop right back to the maximum point.
	// And then write the envelope shape:
	apu.writeReg(E_SHP, bit(ENVA_RST));
	
	//* You can also use the envelopes for melodic purposes with the higher 8 octaves.
	// The pitch is calculated the same way it is for tone, just replace 
	// the tone phase accumulator width of 15 bits with the envelope width of 23:
	frequency = 261.625;	// In Hertz
	period = frequency / chipSampleRate * (1<<23);
	clamp(period, 0, (1<<23)-1);
	octave = floor(log2(period) - 7); //! This still stays at 7 as it stems from the octave 0
	clamp(octave, 0, 15);	// Update the limit here to 15
	increment = round(period / pow(2, octave));
	if (increment > UINT8_MAX && octave < 15) {	// And here as well
		octave++;
		increment /= 2.0;
	}
	clamp(increment, 0, UINT8_MAX);
	// And write the pitch:
	apu.writeReg(EPLOA, increment);
	apu.writeReg(EPIHI, PitchHi_Env_A(octave));

	// Let it simmer for a bit:
	writeFrames(10);

	//* Another feature of envelopes is being able to halve their volume on a channel.
	// This is done with the channel's static register's most significant bit -
	// Values ≥ 0x80 will keep the envelope at full volume,
	// Values ≤ 0x7F will halve it:
	apu.writeReg(VOL_A, 0x7F);
	// This, combined with the panning bits,
	// gives the envelopes a total of 6 effective volume levels on each channel.

	// Let it simmer for a bit:
	writeFrames(10);

	// The repeating triangle envelopes (ALT being 1 and HOLD being 0) consist of
	// 2 slopes instead of 1,
	//* which effectively halves their frequency:
	apu.writeReg(E_SHP, bit(ENVA_ALT)); 

	// Let it simmer for a bit:
	writeFrames(10);

	// Since the envelope acts effectively as the volume curve,
	//* To disable tone but keep the envelope playing you need to set the duty cycle to not 0:
	apu.writeReg(PILOA, 0);		// Disable the tone
	apu.writeReg(PHIAB, bit(PR_SQ_A));	// Phase reset needed
	apu.writeReg(DUTYA, 0x40);	// Any non-0 value will do

	// Let it simmer for a bit:
	writeFrames(10);

	//* On the contrary, to enable the envelope with noise (but not tone), set the duty cycle to 0:
	apu.writeReg(DUTYA, 0);
	// And enable both the envelope and noise obviously:
	apu.writeReg(CFG_A, bit(NOISE_EN)|bit(ENV_EN)|EnvNum(0)|Pan(3, 3));

	// Let it simmer for a bit:
	writeFrames(10);

	// Let's disable the envelope for a future comparison:
	apu.writeReg(CFG_A, bit(NOISE_EN)|Pan(3, 3));

	// And let it simmer for a bit:
	writeFrames(20);

	//* One of the more advanced things to do on this chip is setting the noise tap values.
	// The default noise tap value is 0x2400, which makes it somewhat sound
	// like a white noise generator. Different sounding pseudorandom 1-bit noise
	// can be achieved by changing the value.
	// To change the value just write to one of the two NTPXX registers, and it
	// will be updated immediately.
	//* Note that the LFSR contents are not reset by this,
	// nor are they reset by the phase reset bit.
	apu.writeReg(NTPLO, 0x3B);	// Now the value is 0x243B
	// Now the noise sounds somewhat different.

	// And let it simmer for a bit:
	writeFrames(30);

	// That's pretty much it for the chip capabilities in the ATtiny85APU v1.0.
	// Deleting the APU happens automatically in the destructor.

	//* Thank you for reading this tutorial on t85APU usage.
	// Please contact me (links are in my GitHub profile) for any help using this chip in
	// your project. Good luck!
	return 0;
}