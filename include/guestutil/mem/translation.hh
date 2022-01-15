/**
 * @file translation.hh
 * @author Untitled (gnu.imm@outlook.com)
 * @brief Wrapping classes for address translation (kernel space only for now).
 * @version 0.1
 * @date 2022-01-15
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef B01DE1A0_CBEE_447C_ABE4_D413F625A7D1
#define B01DE1A0_CBEE_447C_ABE4_D413F625A7D1


#include <libvmi/libvmi.h>

#include <guestutil/mem.hh>
#include <debug.hh>

#include <stdexcept>


namespace guestutil {
namespace memory {
namespace translation {


/**
 * @brief Base class of memory addresses.
 * 
 */
class Addr {
protected:
  addr_t addr;
public:
  explicit Addr(addr_t _addr): addr(_addr) {};

  /**
   * @brief Get the raw address value.
   * 
   * @return addr_t 
   */
  inline addr_t get() const {
    return addr;
  }

  inline operator addr_t() const {
    return addr;
  }

  template <typename AT1, typename AT2>
  inline friend AT1 operator+(const AT1 src, const AT2 plus) {
    static_assert(std::is_base_of<Addr, AT1>::value,
      "Left operand must be an Addr");
    return AT1(src.addr + plus);
  }

  template <typename AT1, typename AT2>
  inline friend AT1 operator-(const AT1 src, const AT2 plus) {
    static_assert(std::is_base_of<Addr, AT1>::value,
      "Left operand must be an Addr");
    return AT1(src.addr - plus);
  }

  template <typename AT>
  inline friend AT &operator+=(AT &src, const addr_t plus) {
    static_assert(std::is_base_of<Addr, AT>::value,
      "left operand must be an Addr");
    src.addr += plus;
    return src;
  }

  template <typename AT>
  inline friend AT &operator-=(AT &src, const addr_t plus) {
    static_assert(std::is_base_of<Addr, AT>::value,
      "left operand must be an Addr");
    src.addr -= plus;
    return src;
  }

  inline friend
  std::ostream &operator<<(std::ostream &os, const Addr &self) {
    os << F_HEX<addr_t>(self.get());
    return os;
  }
};

class VirtAddr;
class PhyAddr;
class PageNum;
class GFN;

/**
 * @brief Guest virtual address.
 * 
 */
class VirtAddr: public Addr {
public:
  using Addr::Addr;

  inline PhyAddr toPhyAddr(vmi_instance_t vmi);
  inline PageNum toPageNum();
  inline GFN toGFN(vmi_instance_t vmi);
};

/**
 * @brief Guest physical address.
 * 
 */
class PhyAddr: public Addr {
public:
  using Addr::Addr;

  inline VirtAddr toVirtAddr();
  inline PageNum toPageNum();
  inline GFN toGFN();
};

/**
 * @brief **Virtual** page number.
 * 
 */
class PageNum: public Addr {
public:
  using Addr::Addr;

  inline VirtAddr toVirtAddr();
  inline VirtAddr toVirtAddr(addr_t offset);
  inline PhyAddr toPhyAddr(vmi_instance_t vmi);
  inline PhyAddr toPhyAddr(vmi_instance_t vmi, addr_t offset);
  inline GFN toGFN(vmi_instance_t vmi);

  inline friend
  std::ostream &operator<<(std::ostream &os, const PageNum &self) {
    os << F_SHORT_HEX<addr_t>(self.get());
    return os;
  }
};

/**
 * @brief Guest frame number of **physical** memory.
 * 
 */
class GFN: public Addr {
public:
  using Addr::Addr;

  inline VirtAddr toVirtAddr();
  inline VirtAddr toVirtAddr(addr_t offset);
  inline PhyAddr toPhyAddr();
  inline PhyAddr toPhyAddr(addr_t offset);
  inline PageNum toPageNum();

  inline friend
  std::ostream &operator<<(std::ostream &os, const GFN &self) {
    os << F_SHORT_HEX<addr_t>(self.get());
    return os;
  }
};


inline PhyAddr VirtAddr::toPhyAddr(vmi_instance_t vmi) {
  return PhyAddr(kvaToGPA(vmi, addr));
}

inline PageNum VirtAddr::toPageNum() {
  return PageNum(glaToPageNum(addr));
}

inline GFN VirtAddr::toGFN(vmi_instance_t vmi) {
  return toPhyAddr(vmi).toGFN();
}

inline VirtAddr PhyAddr::toVirtAddr() {
  throw std::runtime_error("Not implemented");
}

inline GFN PhyAddr::toGFN() {
  return GFN(gpaToGFN(addr));
}

inline VirtAddr PageNum::toVirtAddr() {
  return VirtAddr(addr << PAGE_SHIFT);
}

inline VirtAddr PageNum::toVirtAddr(addr_t offset) {
  return toVirtAddr() + offset;
}

inline PhyAddr PageNum::toPhyAddr(vmi_instance_t vmi) {
  return toVirtAddr().toPhyAddr(vmi);
}

inline PhyAddr PageNum::toPhyAddr(vmi_instance_t vmi, addr_t offset) {
  return toVirtAddr().toPhyAddr(vmi) + offset;
}

inline GFN PageNum::toGFN(vmi_instance_t vmi) {
  return toVirtAddr().toPhyAddr(vmi).toGFN();
}

inline VirtAddr GFN::toVirtAddr() {
  throw std::runtime_error("Not implemented");
}

inline VirtAddr GFN::toVirtAddr(addr_t offset) {
  return toVirtAddr() + offset;
}

inline PhyAddr GFN::toPhyAddr() {
  return PhyAddr(addr >> PAGE_SHIFT);
}

inline PhyAddr GFN::toPhyAddr(addr_t offset) {
  return toPhyAddr() + offset;
}

inline PageNum GFN::toPageNum() {
  throw std::runtime_error("Not implemented");
}


}
}
}


#endif /* B01DE1A0_CBEE_447C_ABE4_D413F625A7D1 */
