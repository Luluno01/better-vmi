#ifndef C25CB003_C04F_4E40_946A_9E2B7B4FA0F8
#define C25CB003_C04F_4E40_946A_9E2B7B4FA0F8


#include <config.h>
#include <iostream>
#include <iomanip>


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

/**
 * @brief Format as a hexadecimal integer.
 * 
 */
#define F_HEX(intVal) "0x" \
  << std::setfill('0') << std::setw(sizeof(intVal) * 2) \
  << std::right << std::hex << unsigned(intVal)
  // Convert `intVal` to an integer in case it's uint8_t

#define F_UINT8(uint8) std::dec << (0xff & unsigned(uint8))


#endif /* C25CB003_C04F_4E40_946A_9E2B7B4FA0F8 */
