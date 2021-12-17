/**
 * @file TempMem.hh
 * @author Untitled (gnu.imm@outlook.com)
 * @brief Helper class for managing the life of temporary memory modification.
 * @version 0.1
 * @date 2021-12-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifndef A7378944_FA81_459C_B281_9A161BFFC4E0
#define A7378944_FA81_459C_B281_9A161BFFC4E0

#include <libvmi/libvmi.h>
#include <exception>
#include <debug.hh>


namespace guestutil {
namespace memory {


class TempMemError: public std::exception {};

class TempMemAddrError: public TempMemError {
public:
  virtual const char *what() const throw() {
    return "Address must not be null";
  }
};

class TempMemAlreadyAppliedError: public TempMemError {
public:
  virtual const char *what() const throw() {
    return "Modification is already applied";
  }
};

/**
 * @brief Temporary memory modification helper class. Destruction of this will
 * undo the temporary modification.
 * 
 * Note that this implementation DOES NOT
 * 
 * 1. Handle read/write accesses to the same memory region by the guest.
 * 2. Handle address space translation (this rely on the memory reading and
 *    writing functions provided by the caller as template arguments).
 * 
 * Example usage:
 * 
 * ```C++
 * vmi_instance_t vmi;
 * // ...
 * memory::TempMem<
 *   uint8_t,
 *   memory::Read8KVA<uint8_t>,
 *   memory::Write8KVA<uint8_t>
 * > tmpMem {
 *   memory::Read8KVA<uint8_t>(vmi),
 *   memory::Write8KVA<uint8_t>(vmi)
 * };
 * ```
 * 
 * @tparam IT primitive type of the modification data, e.g., uint8_t.
 * @tparam ReadFn the memory reading function bound to a specific VMI instance,
 * e.g., `guestutil::memory::Read8KVA(&vmi)`.
 * @tparam WriteFn the memory writing function bound to a specific VMI
 * instance, e.g., `guestutil::memory::Read8KVA(&vmi)`.
 */
template<typename IT, typename ReadFn, typename WriteFn>
class TempMem {
private:
  /**
   * @brief Guest address of the modification made (used later to undo the
   * modification). This could be physical, virtual, kernel or non-kernel
   * depending on the readFn and writeFn provided.
   */
  addr_t addr;
  /**
   * @brief Saved old value of target memory region.
   * 
   */
  IT oldVal;
  /**
   * @brief The memory reading function bound to a specific VMI instance.
   * (Expecting compiler inlining optimization.)
   * 
   */
  ReadFn readFn;
  /**
   * @brief The memory writing function bound to a specific VMI instance.
   * (Expecting compiler inlining optimization.)
   * 
   */
  WriteFn writeFn;
public:
  TempMem() = delete;
  TempMem(ReadFn _readFn, WriteFn _writeFn):
    addr(0), oldVal(0),
    readFn(_readFn), writeFn(_writeFn) {};

  /**
   * @brief Use `readFn` to get and save the old value for later restoration,
   * then apply the modification.
   * 
   * @param addr 
   * @param val 
   * @return IT the old value
   */
  inline IT apply(addr_t addr, IT &val) {
    DBG() << "TempMem.apply(addr, val)" << std::endl
          << "  addr      : " << F_PTR(addr) << std::endl
          << "  val       : " << F_HEX(val) << std::endl
          << "  this->addr: " << F_PTR(this->addr) << std::endl;
    if (!addr) throw TempMemAddrError();
    if (this->addr != 0) throw TempMemAlreadyAppliedError();
    oldVal = readFn(addr);
    DBG() << "  oldVal    : " << F_HEX(oldVal) << std::endl;
    writeFn(addr, val);
    this->addr = addr;
    return oldVal;
  }

  inline IT getOldVal() {
    return oldVal;
  }

  /**
   * @brief Undo the modification made.
   * 
   * 
   * @return bool if modification is undone (`false` means the modification was
   * never made or unsuccessful).
   */
  inline bool undo() {
    if (addr) {
      DBG() << "TempMem.undo()" << std::endl
            << "  addr  : " << F_PTR(addr) << std::endl
            << "  oldVal: " << F_HEX(oldVal) << std::endl;
      writeFn(addr, oldVal);
      addr = 0;
      return true;
    }
    return false;
  }

  ~TempMem() {
    undo();
  }
};

#define UINT_N_TEMP_MEM_ALIAS_NAME(size) TempMemUInt ## size

#define DEFINE_UINT_N_TEMP_MEM(size) \
  using UINT_N_TEMP_MEM_ALIAS_NAME(size) = TempMem< \
    UINT_N_T(size), \
    READ_UINT_N_KVA_ALIAS_NAME(size), \
    WRITE_UINT_N_KVA_ALIAS_NAME(size) \
  >

DEFINE_UINT_N_TEMP_MEM(8);
DEFINE_UINT_N_TEMP_MEM(16);
DEFINE_UINT_N_TEMP_MEM(32);
DEFINE_UINT_N_TEMP_MEM(64);


}
}


#endif /* A7378944_FA81_459C_B281_9A161BFFC4E0 */
