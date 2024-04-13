#include <cmath>

namespace BandFilter {

static const double PI = 3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679;

typedef struct __BandFilter {
		double a0, a1, a2, b1, b2;
		double x_1, x_2, y_1, y_2;
} BandFilter;

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
*/
void setNotchFilterParams(BandFilter & filter, double targetFreq, double bandwidth){
	double cospi2f2 = std::cos(2 * PI * targetFreq) * 2;
	double r = 1 - 3 * bandwidth;
	double k = (double)(1 - cospi2f2*r + r*r) / (double)(2 - cospi2f2);

	filter.a0 = k, filter.a2 = k;
	filter.a1 = -1*k*cospi2f2;
	filter.b1 = r*cospi2f2;
	filter.b2 = -(r*r);
}

void resetFilter(BandFilter & filter) {
	filter.x_1 = 0;
	filter.x_2 = 0;
	filter.y_1 = 0;
	filter.y_2 = 0;
}

void setNotchFilterParams(BandFilter & filter, double sampleFreq, double targetFreq, double bandwidth) {
	setNotchFilterParams(filter, targetFreq / sampleFreq, bandwidth);
}

template <typename T>
void filterBuffer(BandFilter & filter, const T *x, T *y, size_t n) {
    for (size_t i = 0; i < n; i++) {
        y[i] = filter.a0 * x[i] + filter.a1 * filter.x_1 + filter.a2 * filter.x_2  // IIR difference equation
                         		+ filter.b1 * filter.y_1 + filter.b2 * filter.y_2;
        filter.x_2 = filter.x_1;       // shift delayed x, y samples
        filter.x_1 = (double)x[i];
        filter.y_2 = filter.y_1;
        filter.y_1 = (double)y[i];
    }
}

template <typename T>
void filterBuffer(BandFilter & filter, T *x, size_t n) {
    for (size_t i = 0; i < n; i++) {
		T oldX = x[i];
        x[i] = filter.a0 * x[i] + filter.a1 * filter.x_1 + filter.a2 * filter.x_2  // IIR difference equation
                         		+ filter.b1 * filter.y_1 + filter.b2 * filter.y_2;
        filter.x_2 = filter.x_1;       // shift delayed x, y samples
        filter.x_1 = (double)oldX;
        filter.y_2 = filter.y_1;
        filter.y_1 = (double)x[i];
    }
}

template <typename T>
T filterSingle(BandFilter & filter, T x) {
	T output;
	output = filter.a0 * x + filter.a1 * filter.x_1 + filter.a2 * filter.x_2  // IIR difference equation
							+ filter.b1 * filter.y_1 + filter.b2 * filter.y_2;
	filter.x_2 = filter.x_1;       // shift delayed x, y samples
	filter.x_1 = (double)x;
	filter.y_2 = filter.y_1;
	filter.y_1 = (double)output;
	return output;
}

}