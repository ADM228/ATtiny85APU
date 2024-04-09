#include <cmath>
#include <cstring>
#include <cstdint>
#include <cwchar>

#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <string>

#include <sndfile.h>
#include <soundio/soundio.h>

#include "ATtiny85APU.h"
#include "BitConverter.cpp"


static const uint32_t currentFileVersion = 0x00000000;	// 16.8.8 bits semantic versioning
static const uint32_t currentGd3Version = 0x00000100;

t85APU * apu;

std::filesystem::path filePath;
bool fileDefined = false;

uint32_t sampleRate = 44100;

bool notchFilter = false;

uint8_t outputMode;
bool outputMethodOverride = false;

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
	char buffer[4];
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

	return output;
}

int main (int argc, char** argv) {
	std::cout << "ATtiny85APU register dump player v0.1" << std::endl << "© alexmush, 2024" << std::endl << std::endl;
	for (int i = 1; i < argc; i++) {	// Check for the help command specifically
		if (!(
			memcmp(argv[i], "-h", 2) &&
			memcmp(argv[i], "--help", 2+4))) {
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
			memcmp(argv[i], "-i", 2) && 
			memcmp(argv[i], "--input", 7))) {
			if (fileDefined) {
				std::cout << "Cannot input more than 1 file at once" << std::endl;
			} else if (i+1 >= argc) {
				std::cout << "File argument not supplied" << std::endl;
			} else if (argv[i+1][0] == char("-"[0])){
				std::cout << "Invalid file argument: \"" << argv[i+1] << "\"" << std::endl;
			} else {
				filePath = std::filesystem::path(argv[i+1]);
				if (!std::filesystem::exists(filePath)) {
					std::cout << "File \"" << argv[i+1] << "\" does not exist" << std::endl;
				} else {
					fileDefined = true;
				}
			}
			i++;
		} else if (!(
			memcmp(argv[i], "-s", 3) && 
			memcmp(argv[i], "--sample-rate", 2+6+1+4))) {
			// Sample rate
			if (i+1 >= argc) {
				std::cout << "Sample rate argument not supplied" << std::endl;
			} else {
				char* p;
				auto tmp = strtol(argv[i+1], &p, 10);
				if (*p) {
					std::cout << "Invalid sample rate argument: \"" << argv[i+1] << "\"" << std::endl;
				} else {
					sampleRate = tmp;
				}
			}
			i++;
		} else if (!(
			memcmp(argv[i], "-om", 3) &&
			memcmp(argv[i], "--output-method", 2+6+1+6))) {
			// Override output mode
			if (i+1 >= argc) {
				std::cout << "Output method argument not supplied" << std::endl;
			} else {
				char* p;
				auto tmp = strtol(argv[i+1], &p, 10);
				if (*p) {
					std::cout << "Invalid output method argument: \"" << argv[i+1] << "\"" << std::endl;
				} else {
					outputMode = tmp;
					outputMethodOverride = true;
				}
			}
			i++;
		} else if (!(
			memcmp(argv[i], "-f", 2) && 
			memcmp(argv[i], "--filter", 2+6))) {
			// Enable notch filter
			notchFilter = true;
		} else if (!(
			memcmp(argv[i], "-nf", 3) && 
			memcmp(argv[i], "--no-filter", 2+2+1+6))) {
			// Disable notch filter
			notchFilter = false;
		}
	}
	if (!fileDefined) {
		std::string filename;
		std::cout << "\tPlease input register dump file name: ";
		std::cin >> filename;
		filePath = std::filesystem::path(filename);
		if (!std::filesystem::exists(filePath)) {
			std::cout << "File \"" << filename << "\" does not exist" << std::endl;
			std::exit(1);
		} else {
			fileDefined = true;
		}
	}

	auto expectedFileSize = std::filesystem::file_size(filePath);
	std::ifstream regDumpFile (filePath);

	// Read header
	char buffer[4];
	regDumpFile.read(buffer, 4);
	if (memcmp(buffer, "t85!", 4)) {
		std::cout << "File header does not match" << std::endl;
		std::exit(2);
	}

	// File size
	regDumpFile.read(buffer, 4);
	uint32_t fileSize = BitConverter::readUint32(buffer) + 0x04;
	if (fileSize > expectedFileSize) {
		std::cout << "File seems to be cut off by " << fileSize - expectedFileSize << " bytes. " << std::endl;
		std::exit(3);
	} else if (fileSize < expectedFileSize) {
		std::cout << "File seems to contain " << expectedFileSize - fileSize << " extra bytes of data. " << std::endl;
	}
	if (fileSize < 0x2C) {
		std::cout << "File is too small for the t85 header" << std::endl;
		std::exit(2);
	}

	// File version
	regDumpFile.read(buffer, 4);
	uint32_t fileVersion = BitConverter::readUint32(buffer);
	if (fileVersion > currentFileVersion) {
		std::cout << "File version (" << 
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
	uint32_t apuClock = BitConverter::readUint32(buffer);
	if (apuClock == 0) {
		std::cout << "APU Clock speed not specified, assuming 8MHz." << std::endl;
		apuClock = 8000000;
	}

	// VGM data offset
	regDumpFile.read(buffer, 4);
	uint32_t regDataLocation = BitConverter::readUint32(buffer);
	if (regDataLocation == 0) {
		std::cout << "This file contains no register dump data." << std::endl;
	} else {
		regDataLocation += 0x10;	// Offset after all
		if (regDataLocation > fileSize) {
			regDataLocation = 0;
			std::cout << "Register dump pointer out of bounds." << std::endl;
		}
	}

	// GD3 data offset
	regDumpFile.read(buffer, 4);
	uint32_t gd3DataLocation = BitConverter::readUint32(buffer);
	if (gd3DataLocation == 0) {
		std::cout << "This file contains no GD3 data." << std::endl;
	} else {
		gd3DataLocation += 0x14;
		if (regDataLocation > fileSize) {
			gd3DataLocation = 0;
			std::cout << "GD3 pointer out of bounds." << std::endl;
		}
	}

	// Total amount of samples
	regDumpFile.read(buffer, 4);
	uint32_t totalSmpCount = BitConverter::readUint32(buffer);
	if (totalSmpCount == 0) {
		std::cout << "This file specifies its length as 0." << std::endl;
	} else {
		std::cout << "This file is " << totalSmpCount << " samples long, that's " << totalSmpCount / 44100.0 << " seconds." << std::endl;
	}

	// Loop stuff
	regDumpFile.read(buffer, 4);
	uint32_t loopOffset = BitConverter::readUint32(buffer);
	regDumpFile.read(buffer, 4);
	uint32_t loopLength = BitConverter::readUint32(buffer);
	if (!loopOffset || !loopLength) {
		loopOffset = 0; loopLength = 0;
		std::cout << "No loop" << std::endl;
	} else {
		loopOffset += 0x1C;
		if (loopOffset > fileSize) {
			loopOffset = 0; loopLength = 0;
			std::cout << "Loop pointer out of bounds, treating as no loop." << std::endl;
		} else if (loopLength > totalSmpCount) {
			loopOffset = 0; loopLength = 0;
			std::cout << "Loop length out of bounds, treating as no loop." << std::endl;
		} else {
			std::cout << "Looped validly" << std::endl;
		}
	}
	
	// Extra header
	regDumpFile.read(buffer, 4);
	uint32_t extraHeaderOffset = BitConverter::readUint32(buffer);
	if (!extraHeaderOffset) {
		std::cout << "No extra header" << std::endl;
	} else {
		extraHeaderOffset += 0x24;
		if (extraHeaderOffset > fileSize) {
			extraHeaderOffset = 0;
			std::cout << "Extra header pointer out of bounds." << std::endl;
		}
	}

	// Output method
	regDumpFile.read(buffer, 1);
	if (!outputMethodOverride) {
		uint8_t outputMethod = *buffer;
	}


	// Read GD3
	if (gd3DataLocation) {
		int errCode;

		auto data = readGd3Data(regDumpFile, fileSize, gd3DataLocation, errCode);

		if (data != nullptr && !errCode) {

			if (data->trackNameEnglish.size() + data->trackNameOG.size() > 0) {
				std::cout << "Track name: " <<
					data->trackNameEnglish << 
					(data->trackNameEnglish.size() && data->trackNameOG.size() ? " / " : "") <<
					data->trackNameOG << std::endl;
			}
			if (data->gameNameEnglish.size() + data->gameNameOG.size() > 0) {
				std::cout << "Game name: " <<
					data->gameNameEnglish << 
					(data->gameNameEnglish.size() && data->gameNameOG.size() ? " / " : "") <<
					data->gameNameOG << std::endl;
			}
			if (data->systemNameEnglish.size() + data->systemNameOG.size() > 0) {
				std::cout << "System: " <<
					data->systemNameEnglish << 
					(data->systemNameEnglish.size() && data->systemNameOG.size() ? " / " : "") <<
					data->systemNameOG << std::endl;
			}
			if (data->authorNameEnglish.size() + data->authorNameOG.size() > 0) {
				std::cout << "Author: " <<
					data->authorNameEnglish << 
					(data->authorNameEnglish.size() && data->authorNameOG.size() ? " / " : "") <<
					data->authorNameOG << std::endl;
			}
			if (data->releaseDate.size())
				std::cout << "Release date: " << data->releaseDate << std::endl;
			if (data->vgmDumper.size())
				std::cout << "T85 by: " << data->vgmDumper << std::endl;
			if (data->notes.size())
				std::cout << "Notes: \n===\n" << data->notes << "\n===" << std::endl;

		} else {
			std::cout << gd3Errors[errCode] << std::endl;
		}

	}


	// Read the actual data
}