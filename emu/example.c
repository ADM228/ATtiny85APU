#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "libt85apu/t85apu.h"
#include "libt85apu/t85apu_regdefines.h"

#define clockSpeed 8000000
#define sampleRate 44100
#define frameRate 60
#define samplesPerFrame (sampleRate/frameRate)

#define clamp(var, min, max) { \
	if (var < min) var = min; \
	if (var > max) var = max; \
}

int16_t sampleBuffer[samplesPerFrame];
t85APU * apu;
FILE * file;

void writeFrames(unsigned int frames) {
	for (unsigned int frame = 0; frame < frames; frame++) {
		for (int j = 0; j < samplesPerFrame; j++) {
			sampleBuffer[j] = t85APU_calcS16(apu);
		}
		fwrite(sampleBuffer, sizeof(sampleBuffer), 1, file);
	}
}

int main (int argc, char ** argv) {
	// Open the file:
	if (argc < 2) {
		fprintf(stderr, "Usage: example_c <output file>\n");
		return 1;
	}
	file = fopen(argv[1], "wb");
	if (!file) {
		fprintf(stderr, "Failed to open file '%s'!\n", argv[1]);
		return 2;
	}

	// Initialize the APU:
	apu = t85APU_new(clockSpeed, sampleRate, 0, 16);

	//* Basic initalization of a pulse channel involves 4 steps:
	// The pitch increment,
	t85APU_writeReg(apu, PILOA, 0x88);
	// The octave (technically optional),
	t85APU_writeReg(apu, PHIAB, PitchHi_Sq_A(0x3));
	// The duty cycle,
	t85APU_writeReg(apu, DUTYA, 0x40);	// 0x40 / 0x100 = 25% duty cycle
	// And the channel volume.
	t85APU_writeReg(apu, VOL_A, 0xFF);	// 0xFF / 0xFF = 100% volume
	
	//* Let this simmer for a half a sec.
	writeFrames(30);

	//* But, how do you even calculate those mysterious pitch parameters?
	// Essentially, the octave shifts the pitch increment to create a
	// 15-bit total period value (the omitted bit is the lowest).
	// This period value then feeds a phase accumulator to produce the sound.

	//* First, we have to calculate the total 15-bit period.
	// This may either be directly calculated, or converted in some way
	// from other measurements. If we are converting, we will need to know
	// the sample rate of the chip, which is the chip's clock divided by 512.
	double chipSampleRate = clockSpeed / 512.0;

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
	t85APU_writeReg(apu, PILOA, increment);
	t85APU_writeReg(apu, PHIAB, PitchHi_Sq_A(octave));

	// And let it simmer for another half a sec:
	writeFrames(30);

	//* To completely stop the phase accumulator, write 0 as the pitch increment value:
	t85APU_writeReg(apu, PILOA, 0);
	// The octave does not matter, as 0 × anything is still 0.

	// But, it is unknown whether it is low or high at the moment.
	//* This is where phase resets come into play.
	// It is performed by writing to bits 3 and/or 7 of the octave registers:
	t85APU_writeReg(apu, PHIAB, 1<<PR_SQ_A);
	
	// Now, the pulse generator is guaranteed to be in a high state, with an exception:
	//* Using 0% duty cycle is equivalent to disabling the tone entirely.
	t85APU_writeReg(apu, DUTYA, 0);
	// The output from this pulse generator is now 0 until
	// the duty cycle is set back to a non-zero value.

	// Let it simmer for a bit, as usual:
	writeFrames(5);

	// TODO: noise tutorial
	// TODO: envelope tutorial

	// Delete the APU at the end:
	t85APU_delete(apu);
	return 0;
}