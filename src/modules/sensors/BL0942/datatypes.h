#pragma once

#include <cstdint>

//#include "helpers.h"


namespace internal {

// Various functions can be constexpr in C++14, but not in C++11 (because their body isn't just a return statement).
// Define a substitute constexpr keyword for those functions, until we can drop C++11 support.
#if __cplusplus >= 201402L
#define constexpr14 constexpr
#else
#define constexpr14 inline  // constexpr implies inline
#endif


// std::byteswap from C++23
template<typename T> constexpr14 T byteswap(T n) {
  T m;
  for (size_t i = 0; i < sizeof(T); i++)
    reinterpret_cast<uint8_t *>(&m)[i] = reinterpret_cast<uint8_t *>(&n)[sizeof(T) - 1 - i];
  return m;
}
template<> constexpr14 uint8_t byteswap(uint8_t n) { return n; }
template<> constexpr14 uint16_t byteswap(uint16_t n) { return __builtin_bswap16(n); }
template<> constexpr14 uint32_t byteswap(uint32_t n) { return __builtin_bswap32(n); }
template<> constexpr14 uint64_t byteswap(uint64_t n) { return __builtin_bswap64(n); }
template<> constexpr14 int8_t byteswap(int8_t n) { return n; }
template<> constexpr14 int16_t byteswap(int16_t n) { return __builtin_bswap16(n); }
template<> constexpr14 int32_t byteswap(int32_t n) { return __builtin_bswap32(n); }
template<> constexpr14 int64_t byteswap(int64_t n) { return __builtin_bswap64(n); }

/// Convert a value between host byte order and big endian (most significant byte first) order.
template<typename T> constexpr14 T convert_big_endian(T val) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return byteswap(val);
#else
  return val;
#endif
}

/// Convert a value between host byte order and little endian (least significant byte first) order.
template<typename T> constexpr14 T convert_little_endian(T val) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return val;
#else
  return byteswap(val);
#endif
}

/// Wrapper class for memory using big endian data layout, transparently converting it to native order.
template<typename T> class BigEndianLayout {
 public:
  constexpr14 operator T() { return convert_big_endian(val_); }

 private:
  T val_;
} __attribute__((packed));

/// Wrapper class for memory using big endian data layout, transparently converting it to native order.
template<typename T> class LittleEndianLayout {
 public:
  constexpr14 operator T() { return convert_little_endian(val_); }

 private:
  T val_;
} __attribute__((packed));

}  // namespace internal

/// 24-bit unsigned integer type, transparently converting to 32-bit.
struct uint24_t {  // NOLINT(readability-identifier-naming)
  operator uint32_t() { return val; }
  uint32_t val : 24;
} __attribute__((packed));

/// 24-bit signed integer type, transparently converting to 32-bit.
struct int24_t {  // NOLINT(readability-identifier-naming)
  operator int32_t() { return val; }
  int32_t val : 24;
} __attribute__((packed));

// Integer types in big or little endian data layout.
using uint64_be_t = internal::BigEndianLayout<uint64_t>;
using uint32_be_t = internal::BigEndianLayout<uint32_t>;
using uint24_be_t = internal::BigEndianLayout<uint24_t>;
using uint16_be_t = internal::BigEndianLayout<uint16_t>;
using int64_be_t = internal::BigEndianLayout<int64_t>;
using int32_be_t = internal::BigEndianLayout<int32_t>;
using int24_be_t = internal::BigEndianLayout<int24_t>;
using int16_be_t = internal::BigEndianLayout<int16_t>;
using uint64_le_t = internal::LittleEndianLayout<uint64_t>;
using uint32_le_t = internal::LittleEndianLayout<uint32_t>;
using uint24_le_t = internal::LittleEndianLayout<uint24_t>;
using uint16_le_t = internal::LittleEndianLayout<uint16_t>;
using int64_le_t = internal::LittleEndianLayout<int64_t>;
using int32_le_t = internal::LittleEndianLayout<int32_t>;
using int24_le_t = internal::LittleEndianLayout<int24_t>;
using int16_le_t = internal::LittleEndianLayout<int16_t>;

struct DataPacket {
  uint8_t frame_header;
  uint24_le_t i_rms;
  uint24_le_t v_rms;
  uint24_le_t i_fast_rms;
  int24_le_t watt;
  uint24_le_t cf_cnt;
  uint16_le_t frequency;
  uint8_t reserved1;
  uint8_t status;
  uint8_t reserved2;
  uint8_t reserved3;
  uint8_t checksum;
} __attribute__((packed));