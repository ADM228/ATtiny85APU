#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cwchar>

#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <list>
#include <string>

#include <sndfile.hh>
#include <soundio/soundio.h>
#include <thread>

#include "ATtiny85APU.h"
#include "BitConverter.cpp"


static const uint32_t currentFileVersion = 0x00000000;	// 16.8.8 bits semantic versioning
static const uint32_t currentGd3Version = 0x00000100;

t85APU * apu;

std::filesystem::path inFilePath;
bool inFileDefined = false;
std::ifstream regDumpFile;

std::filesystem::path outFilePath;
bool outFileDefined = false;

uint32_t sampleRate = 44100;
bool sampleRateDefined = false;

bool notchFilter = false;

uint8_t outputMethod;
bool outputMethodOverride = false;

double ticksPerSample;

double sampleTickCounter = 0.0;

uint_fast16_t waitTimeCounter = 0;

uint32_t regDataLocation, gd3DataLocation, loopOffset, extraHeaderOffset;
uint32_t apuClock, totalSmpCount, loopLength;

char buffer[4];

bool ended = false;

std::list<uint16_t> regWrites;

// utility wrapper to adapt locale-bound facets for wstring/wbuffer convert
template<class Facet>
struct deletable_facet : Facet
{
    template<class... Args>
    deletable_facet(Args&&... args) : Facet(std::forward<Args>(args)...) {}
    ~deletable_facet() {}
};

std::wstring_convert<deletable_facet<std::codecvt<char16_t, char, std::mbstate_t>>, char16_t> utf16Converter;

// t85 format is VGM-inspired:

/*	 _______________________________________________________________________
	|		|	0	|	1	|	2	|	3	|	4	|	5	|	6	|	7	|
	|=======|=======|=======|=======|=======|=======|=======|=======|=======|
	|  0x00 |		"t85!" ID string		|		End of file offset	 	|
	|  0x08	|		  File version			|		APU clock speed			|
	|  0x10	|		VGM data offset			|			GD3 offset			|
	|  0x18	|	Total number of samples		|			Loop offset			|
	|  0x20	|	Amount of looping samples	|	  Extra header offset		|
	|  0x28	|OutMthd|	**   RESERVED   **	|_______________________________|
	|_______|_______________________________|



*/

//	Notch filter

static const double PI = 3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679;

void filter(const double *x, double *y, size_t n, uint32_t smpRate)
{
	static const double bw = 0.001;

	const double f = 15625.f / smpRate; 
	const double cospi2f2 = std::cos(2 * PI * f) * 2;
	const double r = 1 - 3 * bw;
	const double k = (double)(1 - cospi2f2*r + r*r) / (double)(2 - cospi2f2);

	const double a0 = k, a2 = k;
	const double a1 = -1*k*cospi2f2;
	const double b1 = r*cospi2f2;
	const double b2 = -(r*r);

    static double x_2 = 0.0f;                    // delayed x, y samples
    static double x_1 = 0.0f;
    static double y_2 = 0.0f;
    static double y_1 = 0.0f;

    for (size_t i = 0; i < n; i++)
    {
        y[i] = a0 * x[i] + a1 * x_1 + a2 * x_2  // IIR difference equation
                         + b1 * y_1 + b2 * y_2;
        x_2 = x_1;                              // shift delayed x, y samples
        x_1 = x[i];
        y_2 = y_1;
        y_1 = y[i];
    }
}

/*
a0 = K
a1 = -2*K*cos(2*pi*f)
a2 = K
b1 = 2*R*cos(2*pi*f)
b2 = -(R*R)

Where:

K = (1 - 2*R*cos(2*pi*f) + R*R) / (2 - 2*cos(2*pi*f))
R = 1 - 3BW

f = center freq
BW = bandwidth (≥ 0.0003 recommended)
Both expressed as a multiple of the sample rate (must be > 0 && ≤ 0.5)

Sources: 
1. https://dsp.stackexchange.com/questions/11290/linear-phase-notch-filter-band-reject-filter-implementation-in-c
2. https://dspguide.com/ch19/3.htm

Enable it optionally, f = 15625Hz
*/

