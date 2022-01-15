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


/* Memory address translation utilities */

/* Enum types & useful macros */

enum TranslationType {
  /**
   * @brief Guest physical address to guest frame number.
   * 
   * This type of translation never fails for now. Added here for completeness.
   * 
   */
  GPA_TO_GFN,
  /**
   * @brief Kernel virtual address to guest physical address.
   * 
   */
  KVA_TO_GPA,
  /**
   * @brief Kernel virtual address to guest frame number.
   * 
   */
  KVA_TO_GFN,
  /**
   * @brief Kernel symbol to kernel virtual address.
   * 
   */
  KSYM_TO_KVA,
  /**
   * @brief Kernel symbol to guest physical address.
   * 
   */
  KSYM_TO_GPA,
  /**
   * @brief Kernel symbol to guest frame number.
   * 
   */
  KSYM_TO_GFN,
};

/* ======== Errors ======== */

class MemoryTranslationError: public MemoryError {
public:
  /**
   * @brief Symbol name to translate (if any) that caused this error.
   * 
   */
  const char *symbol;
  TranslationType type;

  /**
   * @brief Construct a new Memory Translation Error object.
   * 
   * @param _type Translation type.
   * @param _addr The virtual address to translate (if any) that caused this
   * error.
   * @param _symbol The symbol name to translate (if any) that caused this
   * error.
   */
  MemoryTranslationError(
    TranslationType _type,
    addr_t _addr, const char *_symbol
  ): MemoryError(_addr), symbol(_symbol), type(_type) {};
  
  virtual const char *what() const throw() {
    return "Failed to translate memory address";
  }
};

/* ======== Translations ======== */

/**
 * @brief The number of lower bits to discard when calculating the frame
 * number.
 * 
 * For now, this is hardcoded as 12 (x86 specific).
 * 
 * @see https://www.kernel.org/doc/Documentation/virtual/kvm/mmu.txt
 * 
 */
constexpr unsigned int PAGE_SHIFT = 12;

/**
 * @brief Convert a GPA (Guest Physical Address) to GFN (Guest Frame Number).
 * 
 * @param[in] gpa The guest physical address to be translated.
 * @return addr_t The frame number of given guest physical address.
 */
inline addr_t gpaToGFN(addr_t gpa) {
  return gpa >> PAGE_SHIFT;
}

/**
 * @brief Convert a GLA (Guest Linear Address) to the page number (in virtual
 * address space).
 * 
 * @param gla The guest linear address to be translated.
 * @return addr_t The page number of given guest linear address.
 */
inline addr_t glaToPageNum(addr_t gla) {
  return gla >> PAGE_SHIFT;
}

/**
 * @brief Convert a KVA (Kernel Virtual Address) to GPA (Guest Physical
 * Address).
 * 
 * @param[in] vmi 
 * @param[in] kva The guest kernel virtual address to be translated.
 * @return addr_t The guest physical address of given guest kernel virtual
 * address.
 */
inline addr_t kvaToGPA(vmi_instance_t vmi, addr_t kva) {
  addr_t gpa = 0;
  if (vmi_translate_kv2p(vmi, kva, &gpa) == VMI_FAILURE) {
    throw MemoryTranslationError(KVA_TO_GPA, kva, nullptr);
  }
  return gpa;
}

/**
 * @brief Convert a KVA (Kernel Virtual Address) to GFN (Guest Frame Number).
 * 
 * @param[in] vmi 
 * @param[in] kva The guest kernel virtual address to be translated.
 * @return addr_t The frame number of given guest kernel virtual address.
 */
inline addr_t kvaToGFN(vmi_instance_t vmi, addr_t kva) {
  // Translate to GPA first, and then to GFN
  return gpaToGFN(kvaToGPA(vmi, kva));
}

/**
 * @brief Translate a KSYM (Kernel Symbol) to KVA (Kernel Virtual Address).
 * 
 * @param[in] vmi 
 * @param[in] symbol The kernel symbol to be translated.
 * @return addr_t The kernel virtual address of given symbol.
 */
inline addr_t ksymToKVA(vmi_instance_t vmi, const char *symbol) {
  addr_t kva = 0;
  if (vmi_translate_ksym2v(vmi, symbol, &kva) == VMI_FAILURE) {
    throw MemoryTranslationError(KSYM_TO_KVA, 0, symbol);
  }
  return kva;
}

/**
 * @brief Translate a KSYM (Kernel Symbol) to GPA (Guest Physical Address).
 * 
 * @param[in] vmi 
 * @param[in] symbol The kernel symbol to be translated.
 * @return addr_t The guest physical address of given symbol.
 */
inline addr_t ksymToGPA(vmi_instance_t vmi, const char *symbol) {
  // Translate to KVA first, and then to GPA
  return kvaToGPA(vmi, ksymToKVA(vmi, symbol));
}

/**
 * @brief Translate a KSYM (Kernel Symbol) to GFN (Guest Frame Number).
 * 
 * @param[in] vmi 
 * @param[in] symbol The kernel symbol to be translated.
 * @return addr_t The guest frame number of given symbol.
 */
inline addr_t ksymToGFN(vmi_instance_t vmi, const char *symbol) {
  // Translate to GPA first, and then to GFN
  return gpaToGFN(ksymToGPA(vmi, symbol));
}

}
}

#endif /* C7F49205_DA22_46A3_A0D9_8D4123F1D4AE */
