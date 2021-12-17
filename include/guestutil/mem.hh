/**
 * @file mem.hh
 * @author Untitled (gnu.imm@outlook.com)
 * @brief Primitive memory operations.
 * @version 0.1
 * @date 2021-12-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifndef C7F49205_DA22_46A3_A0D9_8D4123F1D4AE
#define C7F49205_DA22_46A3_A0D9_8D4123F1D4AE

#include <libvmi/libvmi.h>
#include <exception>
#include <string>

namespace guestutil {
namespace memory {


/* Enum types & useful macros */

enum MemoryReadAccess {
  READ_VA,
  READ_KVA,
  READ_8_KVA,
  READ_16_KVA,
  READ_32_KVA,
  READ_64_KVA,
  READ_ADDR_KVA,
  READ_STR_KVA
};

enum MemoryWriteAccess {
  WRITE_8_KVA,
  WRITE_16_KVA,
  WRITE_32_KVA,
  WRITE_64_KVA
};

#define UINT_N_T(size) uint ## size ## _t
#define I_N(size) I ## size
#define VMI_READ_X_VA(x) vmi_read_ ## x ## _va
#define VMI_WRITE_X_VA(x) vmi_write_ ## x ## _va

/* ======== Errors ======== */

class MemoryError: public std::exception {
public:
  addr_t addr;
  MemoryError(addr_t _addr): addr(_addr) {};

  virtual const char *what() const throw() {
    return "Failed to access guest memory";
  }
};

class MemoryReadError: public MemoryError {
public:
  MemoryReadAccess access;
  MemoryReadError(addr_t addr, MemoryReadAccess _access):
    MemoryError(addr), access(_access) {};

  virtual const char *what() const throw() {
    return "Failed to read guest memory";
  }
};

class MemoryWriteError: public MemoryError {
public:
  MemoryWriteAccess access;
  MemoryWriteError(addr_t addr, MemoryWriteAccess _access):
    MemoryError(addr), access(_access) {};

  virtual const char *what() const throw() {
    return "Failed to write guest memory";
  }
};

/* ======== Read ======== */

/**
 * @brief Read `count` bytes into `buff` from memory located at the virtual
 * address `va` of the process with the PID `pid`.
 * 
 * @param[in] vmi 
 * @param[in] va 
 * @param[in] pid 
 * @param[in] count 
 * @param[out] buff 
 * @return size_t bytes read.
 */
inline size_t readVA(
  vmi_instance_t vmi,
  addr_t va,
  vmi_pid_t pid,
  size_t count,
  void *buff
) {
  size_t bytesRead = 0;
  if (vmi_read_va(vmi, va, pid, count, buff, &bytesRead) == VMI_FAILURE) {
    throw MemoryReadError(va, READ_VA);
  }
  return bytesRead;
}

/**
 * @brief Read `count` bytes into `buff` from memory located at kernel virtual
 * address `kva`.
 * 
 * @param[in] vmi 
 * @param[in] kva 
 * @param[in] count 
 * @param[out] buff 
 */
inline void readKVA(
  vmi_instance_t vmi,
  addr_t kva,
  size_t count,
  void *buff
) {
  size_t bytesRead = 0;
  if (
    vmi_read_va(vmi, kva, 0, count, buff, &bytesRead) == VMI_FAILURE ||
    bytesRead != count  // Magic?
  ) {
    throw MemoryReadError(kva, READ_KVA);
  }
}

/**
 * @brief Read an address (pointer) from kernel virtual address (KVA).
 * 
 * @param[in] vmi 
 * @param[in] kva kernel virtual address of the address (pointer) to be read.
 * @tparam _ dummy template argument.
 * @return addr_t 
 */
template <typename _ = addr_t>
inline addr_t readAddrKVA(vmi_instance_t vmi, addr_t kva) {
  addr_t res = 0;
  if (vmi_read_addr_va(vmi, kva, 0, &res) == VMI_FAILURE) {
    throw MemoryReadError(kva, READ_ADDR_KVA);
  }
  return res;
}

#define READ_X_KVA_FN_NAME(x) read ## x ## KVA
#define READ_X_KVA_CLS_NAME(x) Read ## x ## KVA
#define READ_X_KVA_ACC_NAME(x) READ_ ## x ## _KVA

/**
 * @brief Read bits from kernel virtual address (KVA).
 * 
 */
