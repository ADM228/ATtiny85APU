#ifndef __BITCONVERTER_INCLUDED__
#define __BITCONVERTER_INCLUDED__

#include <cstdint>

namespace BitConverter {

#if !defined(__BIG_ENDIAN_OVERRIDE) && ( \
    defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || \
    defined(__LITTLE_ENDIAN__) || \
    defined(__ARMEL__) || \
    defined(__THUMBEL__) || \
    defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) )

// system is natively little-endian, no conversion needed

// Write a uint64_t to ptr, exists for portability with big-endian systems
template <typename T> inline void writeBytes (T * ptr, uint64_t input) { *(uint64_t *)ptr = input; }

// Write a uint32_t to ptr, exists for portability with big-endian systems
template <typename T> inline void writeBytes (T * ptr, uint32_t input) { *(uint32_t *)ptr = input; }

// Write a uint16_t to ptr, exists for portability with big-endian systems
template <typename T> inline void writeBytes (T * ptr, uint16_t input) { *(uint16_t *)ptr = input; }

// Read a uint64_t from ptr, exists for portability with big-endian systems
template <typename T> inline uint64_t readUint64 (T * ptr) {return *(uint64_t *) ptr;}

// Read a uint32_t from ptr, exists for portability with big-endian systems
template <typename T> inline uint32_t readUint32 (T * ptr) {return *(uint32_t *) ptr;}

// Read a uint16_t from ptr, exists for portability with big-endian systems
template <typename T> inline uint16_t readUint16 (T * ptr) {return *(uint16_t *) ptr;}

#elif defined(__BIG_ENDIAN_OVERRIDE) || \
    defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || \
    defined(__BIG_ENDIAN__) || \
    defined(__ARMEB__) || \
    defined(__THUMBEB__) || \
    defined(__AARCH64EB__) || \
    defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__) || \
    defined(_MSC_VER)   // MSVC YOU FUCKING SON OF A BITCH

// system is natively big-endian (or undetermined in case of MSVC), conversion required

// Write a uint64_t to ptr, while converting it to little-endian
template <typename T>
inline void writeBytes (T * ptr, uint64_t input) {
    uint8_t * tmp = (uint8_t*) ptr; 
    *(tmp  )= (input    )&0xFF;
    *(tmp+1)= (input>> 8)&0xFF;
    *(tmp+2)= (input>>16)&0xFF;
    *(tmp+3)= (input>>24)&0xFF;
    *(tmp+4)= (input>>32)&0xFF;
    *(tmp+5)= (input>>40)&0xFF;
    *(tmp+6)= (input>>48)&0xFF;
    *(tmp+7)= (input>>56)&0xFF;
}

// Write a uint64_t to ptr, while converting it to little-endian
inline void writeBytes (uint64_t input, uint8_t * ptr) {
    *(ptr  )= (input    )&0xFF;
    *(ptr+1)= (input>> 8)&0xFF;
    *(ptr+2)= (input>>16)&0xFF;
    *(ptr+3)= (input>>24)&0xFF;
    *(ptr+4)= (input>>32)&0xFF;
    *(ptr+5)= (input>>40)&0xFF;
    *(ptr+6)= (input>>48)&0xFF;
    *(ptr+7)= (input>>56)&0xFF;
}

// Write a uint32_t to ptr, while converting it to little-endian
template <typename T>
inline void writeBytes (T * ptr, uint32_t input) {
    uint8_t * tmp = (uint8_t*) ptr; 
    *(tmp  )= (input    )&0xFF;
    *(tmp+1)= (input>> 8)&0xFF;
    *(tmp+2)= (input>>16)&0xFF;
    *(tmp+3)= (input>>24)&0xFF;
}

// Write a uint32_t to ptr, while converting it to little-endian
inline void writeBytes (uint32_t input, uint8_t * ptr) {
    *(ptr  )= (input    )&0xFF;
    *(ptr+1)= (input>> 8)&0xFF;
    *(ptr+2)= (input>>16)&0xFF;
    *(ptr+3)= (input>>24)&0xFF;
}

// Write a uint16_t to ptr, while converting it to little-endian
template <typename T>
inline void writeBytes (T * ptr, uint16_t input) {
    uint8_t * tmp = (uint8_t*) ptr; 
    *(tmp  )= (input    )&0xFF;
    *(tmp+1)= (input>> 8)&0xFF;
}

// Write a uint16_t to ptr, while converting it to little-endian
inline void writeBytes (uint16_t input, uint8_t * ptr) {
    *(ptr  )= (input    )&0xFF;
    *(ptr+1)= (input>> 8)&0xFF;
}

// Read a uint64_t from ptr in little-endian
inline uint64_t readUint64 (uint8_t * ptr) {
    return \
        (uint64_t)*(ptr  )    |
        (uint64_t)*(ptr+1)<< 8|
        (uint64_t)*(ptr+2)<<16|
        (uint64_t)*(ptr+3)<<24|
        (uint64_t)*(ptr+4)<<32|
        (uint64_t)*(ptr+5)<<40|
        (uint64_t)*(ptr+6)<<48|
        (uint64_t)*(ptr+7)<<56;
}

// Read a uint32_t from ptr in little-endian
inline uint32_t readUint32 (uint8_t * ptr) {
    return \
        (uint32_t)*(ptr  )    |
        (uint32_t)*(ptr+1)<< 8|
        (uint32_t)*(ptr+2)<<16|
        (uint32_t)*(ptr+3)<<24;
}

// Read a uint16_t from ptr in little-endian
inline uint16_t readUint16 (uint8_t * ptr) {
    return *(ptr) | *(ptr+1)<<8;
}

#else
#error "Unsupported architecture. Please contact the lead dev and report what architecture you're compiling on"
#endif

}   // namespace BitConverter

#endif  // __BITCONVERTER_INCLUDED__