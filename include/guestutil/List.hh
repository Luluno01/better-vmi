/**
 * @file List.hh
 * @author Untitled (gnu.imm@outlook.com)
 * @brief Linux kernel list util (read-only).
 * @version 0.1
 * @date 2021-12-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifndef DA4173E3_91C5_4215_87C4_425D2CE95B52
#define DA4173E3_91C5_4215_87C4_425D2CE95B52

#include <libvmi/libvmi.h>
#include <guestutil/mem.hh>


namespace guestutil {
namespace list {


/**
 * @brief List item referenced by the `struct list_head` member.
 * 
 * ```
 * struct list_head {
 *   struct list_head *next, *prev;
 * };
 * ```
 * 
 * So `next` has the offset 0.
 */
class ListItem {
private:
  addr_t headMember;
public:
  ListItem() {};
  ListItem(addr_t _headMember): headMember(_headMember) {};

  inline ListItem next(vmi_instance_t vmi) {
    return fromAddr(guestutil::memory::readAddrKVA(vmi, headMember + 0));
  }

  inline addr_t getVA() {
    return headMember;
  }

  inline bool operator==(ListItem another) {
    return getVA() == another.getVA();
  }

  inline bool operator!=(ListItem another) {
    return getVA() != another.getVA();
  }

  inline static ListItem fromAddr(addr_t headMember) {
    static_assert(sizeof(ListItem) == sizeof(addr_t));
    return static_cast<ListItem>(headMember);
  }
};

/**
 * @brief Linux kernel list util. (For kernel space only.)
 * This does not change the state of the VM, i.e., it only reads from the
 * guest's kernel memory. However, the caller should pause the VM before doing
 * any read accesses.
 * @see Linux kernel source: include/linux/list.h
 */
class List {
protected:
  /**
   * @brief First list item (referenced by the `struct list_head` member).
   */
  ListItem first;
  /**
   * @brief Offset of the `struct list_head` member of the list item.
   */
  addr_t listHeadOffset;

  List() {};
  inline void init(ListItem _first, addr_t _listHeadOffset) {
    first = _first;
    listHeadOffset = _listHeadOffset;
  }
public:
  List(ListItem _first, addr_t _listHeadOffset) {
    init(_first, _listHeadOffset);
  };

  List(addr_t _first, addr_t _listHeadOffset) {
    init(ListItem::fromAddr(_first), _listHeadOffset);
  }

  inline ListItem getFirst() {
    return first;
  }

  inline addr_t getListHeadOffset() {
    return listHeadOffset;
  }

  inline bool isEmpty(vmi_instance_t vmi) {
    return getFirst().next(vmi) == getFirst();
  }

  inline addr_t getObjectAddr(ListItem &item) {
    return item.getVA() - getListHeadOffset();
  }

  inline addr_t getMemberAddr(ListItem &item, addr_t offset) {
    return getObjectAddr(item) + offset;
  }

  inline addr_t getMemberAddr(addr_t objAddr, addr_t offset) {
    return objAddr + offset;
  }

  /**
   * @brief Iterate through each item in the list. Stop when `action` returns
   * `true`. It's the caller's responsibility to pause the VM if necessary.
   * 
   * @tparam F callback function type, `bool (ListItem currentItem)`.
   * @param vmi 
   * @param action callback function. Return truthy value to break from the
   * iteration.
   */
  template<typename F>
  inline void forEach(vmi_instance_t vmi, F action) {
    for (
      ListItem pos = getFirst().next(vmi);
      pos != getFirst();
      pos = pos.next(vmi)
    ) {
      if (action(pos)) break;
    }
  }
};

}
}

#endif /* DA4173E3_91C5_4215_87C4_425D2CE95B52 */