#define DEFINE_READ_N_KVA(size) \
  template <typename I_N(size) = UINT_N_T(size)> \
  inline I_N(size) READ_X_KVA_FN_NAME(size)(vmi_instance_t vmi, addr_t kva) { \
    UINT_N_T(size) res = 0; \
    if (VMI_READ_X_VA(size)(vmi, kva, 0, &res) == VMI_FAILURE) { \
      throw MemoryReadError(kva, READ_X_KVA_ACC_NAME(size)); \
    } \
    return *reinterpret_cast<I_N(size) *>(&res); \
  } \
  template <typename I_N(size) = UINT_N_T(size)> \
  class READ_X_KVA_CLS_NAME(size) { \
  private: \
    vmi_instance_t vmi; \
  public: \
    READ_X_KVA_CLS_NAME(size)(vmi_instance_t _vmi): vmi(_vmi) {}; \
    inline I_N(size) operator()(addr_t kva) { \
      return READ_X_KVA_FN_NAME(size)<I_N(size)>(vmi, kva); \
    } \
  }

DEFINE_READ_N_KVA(8);
DEFINE_READ_N_KVA(16);
DEFINE_READ_N_KVA(32);
DEFINE_READ_N_KVA(64);

#define READ_UINT_N_KVA_ALIAS_NAME(x) ReadUInt ## x ## KVA

/**
 * @brief Read unsigned integer from kernel virtual address (KVA).
 * 
 */
#define DEFINE_READ_UINT_N_KVA_ALIAS(size) \
  using READ_UINT_N_KVA_ALIAS_NAME(size) = READ_X_KVA_CLS_NAME(size)<UINT_N_T(size)>

DEFINE_READ_UINT_N_KVA_ALIAS(8);
DEFINE_READ_UINT_N_KVA_ALIAS(16);
DEFINE_READ_UINT_N_KVA_ALIAS(32);
DEFINE_READ_UINT_N_KVA_ALIAS(64);

/**
 * @brief Read a string from kernel virtual address (KVA). It's the caller's
 * responsibility to free the string.
 * 
 * @param[in] vmi 
 * @param[in] kva kernel virtual address of the string to be read.
 * @return char* 
 */
inline char *readStrKVA(vmi_instance_t vmi, addr_t kva) {
  char *res;
  if (!(res = vmi_read_str_va(vmi, kva, 0))) {
    throw MemoryReadError(kva, READ_STR_KVA);
  }
  return res;
}

inline std::string readCppStrKVA(vmi_instance_t vmi, addr_t kva) {
  char *cStr = readStrKVA(vmi, kva);
  std::string cppStr(cStr);
  delete []cStr;
  return cppStr;
}

/* ======== Write ======== */

#define WRITE_X_KVA_FN_NAME(x) write ## x ## KVA
#define WRITE_X_KVA_CLS_NAME(x) Write ## x ## KVA
#define WRITE_X_KVA_ACC_NAME(x) WRITE_ ## x ## _KVA

/**
 * @brief Write bits by kernel virtual address.
 * 
 */
#define DEFINE_WRITE_N_KVA(size) \
  template <typename I_N(size) = UINT_N_T(size)> \
  inline void WRITE_X_KVA_FN_NAME(size)(vmi_instance_t vmi, addr_t kva, I_N(size) &val) { \
    if (VMI_WRITE_X_VA(size)(vmi, kva, 0, &val) == VMI_FAILURE) { \
      throw MemoryWriteError(kva, WRITE_X_KVA_ACC_NAME(size)); \
    } \
  } \
  template <typename I_N(size) = UINT_N_T(size)> \
  class WRITE_X_KVA_CLS_NAME(size) { \
  private: \
    vmi_instance_t vmi; \
  public: \
    WRITE_X_KVA_CLS_NAME(size)(vmi_instance_t _vmi): vmi(_vmi) {}; \
    inline void operator()(addr_t kva, I_N(size) &val) { \
      WRITE_X_KVA_FN_NAME(size)<I_N(size)>(vmi, kva, val); \
    } \
  }

DEFINE_WRITE_N_KVA(8);
DEFINE_WRITE_N_KVA(16);
DEFINE_WRITE_N_KVA(32);
DEFINE_WRITE_N_KVA(64);

#define WRITE_UINT_N_KVA_ALIAS_NAME(x) WriteUInt ## x ## KVA

/**
 * @brief Read unsigned integer from kernel virtual address (KVA).
 * 
 */
#define DEFINE_WRITE_UINT_N_KVA_ALIAS(size) \
  using WRITE_UINT_N_KVA_ALIAS_NAME(size) = WRITE_X_KVA_CLS_NAME(size)<UINT_N_T(size)>

DEFINE_WRITE_UINT_N_KVA_ALIAS(8);
DEFINE_WRITE_UINT_N_KVA_ALIAS(16);
DEFINE_WRITE_UINT_N_KVA_ALIAS(32);
DEFINE_WRITE_UINT_N_KVA_ALIAS(64);

}
}

#endif /* C7F49205_DA22_46A3_A0D9_8D4123F1D4AE */
