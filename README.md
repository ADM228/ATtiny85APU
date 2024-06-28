# ATtiny85APU

This project is creating a soundchip out of an ATtiny85. Written entirely in assembly, it is small enough to fit even onto an ATtiny25 (and the '45 too, obviously).

## Capabilities

- 5 channels (named A, B, C, D and E)
  - Each of those has a phase accumulator-powered pulse oscillator with:
    - 8-bit duty cycle
    - 8-bit phase accumulator increment value
    - 3-bit octave value (shifts the increment value for a total 15-bit range)
    - Dedicated phase reset bit
- Volume control:
  - 2 envelope generators (A and B):
    - Powered by phase accumulators
    - 8-bit phase accumulator increment value
    - 4-bit octave value
      - When the MSB of the octave value is set, the pitch is equivalent to the pulse oscillators
    - Shapes are bit-compatible with the widely used soundchip AY-3-8910, but linear and with 256 steps
    - Dedicated phase reset bit
  - A channel's static volume:
    - 8 bits when not using envelopes
    - When using envelopes, the MSB halves the envelope's volume
  - Panning registers:
    - 2 bits each
    - Always enabled, even without stereo
- Noise generator:
  - Ticked by phase accumulator similar to the tone ones:
    - 8-bit phase accumulator increment value
    - 3-bit octave value (shifts the increment value for a total 15-bit range)
    - Dedicated phase reset bit (only resets the phase accumulator, not LFSR)
  - Powered by a 16-bit [Galois LFSR](https://en.wikipedia.org/wiki/Linear-feedback_shift_register#Galois_LFSRs) with adjustable taps
  - The operation is XNOR, always can come back to life
  - On each channel, OR'd with the pulse wave

## Registers

The registers themselves:

```
 ___________ _______ _______________________________________________________________
|           | Bit → |   7   |   6   |   5   |   4   |   3   |   2   |   1   |   0   |
|   Index   |  Name |=======|=======|=======|=======|=======|=======|=======|=======|
|   0x0n    | PILOX |          Pitch increment value for tone on channel X          |
|(n = 0..4,X = A..E)|                                                               |
|   0x05    | PILON |           Pitch increment value for noise generator           |
|===========|=======|=======|=======|=======|=======|=======|=======|=======|=======|
|   0x06    | PHIAB |Ch.B PR|Channel B octave number|Ch.A PR|Channel A octave number|
|   0x07    | PHICD |Ch.D PR|Channel D octave number|Ch.C PR|Channel C octave number|
|   0x08    | PHIEN |NoisePR|Noise gen octave number|Ch.E PR|Channel E octave number|
|===========|=======|=======|=======|=======|=======|=======|=======|=======|=======|
|   0x0n    | DUTYX |                   Channel X tone duty cycle                   |
|(n = 9..D,X = A..E)|                                                               |
|===========|=======|=======|=======|=======|=======|=======|=======|=======|=======|
|   0x0E    | NTPLO |             Noise LFSR inversion value (low byte)             |
|   0x0F    | NTPHI |             Noise LFSR inversion value (high byte)            |
|===========|=======|=======|=======|=======|=======|=======|=======|=======|=======|
|   0x1n    | VOLX  |                    Channel X static volume                    |
|(n = 0..4,X = A..E)|                                                               |
|===========|=======|=======|=======|=======|=======|=======|=======|=======|=======|
|   0x1n    | CFGX  |NoiseEn| EnvEn |Env/Smp| Slot# |  Right volume |  Left volume  |
|(n = 5..9,X = A..E)|                                                               |
|===========|=======|=======|=======|=======|=======|=======|=======|=======|=======|
|   0x1A    | ELLO  |             Low byte of envelope phase load value             |
|   0x1B    | ELHI  |            High byte of envelope phase load value             |
|===========|=======|=======|=======|=======|=======|=======|=======|=======|=======|
|   0x1C    | ESHP  |EnvB PR|    Envelope B shape   |EnvA PR|    Envelope A shape   |
|===========|=======|=======|=======|=======|=======|=======|=======|=======|=======|
|   0x1D    | EPLA  |             Pitch increment value for envelope A              |
|   0x1E    | EPLB  |             Pitch increment value for envelope B              |
|===========|=======|=======|=======|=======|=======|=======|=======|=======|=======|
|   0x1F    | EPH   |     Envelope B octave num     |     Envelope A octave num     |
|===========|=======|=======|=======|=======|=======|=======|=======|=======|=======|
|___________|_______|_______|_______|_______|_______|_______|_______|_______|_______|

```

Notes:

- PR means phase reset in every case
- The envelope phase load values are shared, and loaded into an envelope when its phase reset bit is set (it can be both)
- The default values for registers:
  - Most are 0
  - `NTPHI` is 0x24, sorta corresponding to the AY-3-8910
  - `CFGX` is 0x0F, corresponding to maximum panning volume on both sides
- Since the chip currently only outputs mono audio, only the left panning bits in `CFGX` are used. It is recommended to set the right panning bits to the same contents as the left panning bits for compatibility in the future.

## Real hardware

### Firmware

The firmware is located in the [avr](avr/) folder, and is entirely written in AVR assembly. Its size is currently under 2 Kibibytes, therefore it can fit onto an ATtiny25 and '45 in addition to the '85. It is assembled with the [avra](github.com/Ro5bert/avra) assembler by running the command `make firmware` or `make avr`, which puts the resulting `main.hex` file into the `bin/avr` folder.

### Connections

The pin connection diagram is:

```
               __ __
     /Reset -1|° U  |8- VCC
         CS -2|     |7- SPI CLK
        OUT -3|     |6- SPI DO (MOSI)
        GND -4|_____|5- SPI DI (MISO)
```

Pins 2, 5, 6 and 7 are used for communicating with the register write buffer via SPI. The ATtiny is the *master* of SPI, checking for new register writes once every sample. The connection is:
| `CS` | Function |
|:----:|---|
| 0 | Fetch from register write storage |
| 1 | No SPI |

### Output

Currently, the only available output option is PWM output through the `OUT` pin. The PWM rate is 2× the sample rate, aka the master clock divided by 256.

### Register writes

1 register write is 16 bits long and must work like this:

1. The MSB must be a "1" for the ATtiny to even proceed to get the register write.
2. The next 7 bits are the address of the register write
3. The next 8 bits are the value of the register write

The ATtiny85APU automatically flushes 1 register write per sample, the SPI CLK speed is ½ of the master clock speed (e.g. 4 MHz SPI CLK speed at 8 MHz clock speed). Order of transfer is MSB first.

### Clock speeds

Due to all of the pins being busy, the ATtiny85APU cannot receive an external clock signal. Therefore, it only has 2 clock source options:

- The default internal oscillator:
  - Maximum 8 MHz clock speed, corresponding to 15625 Hz sample rate (and 31250 Hz PWM rate)
  - Divisible by 8 with the `CKDIV8` fuse (set by default, it is absolutely recommended to disable this for the sake of your ears)
    - At 8 MHz, can be powered by as little as 2.4V
    - At 4 MHz and lower, can be powered by 1.8V
- The PLL:
  - 16 MHz clock speed, corresponding to 31250 Hz sample rate (and 62500 Hz PWM rate)
  - Requires at least 3.78V for safe operation
  
## Emulation

The emulator is located in the [emu/libt85apu](emu/libt85apu/) folder. It is written in C99. Features:

- Fully compatible with the features of the real hardware
- Cycle-accurate emulation of the output delays from when it was calculated
- Ability to set arbitrary sample and clock rates
- 2 resampling quality options available:
  - 0: No resampling
  - 1: Averaging of values on that sample
- 2 options for emulating PWM output on pin 3:
  - Essentially an 8-bit DAC
  - Actual cycle-accurate PWM emulation
- Emulation of a register write buffer that register writes can pile up onto and then automatically flushed when it's time to update
  - Sizing can be defined at compile time or runtime via the `T85APU_SHIFT_REGISTER_SIZE` define
  - A function that tells you whether an update is pending in the shift register
- Raw and padded sample output
- A class-based C++ wrapper for your convenience
- zlib licensed

For more info check out the [t85apu.h](emu/libt85apu/t85apu.h) and [t85apu.hpp](emu/libt85apu/t85apu.hpp) files. The emulator also provides useful register defines in the [t85apu_regdefines.h](emu/libt85apu/t85apu_regdefines.h) file.

### Manual usage

To use it manually, copy the [emu/libt85apu](emu/libt85apu/) folder into your project, and include the `t85apu.h` in C, or `t85apu.hpp` if you want to use the C++ wrapper. Then, compile the t85apu.c file as a library and link it to the final executable.

### Usage with CMake

The emulator also has a CMake API, which makes including it in your project much easier. To do that, you need to merely add this repository via FetchContent. Here's an example of what to add to your `CMakeLists.txt`:

```
include(FetchContent)
FetchContent_Declare(t85apu
	GIT_REPOSITORY https://github.com/ADM228/ATtiny85APU.git
	GIT_TAG main)

set(T85APU_REGWRITE_BUFFER_SIZE 1)
FetchContent_MakeAvailable(t85apu)

target_link_libraries(<your_executable_name> PRIVATE t85apu)
```