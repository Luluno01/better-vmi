#ifndef C25CB003_C04F_4E40_946A_9E2B7B4FA0F8
#define C25CB003_C04F_4E40_946A_9E2B7B4FA0F8


#include <config.h>
#include <iostream>
#include <iomanip>
#include <limits>
#include <cstdint>


#ifdef DEBUG
#define DBG() std::cout
#else
#define DBG() 0 && std::cout
#endif

/**
 * @brief Format as a pointer.
 * 
 */
#define F_PTR(intVal) reinterpret_cast<void *>(intVal)

template <typename NumType>
inline bool isSigned(const NumType &_) {
  (void) _;  // Unused
  return std::numeric_limits<NumType>::is_signed;
}

template <typename NumType>
inline NumType _abs(const NumType &intVal) {
  return intVal > 0 ? intVal : -intVal;
}

/**
 * @brief Format as a decimal integer (up to 32 bits).
 * 
 */
#define F_D32(intVal) \
  std::dec << (isSigned(intVal) ? signed(intVal) : unsigned(intVal))
  // Because sometimes it prints in hex format, I have no idea why

/**
 * @brief Format as a decimal integer (up to 64 bits).
 * 
 */
#define F_D64(intVal) \
  std::dec << ( \
    isSigned(intVal) ? \
    (int64_t) intVal : (uint64_t) intVal \
  )

#define F_DEC(intVal) F_D64(intVal)

/**
 * @brief Format as an unsigned hexadecimal integer (up to 32 bits).
 * 
 */
#define F_UH32(intVal) "0x" \
  << std::setfill('0') << std::setw(sizeof(intVal) * 2) \
  << std::right << std::hex << unsigned(intVal)
  // Convert `intVal` to an integer in case it's uint8_t

/**
 * @brief Format as an unsigned hexadecimal integer (up to 64 bits).
 * 
 */
#define F_UH64(intVal) "0x" \
  << std::setfill('0') << std::setw(sizeof(intVal) * 2) \
  << std::right << std::hex << (uint64_t) intVal

/**
 * @brief Format as an unsigned hexadecimal integer without leading zeros (up
 * to 32 bits).
 * 
 */
#define F_SHORT_UH32(intVal) "0x" \
  << std::hex << unsigned(intVal)
  // Convert `intVal` to an integer in case it's uint8_t

/**
 * @brief Format as an unsigned hexadecimal integer without leading zeros (up
 * to 64 bits).
 * 
 */
#define F_SHORT_UH64(intVal) "0x" \
  << std::hex << (uint64_t) intVal

#define F_H32(intVal) (intVal < 0 ? "-" : "") \
  << F_UH32(_abs(intVal))
#define F_H64(intVal) (intVal < 0 ? "-" : "") \
  << F_UH64(_abs(intVal))
#define F_SHORT_H32(intVal) (intVal < 0 ? "-" : "") \
  << F_SHORT_UH32(_abs(intVal))
#define F_SHORT_H64(intVal) (intVal < 0 ? "-" : "") \
  << F_SHORT_UH64(_abs(intVal))

#define F_UINT8(uint8) std::dec << (0xff & unsigned(uint8))

template <typename IT>
class F_HEX {
private:
  const IT n;
public:
  F_HEX(const IT _n): n(_n) {};
  inline friend
  std::ostream &operator<<(std::ostream &os, const F_HEX &self) {
    const IT n = self.n;
    if (isSigned(n)) {
      if (sizeof(n) > 4) os << F_H64(n);
      else os << F_H32(n);
    } else {
      if (sizeof(n) > 4) os << F_UH64(n);
      else os << F_UH32(n);
    }
    return os;
  }
};

template <typename IT>
class F_SHORT_HEX {
private:
  const IT n;
public:
  F_SHORT_HEX(const IT _n): n(_n) {};
  inline friend
  std::ostream &operator<<(std::ostream &os, const F_SHORT_HEX &self) {
    const IT n = self.n;
    if (isSigned(n)) {
      if (sizeof(n) > 4) os << F_SHORT_H64(n);
      else os << F_SHORT_H32(n);
    } else {
      if (sizeof(n) > 4) os << F_SHORT_UH64(n);
      else os << F_SHORT_UH32(n);
    }
    return os;
  }
};

#endif /* C25CB003_C04F_4E40_946A_9E2B7B4FA0F8 */
