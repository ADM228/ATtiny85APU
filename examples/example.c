#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "t85apu.h"
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

	//* But, how do we even calculate those mysterious pitch parameters?
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
	t85APU_writeReg(apu, PHIAB, bit(PR_SQ_A));
	
	// Now, the pulse generator is guaranteed to be in a high state, with an exception:
	//* Using 0% duty cycle is equivalent to disabling the tone entirely.
	t85APU_writeReg(apu, DUTYA, 0);
	// The output from this pulse generator is now 0 until
	// the duty cycle is set back to a non-zero value.

	// Let it simmer for a bit, as usual:
	writeFrames(5);

	// That was a demonstration of how the pulse works.
	// But that's not the only waveform that the t85APU can do -
	//* There is also a periodic noise generator.
	// Setting it up consists of several steps:
	// The first 2 are the same as in a pulse generator - the pitch:
	t85APU_writeReg(apu, PILON, 0x83);
	t85APU_writeReg(apu, PHIEN, PitchHi_Noise(0x7));
	// Then, we would disable the pulse for better experience, but we just did it
	// After that, you enable the noise on a channel via the CFG_X register:
	t85APU_writeReg(apu, CFG_A, bit(NOISE_EN)|Pan(3, 3));

	// And we have pure noise!

	// Let it simmer for half a sec, as usual:
	writeFrames(15);

	// Let's revisit the disabling of the pulse, more specifically its reasoning -
	//* The noise is OR'd with the pulse wave.
	// If we enable the pulse back, we might get some interesting effects -
	// like e.g. an open hihat. For that, let's set the noise to a high frequency,
	// and do the same with the pulse:
	t85APU_writeReg(apu, PILON, 0xFF);
	t85APU_writeReg(apu, PHIAB, PitchHi_Sq_A(0x6));
	t85APU_writeReg(apu, PILOA, 0xF9);
	// And enable the pulse back by un-zeroing the duty cycle:
	t85APU_writeReg(apu, DUTYA, 0x70);

	// And of course, let it simmer:
	writeFrames(15);

	// Now let's turn off the noise:
	t85APU_writeReg(apu, CFG_A, Pan(3, 3));

	// Make the pulse wave sound more normal:
	t85APU_writeReg(apu, PHIAB, PitchHi_Sq_A(0x4));
	t85APU_writeReg(apu, PILOA, 0x79);

	//* And learn about volume control!
	// There's 3 ways to set volume for a channel on the t85APU -
	//* The static volume, the panning volume, and (if enabled) the envelopes A and B
	// The static volume is the most straightforward - goes from 0 to 255:
	for (int i = 0; i <= 255; i++){
		t85APU_writeReg(apu, VOL_A, i);
		writeFrames(1);
	}

	// Then, the panning volume goes from 0 to 3, per channel per ear (stereo).
	// Currently the t85APU only outputs in mono, so only the left panning
	// bits are used. It is highly recommended to set the right panning to
	// the same value as the left panning for compatibility with future versions.
	for (int i = 3; i >= 0; i--){
		t85APU_writeReg(apu, CFG_A, Pan(i, i));
		writeFrames(6);
	}
	// Internally, those 2 values are multiplied, so they can be used together.

	// And finally, the envelopes. Yes, plural - there are 2 of them, and any one of
	// them can be used on any channel(s).
	//* Setting up the basic envelopes requires 4 register writes:
	// The envelope pitch increment (works the same way as the tone/noise):
	t85APU_writeReg(apu, EPLOA, 0x40);
	// The octave (unlike tone/noise, it goes from 0 to 15,
	// with values 8 through 15 corresponding to tone octaves 0 through 7)
	t85APU_writeReg(apu, EPIHI, PitchHi_Env_A(0x04));
	// Then the envelope shape (this is also where you do phase resets,
	// instead of the octave register):
	t85APU_writeReg(apu, E_SHP, bit(ENVA_HOLD)|bit(ENVA_RST));
	// A phase reset is required to start an envelope after resetting the chip
	// or finishing a non-repeating envelope.
	// And finally, enable the envelope on the channel (and specify its index):
	t85APU_writeReg(apu, CFG_A, bit(ENV_EN)|EnvNum(0)|Pan(3, 3));

	// The period of an envelope can be calculated by the following formula:
	// (1 << 23) / (increment << octave) / chipSampleRate,
	// where 23 is the width of the envelope phase accumulator (to accomodate
	// for the 16 octave range). Thus, the envelope that we just set up
	// has a period of (1 << 23) / (64 << 4) / (8000000 / 512) = 0.524288 seconds.
	writeFrames(32);

	// TODO: envelope shape description

	// Delete the APU at the end:
	t85APU_delete(apu);
	return 0;
}