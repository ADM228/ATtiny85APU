// Low pitch regs
#define PILOA 0x00
#define PILOB 0x01
#define PILOC 0x02
#define PILOD 0x03
#define PILOE 0x04
#define PILON 0x05

// High pitch regs
#define PHIAB 0x06
#define PHICD 0x07
#define PHIEN 0x08

// Duty cycle regs
#define DUTYA 0x09
#define DUTYB 0x0A
#define DUTYC 0x0B
#define DUTYD 0x0C
#define DUTYE 0x0D

// Noise tap registers
#define NTPLO 0x0E
#define NTPHI 0x0F

// Static volume registers
#define VOL_A 0x10
#define VOL_B 0x11
#define VOL_C 0x12
#define VOL_D 0x13
#define VOL_E 0x14

// Channel config registers
#define CFG_A 0x15
#define CFG_B 0x16
#define CFG_C 0x17
#define CFG_D 0x18
#define CFG_E 0x19

// Envelope load
#define ELDLO 0x1A
#define ELDHI 0x1B

// Envelope shapes
#define E_SHP 0x1C

// Low pitch for envelopes
#define EPLOA 0x1D
#define EPLOB 0x1E

// High pitch for envelopes
#define EPIHI 0x1F


// Bit defines
// High pitch regs
#define PR_SQ_A 3
#define PR_SQ_B 7
#define PR_SQ_C 3
#define PR_SQ_D 7
#define PR_SQ_E 3
#define PR_NOISE 7

// Config
#define NOISE_EN 7
#define ENV_EN 6
#define ENV_SMP 5
#define SLOT_NUM 4
#define R_VOL1 3
#define R_VOL0 2
#define L_VOL1 1
#define L_VOL0 0

// Envelope shape
#define ENVA_HOLD 0
#define ENVA_ALT 1
#define ENVA_ATT 2
#define ENVA_RST 3
#define ENVB_HOLD 4
#define ENVB_ALT 5
#define ENVB_ATT 6
#define ENVB_RST 7

// Other useful macros
#define PitchHi_Sq_A(x) (x & 7)
#define PitchHi_Sq_B(x) ((x & 7) << 4)
#define PitchHi_Sq_C(x) (x & 7)
#define PitchHi_Sq_D(x) ((x & 7) << 4)
#define PitchHi_Sq_E(x) (x & 7)
#define PitchHi_Noise(x) ((x & 7) << 4)

#define PitchHi_Env_A(x) (x & 0xF)
#define PitchHi_Env_B(x) ((x & 0xF) << 4)

#define PanLeft(x) (x & 3)
#define PanRight(x) ((x & 3) << 2)
#define Pan(left, right) PanLeft(left)|PanRight(right)