// The GD3 data
typedef struct __gd3 {
	std::string trackNameEnglish;
	std::string trackNameOG;
	std::string gameNameEnglish;
	std::string gameNameOG;
	std::string systemNameEnglish;
	std::string systemNameOG;
	std::string authorNameEnglish;
	std::string authorNameOG;
	std::string releaseDate;
	std::string vgmDumper;
	std::string notes;
} gd3;

static const std::string gd3Errors[] = {
	"",
	"Gd3 ID does not match",
	"Gd3 version too new",
	"Gd3 size too small",
	"Gd3 size outside of file bounds",
	"Too few strings"
};

gd3 * readGd3Data(std::ifstream & file, uint32_t fileSize, uint32_t gd3Offset, int & errCode) {
	errCode = 0;
	file.seekg(gd3Offset, std::ios_base::beg);
	file.read(buffer, 4);
	if (memcmp(buffer, "Gd3 ", 4)) { errCode = 1; return nullptr; }
	file.read(buffer, 4);
	if (BitConverter::readUint32(buffer) > currentGd3Version) { errCode = 2; return nullptr; }
	file.read(buffer, 4);
	uint32_t gd3Size = BitConverter::readUint32(buffer);
	if (gd3Size < 11 * 2) { errCode = 3; return nullptr; }
	if (gd3Size + gd3Offset + 0x0C > fileSize) { errCode = 4; return nullptr; }
	char * dataBuffer = new char[gd3Size];
	file.read(dataBuffer, gd3Size);
	// Check for enough spaces
	size_t amountOfNulls = 0;
	for (size_t i = 0; i < gd3Size>>1; i++) {
		if (*((char16_t *)dataBuffer+i) == 0) amountOfNulls++;
	}
	if (amountOfNulls < 11) { errCode = 5; return nullptr; }

	// Actually convert the mfs
	gd3 * output = new gd3();
	std::u16string bufString;
	char16_t * currentData = (char16_t *)dataBuffer;


	for (int i = 0; i < 11; i++) {
		bufString = std::u16string(currentData);
		currentData += bufString.size()+1;
		*(((std::string *)output)+i) = utf16Converter.to_bytes(bufString);
	}

	delete [] dataBuffer;

	return output;
}

gd3 * gd3Data;

void emulationTick(std::ifstream & file) {
	totalSmpCount--;
	if (waitTimeCounter) waitTimeCounter--;
	else {
		uint8_t cmd = file.get();
		// std::cout << (int)cmd << std::endl;
		switch(cmd) {
			case 0x41:
				file.read(buffer, 2);
				regWrites.push_back(BitConverter::readUint16(buffer));
				break;
			case 0x61:
				file.read(buffer, 2);
				waitTimeCounter = BitConverter::readUint16(buffer)-1;
				break;
			case 0x62:
				waitTimeCounter = 735-1;
				break;
			case 0x63:
				waitTimeCounter = 882-1;
				break;
			case 0x66:
			default:
				break;
		}
	}
}

#pragma region libsoundioUtils

static void write_callback(struct SoundIoOutStream *outstream, int frame_count_min, int frame_count_max) {
	// std::cout << "wcall" << std::endl;
    struct SoundIoChannelArea *areas;
    int err;
    int frames_left = std::max(std::min(int32_t((frame_count_max / ticksPerSample) - 1), (int32_t)totalSmpCount), (int32_t)frame_count_min);
	// std::cout << frames_left << " " << totalSmpCount << " " << frame_count_max << std::endl;
	if (!frames_left) {ended = true; return;}
    for (;;) {
        if ((err = soundio_outstream_begin_write(outstream, &areas, &frames_left))) {
            fprintf(stderr, "unrecoverable stream error: %s\n", soundio_strerror(err));
            exit(1);
        }
		int frame_count = frames_left;

        if (!frame_count)
            break;
		
        const struct SoundIoChannelLayout *layout = &outstream->layout;
		
		for (uint32_t frame = 0; frame < frame_count; frame++) {
			emulationTick(regDumpFile);	// TODO: this is bs, just do a buffer
			sampleTickCounter += ticksPerSample;
			while (sampleTickCounter >= 1.0) {
				// std::cout << sampleTickCounter << "  " << regWrites.size() << std::endl;
				sampleTickCounter -= 1.0;
				if (!t85APU_shiftRegisterPending(apu) && regWrites.size()) {
					t85APU_writeReg(apu, regWrites.front()&0xFF, regWrites.front()>>8);
					// std::cout << frames_left << " - WR: " << std::hex << (regWrites.front()>>8) << "->" << (regWrites.front()&0xFF) << std::dec << std::endl;
					regWrites.pop_front();
				} 
				uint16_t sample = (uint16_t)(t85APU_calc(apu)<<(15-apu->outputBitdepth));
				for (int channel = 0; channel < layout->channel_count; channel++) {
					BitConverter::writeBytes(areas[channel].ptr, sample);
					areas[channel].ptr += areas[channel].step;
				}
			}
		}
        if ((err = soundio_outstream_end_write(outstream))) {
            if (err == SoundIoErrorUnderflow)
                return;
            fprintf(stderr, "unrecoverable stream error: %s\n", soundio_strerror(err));
            exit(1);
        }
		frames_left -= frame_count;
        if (frames_left <= 0)
            break;
    }
    soundio_outstream_pause(outstream, false);
}

