/**
 * @file layout.hh
 * @author Untitled (gnu.imm@outlook.com)
 * @brief The memory layout (hardcoded for x86-64 Linux for now).
 * @version 0.1
 * @date 2022-01-15
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef CA06B561_704F_416D_8462_58EF12B60E6C
#define CA06B561_704F_416D_8462_58EF12B60E6C


#include <libvmi/libvmi.h>

#include <guestutil/mem/translation.hh>
#include <debug.hh>

#include <vector>
#include <functional>  // std::function


namespace guestutil {
namespace memory {
namespace layout {


/**
 * @brief Represent an address range.
 * 
 */
class Range {
private:
  addr_t base;
  addr_t end;
  addr_t size;
public:
  explicit Range(addr_t _base, nullptr_t, addr_t _size):
    base(_base), end(_base + size), size(_size) {};

  explicit Range(addr_t _base, addr_t _end, nullptr_t):
    base(_base), end(_end), size(_end - _base) {};

  /**
   * @brief Get the base address (inclusive) of the range.
   * 
   * @return addr_t 
   */
  inline addr_t getBase() const {
    return base;
  }

  /**
   * @brief Get the end address (exclusive) of the range.
   * 
   * @return addr_t 
   */
  inline addr_t getEnd() const {
    return end;
  }

  /**
   * @brief Get the size of the range.
   * 
   * @return addr_t 
   */
  inline addr_t getSize() const {
    return size;
  }

  inline friend
  std::ostream &operator<<(std::ostream &os, const Range &self) {
    os << F_HEX<addr_t>(self.base) << ':' << F_SHORT_HEX<addr_t>(self.size);
    return os;
  }
};

/**
 * @brief Represent a virtual memory range.
 * 
 */
class VirtRange: public Range {
public:
  using Range::Range;

  /**
   * @brief Get the page number to which the base address belongs (inclusive).
   * 
   * @return addr_t 
   */
  inline addr_t getStartPageNum() {
    return translation::VirtAddr(getBase()).toPageNum().get();
  }

  /**
   * @brief Get the end page number (not the base of the page) of this range
   * (exclusive, returned value - 1 is the last page touched by this range).
   * 
   * @return addr_t 
   */
  inline addr_t getEndPageNum() {
    addr_t end = getEnd();
    translation::PageNum endPage = translation::VirtAddr(end).toPageNum();
    addr_t endPageBase = endPage.toVirtAddr();
    // If the end address (exclusive) is at the beginning of a page,
    // that entire page is not touched by this range
    // Otherwise, at least a part of that page is used by this range
    return end == endPageBase ? endPage.get() : (endPage.get() + 1);
  }

  /**
   * @brief Get the number of pages touched by this range.
   * 
   * @return unsigned int 
   */
  inline addr_t getPages() {
    return getEndPageNum() - getStartPageNum();
  }

  /**
   * @brief Iterate over all page numbers touched by this range.
   * 
   * @param action the action to do to each page number, return `true` to
   * break.
   */
  inline void forEachPageNum(std::function<bool(addr_t)> action) {
    addr_t startPage = getStartPageNum();
    addr_t endPage = getEndPageNum();
    for (addr_t pageNum = startPage; pageNum < endPage; pageNum++) {
      if (action(pageNum)) break;
    }
  }

  /**
   * @brief Get a vector of page numbers touched by this range.
   * 
   * @return std::vector<addr_t> 
   */
  inline std::vector<addr_t> getPageNums() {
    std::vector<addr_t> pageNums;
    forEachPageNum([&pageNums](addr_t pageNum) {
      pageNums.push_back(pageNum);
      return false;
    });
    return pageNums;
  }

  /**
   * @brief Iterate over all GFNs touched by this range (kernel space only for
   * now).
   * 
   * @param vmi 
   * @param action the action to do to each GFN, return `true` to break.
   */
  inline void forEachGFN(
    vmi_instance_t vmi,
    std::function<bool(addr_t)> action
  ) {
    forEachPageNum([vmi, &action](addr_t pageNum) {
      return action(translation::PageNum(pageNum).toGFN(vmi));
    });
  }
};


}
}
}


#endif /* CA06B561_704F_416D_8462_58EF12B60E6C */