void underflow_callback(struct SoundIoOutStream *outstream) {
	static int count = 0;
	std::cerr << "Underflow #" << count++ << std::endl;
}

#pragma endregion

int main (int argc, char** argv) {
	std::cout << "ATtiny85APU register dump player v0.1" << std::endl << "© alexmush, 2024" << std::endl << std::endl;
	for (int i = 1; i < argc; i++) {	// Check for the help command specifically
		if (!(
			memcmp(argv[i], "-h", 3) &&
			memcmp(argv[i], "--help", 2+4+1))) {
                  std::cout << 
R"(Command-line options:
-i <input file> - specify a .t85 register dump to read from
	= --input
-o <output file> - specify a .wav file to output to
	= --output
	If not specified, the audio will be played back live
-s <rate> - specify audio output sample rate in Hz
	= --sample-rate
	Default value: 44100
	Note: the emulation rate always remains at 44100Hz
-f - enable 15625Hz notch filter
	= --filter
-nf - disable 15625Hz notch filter
	= --no-filter
	Default value:
		Enabled when output mode == 1
		Disabled if output mode != 1
		Cannot be enabled if sample rate ≤ 31250Hz
-om <mode> - force output mode
	= --output-method
	Default value: whatever is specified in the register dump file
-h - show this help screen
	= --help)"
                            << std::endl;
                  std::exit(0);
		}
	}
	for (int i = 1; i < argc; i++) {
		if (! (
			memcmp(argv[i], "-i", 3) && 
			memcmp(argv[i], "--input", 8))) {
			if (inFileDefined) {
				std::cerr << "Cannot input more than 1 file at once" << std::endl;
			} else if (i+1 >= argc) {
				std::cerr << "File argument not supplied" << std::endl;
			} else if (argv[i+1][0] == char("-"[0])){
				std::cerr << "Invalid file argument: \"" << argv[i+1] << "\"" << std::endl;
				i--;
			} else {
				inFilePath = std::filesystem::path(argv[i+1]);
				if (!std::filesystem::exists(inFilePath)) {
					std::cerr << "File \"" << argv[i+1] << "\" does not exist" << std::endl;
				} else {
					inFileDefined = true;
				}
			}
			i++;
		} else if (! (
			memcmp(argv[i], "-o", 3) && 
			memcmp(argv[i], "--output", 9))) {
			if (outFileDefined) {
				std::cerr << "Cannot output to more than 1 file at once" << std::endl;
			} else if (i+1 >= argc) {
				std::cerr << "File argument not supplied" << std::endl;
			} else if (argv[i+1][0] == char("-"[0])){
				std::cerr << "Invalid file argument: \"" << argv[i+1] << "\"" << std::endl;
				i--;
			} else {
				outFilePath = std::filesystem::path(argv[i+1]);
				if (std::filesystem::exists(outFilePath.parent_path())) {
					std::cerr << "Directory \"" << outFilePath.parent_path() << "\" does not exist" << std::endl;
				} else {
					outFileDefined = true;
				}
			}
			i++;
		} else if (!(
			memcmp(argv[i], "-s", 3) && 
			memcmp(argv[i], "--sample-rate", 2+6+1+4+1))) {
			// Sample rate
			if (i+1 >= argc) {
				std::cerr << "Sample rate argument not supplied" << std::endl;
			} else {
				char* p;
				auto tmp = strtol(argv[i+1], &p, 10);
				if (*p) {
					std::cerr << "Invalid sample rate argument: \"" << argv[i+1] << "\"" << std::endl;
				} else {
					sampleRate = tmp;
					sampleRateDefined = true;
				}
			}
			i++;
		} else if (!(
			memcmp(argv[i], "-om", 4) &&
			memcmp(argv[i], "--output-method", 2+6+1+6+1))) {
			// Override output mode
			if (i+1 >= argc) {
				std::cout << "Output method argument not supplied" << std::endl;
			} else {
				char* p;
				auto tmp = strtol(argv[i+1], &p, 10);
				if (*p) {
					std::cerr << "Invalid output method argument: \"" << argv[i+1] << "\"" << std::endl;
				} else {
					outputMethod = tmp;
					outputMethodOverride = true;
				}
			}
			i++;
		} else if (!(
			memcmp(argv[i], "-f", 3) && 
			memcmp(argv[i], "--filter", 2+6+1))) {
			// Enable notch filter
			notchFilter = true;
		} else if (!(
			memcmp(argv[i], "-nf", 3) && 
			memcmp(argv[i], "--no-filter", 2+2+1+6+1))) {
			// Disable notch filter
			notchFilter = false;
		}
	}
	if (!inFileDefined) {
		std::string filename;
		std::cout << "\tPlease input register dump file name: ";
		std::cin >> filename;
		inFilePath = std::filesystem::path(filename);
		if (!std::filesystem::exists(inFilePath)) {
			std::cerr << "File \"" << filename << "\" does not exist" << std::endl;
			std::exit(1);
		} else {
			inFileDefined = true;
		}
	}

	auto expectedFileSize = std::filesystem::file_size(inFilePath);
	regDumpFile.open(inFilePath, std::ios_base::binary|std::ios_base::in);

	// Read header
	regDumpFile.read(buffer, 4);
	if (memcmp(buffer, "t85!", 4)) {
		std::cerr << "File header does not match" << std::endl;
		std::exit(2);
	}

	// File size
	regDumpFile.read(buffer, 4);
	uint32_t fileSize = BitConverter::readUint32(buffer) + 0x04;
	if (fileSize > expectedFileSize) {
		std::cerr << "File seems to be cut off by " << fileSize - expectedFileSize << " bytes. " << std::endl;
		std::exit(3);
	} else if (fileSize < expectedFileSize) {
		std::cerr << "File seems to contain " << expectedFileSize - fileSize << " extra bytes of data. " << std::endl;
	}
	if (fileSize < 0x2C) {
		std::cerr << "File is too small for the t85 header" << std::endl;
		std::exit(2);
	}

	// File version
	regDumpFile.read(buffer, 4);
	uint32_t fileVersion = BitConverter::readUint32(buffer);
	if (fileVersion > currentFileVersion) {
		std::cerr << "File version (" << 
			(fileVersion>>16) << "." << 
			((fileVersion>>8) & 0xFF) << "." << 
			(fileVersion & 0xFF) << ") is newer than the player's maximum supported file version (" << 
			(currentFileVersion>>16) << "." << 
			((currentFileVersion>>8) & 0xFF) << "." << 
			(currentFileVersion & 0xFF) << "), please update the player at https://github.com/ADM228/ATtiny85APU" << std::endl;
		std::exit(4);
	}

	// APU clock speed
	regDumpFile.read(buffer, 4);
	apuClock = BitConverter::readUint32(buffer);
	if (apuClock == 0) {
		std::cout << "APU Clock speed not specified, assuming 8MHz." << std::endl;
		apuClock = 8000000;
	}

	// VGM data offset
	regDumpFile.read(buffer, 4);
	regDataLocation = BitConverter::readUint32(buffer);
	if (regDataLocation) {
		regDataLocation += 0x10;	// Offset after all
		if (regDataLocation > fileSize) {
			regDataLocation = 0;
			std::cerr << "Register dump pointer out of bounds." << std::endl;
		}
	}

	// GD3 data offset
	regDumpFile.read(buffer, 4);
	gd3DataLocation = BitConverter::readUint32(buffer);
	if (gd3DataLocation) {
		gd3DataLocation += 0x14;
		if (regDataLocation > fileSize) {
			gd3DataLocation = 0;
			std::cerr << "GD3 pointer out of bounds." << std::endl;
		}
	}

	// Total amount of samples
	regDumpFile.read(buffer, 4);
	totalSmpCount = BitConverter::readUint32(buffer);

	// Loop stuff
	regDumpFile.read(buffer, 4);
	loopOffset = BitConverter::readUint32(buffer);
	regDumpFile.read(buffer, 4);
	loopLength = BitConverter::readUint32(buffer);
	if (!loopOffset || !loopLength) {
		loopOffset = 0; loopLength = 0;
	} else {
		loopOffset += 0x1C;
		if (loopOffset > fileSize) {
			loopOffset = 0; loopLength = 0;
			std::cerr << "Loop pointer out of bounds, treating as no loop." << std::endl;
		} else if (loopLength > totalSmpCount) {
			loopOffset = 0; loopLength = 0;
			std::cerr << "Loop length out of bounds, treating as no loop." << std::endl;
		}
	}
	
	// Extra header
	regDumpFile.read(buffer, 4);
	extraHeaderOffset = BitConverter::readUint32(buffer);
	if (extraHeaderOffset) {
		extraHeaderOffset += 0x24;
		if (extraHeaderOffset > fileSize) {
			extraHeaderOffset = 0;
			std::cerr << "Extra header pointer out of bounds." << std::endl;
		}
	}

	// Output method
	regDumpFile.read(buffer, 1);
	if (!outputMethodOverride) {
		outputMethod = *buffer;
	}


	// Read GD3
	if (gd3DataLocation) {
		int errCode;

		gd3Data = readGd3Data(regDumpFile, fileSize, gd3DataLocation, errCode);

		if (gd3Data != nullptr && !errCode) {

			if (gd3Data->trackNameEnglish.size() + gd3Data->trackNameOG.size() > 0) {
				std::cout << "Track name: " <<
					gd3Data->trackNameEnglish << 
					(gd3Data->trackNameEnglish.size() && gd3Data->trackNameOG.size() ? " / " : "") <<
					gd3Data->trackNameOG << std::endl;
			}
			if (gd3Data->gameNameEnglish.size() + gd3Data->gameNameOG.size() > 0) {
				std::cout << "Game name: " <<
					gd3Data->gameNameEnglish << 
					(gd3Data->gameNameEnglish.size() && gd3Data->gameNameOG.size() ? " / " : "") <<
					gd3Data->gameNameOG << std::endl;
			}
			if (gd3Data->systemNameEnglish.size() + gd3Data->systemNameOG.size() > 0) {
				std::cout << "System: " <<
					gd3Data->systemNameEnglish << 
					(gd3Data->systemNameEnglish.size() && gd3Data->systemNameOG.size() ? " / " : "") <<
					gd3Data->systemNameOG << std::endl;
			}
			if (gd3Data->authorNameEnglish.size() + gd3Data->authorNameOG.size() > 0) {
				std::cout << "Author: " <<
					gd3Data->authorNameEnglish << 
					(gd3Data->authorNameEnglish.size() && gd3Data->authorNameOG.size() ? " / " : "") <<
					gd3Data->authorNameOG << std::endl;
			}
			if (gd3Data->releaseDate.size())
				std::cout << "Release date: " << gd3Data->releaseDate << std::endl;
			if (gd3Data->vgmDumper.size())
				std::cout << "T85 by: " << gd3Data->vgmDumper << std::endl;
			if (gd3Data->notes.size())
				std::cout << "Notes: \n===\n" << gd3Data->notes << "\n===" << std::endl;

		} else {
			std::cerr << gd3Errors[errCode] << std::endl;
		}

	}

	// Extra header (TODO)

	// Read the actual data

	if (!totalSmpCount) {
		std::cerr << "This file specifies its length as 0." << std::endl;
		std::exit(0);
	} else if (!regDataLocation) {
		std::cerr << "This file contains no register dump data." << std::endl;
		std::exit(0);
	}
	// There actually is data, let's go emulate
	apu = t85APU_new(apuClock, sampleRate, outputMethod);

	ticksPerSample = (double)sampleRate / 44100.0;

	regDumpFile.seekg(regDataLocation);

	if (outFileDefined) {
		SndfileHandle outFile(outFilePath, SFM_WRITE, SF_FORMAT_WAV|SF_FORMAT_PCM_16, 1, sampleRate);
		auto audioBuffer = new uint16_t[sampleRate]; 
		size_t idx = 0;

		while (totalSmpCount) {
			emulationTick(regDumpFile);
			sampleTickCounter += ticksPerSample;
			while (sampleTickCounter >= 1.0) {
				// std::cout << sampleTickCounter << "   " << regWrites.size() << std::endl;
				sampleTickCounter -= 1.0;
				if (!t85APU_shiftRegisterPending(apu) && regWrites.size()) {
					t85APU_writeReg(apu, regWrites.front()&0xFF, regWrites.front()>>8);
					std::cout << totalSmpCount << " - WR: " << std::hex << (regWrites.front()>>8) << "->" << (regWrites.front()&0xFF) << std::dec << std::endl;
					regWrites.pop_front();
				} 
				BitConverter::writeBytes(audioBuffer+(idx++), (uint16_t)(t85APU_calc(apu)<<(15-apu->outputBitdepth)));
				if (idx >= sampleRate) {
					idx = 0;
					outFile.write((short *)audioBuffer, sampleRate);
				}
			}
		} 
		if (idx) outFile.write((short *)audioBuffer, idx);

		// outfile wil close on destruction, but the pointer won't
		delete[] audioBuffer;
	} else {
		// Fucking live playback
		struct SoundIo *soundio = soundio_create();
		if (!soundio) {
			fprintf(stderr, "out of memory\n");
			return 1;
		}
		soundio->app_name = "t85play v0.1";
		int err = soundio_connect(soundio);
		if (err) {
			fprintf(stderr, "Unable to connect to backend: %s\n", soundio_strerror(err));
			return 1;
		}
		fprintf(stderr, "Backend: %s\n", soundio_backend_name(soundio->current_backend));
    	soundio_flush_events(soundio);
		int selected_device_index = soundio_default_output_device_index(soundio);
		if (selected_device_index < 0) {
			fprintf(stderr, "Output device not found\n");
			return 1;
		}
		struct SoundIoDevice *device = soundio_get_output_device(soundio, selected_device_index);
		if (!device) {
			fprintf(stderr, "out of memory\n");
			return 1;
		}
		fprintf(stderr, "Output device: %s\n", device->name);
		if (device->probe_error) {
			fprintf(stderr, "Cannot probe device: %s\n", soundio_strerror(device->probe_error));
			return 1;
		}
		struct SoundIoOutStream *outstream = soundio_outstream_create(device);
		if (!outstream) {
			fprintf(stderr, "out of memory\n");
			return 1;
		}
		outstream->write_callback = write_callback;
		std::string sound_name = (gd3Data->gameNameEnglish.size() ? gd3Data->gameNameEnglish : gd3Data->gameNameOG) + " : " + (gd3Data->trackNameEnglish.size() ? gd3Data->trackNameEnglish : gd3Data->trackNameOG);
		outstream->name = sound_name.c_str();
		outstream->sample_rate = sampleRate;
		if (soundio_device_supports_format(device, SoundIoFormatS16NE)) {
       		outstream->format = SoundIoFormatS16NE;
		} else {
			fprintf(stderr, "No suitable device format available.\n");
			return 1;
		}
		if ((err = soundio_outstream_open(outstream))) {
			fprintf(stderr, "unable to open device: %s", soundio_strerror(err));
			return 1;
		}
		if (outstream->layout_error)
			fprintf(stderr, "unable to set channel layout: %s\n", soundio_strerror(outstream->layout_error));
		if ((err = soundio_outstream_start(outstream))) {
			fprintf(stderr, "unable to start device: %s\n", soundio_strerror(err));
			return 1;
		}
		static uint64_t invoked = 0;
		// while (!ended) {soundio_flush_events(soundio);}
		// std::this_thread::sleep_for(std::chrono::seconds(6));
		while (getc(stdin) != "q"[0]) {}
		soundio_outstream_destroy(outstream);
		soundio_device_unref(device);
		soundio_destroy(soundio);


	}

	t85APU_delete(apu);
	return 0;

}