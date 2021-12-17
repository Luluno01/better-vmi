#ifndef B8472E02_29E2_4814_885E_E04B8368B908
#define B8472E02_29E2_4814_885E_E04B8368B908

#include <libvmi/libvmi.h>
#include <guestutil/List.hh>
#include <guestutil/mem.hh>
#include <guestutil/symbol.hh>
#include <guestutil/offset.hh>
#include <string>


namespace guestutil {
namespace process {

class ProcessList: public list::List {
protected:
  addr_t nameOffset;
  addr_t pidOffset;
public:
  ProcessList() = default;
  ProcessList(addr_t initTask,
    addr_t tasksOffset, addr_t _nameOffset, addr_t _pidOffset)
      :guestutil::list::List(initTask + tasksOffset, tasksOffset),
        nameOffset(_nameOffset), pidOffset(_pidOffset) {};

  static inline ProcessList fromVMI(vmi_instance_t vmi) {
    addr_t initTaskAddr = symbol::translateKernelSymbol(vmi, "init_task");
    addr_t tasksOffset = offset::getOffset(vmi, "linux_tasks");
    addr_t nameOffset = offset::getOffset(vmi, "linux_name");
    addr_t pidOffset = offset::getOffset(vmi, "linux_pid");
    ProcessList list(initTaskAddr, tasksOffset, nameOffset, pidOffset);
    return list;
  }

  inline addr_t getNameOffset() {
    return nameOffset;
  }

  inline addr_t getPidOffset() {
    return pidOffset;
  }

  inline std::string name(vmi_instance_t vmi, list::ListItem &proc) {
    addr_t nameAddr = getMemberAddr(proc, getNameOffset());
    return memory::readCppStrKVA(vmi, nameAddr);
  }

  inline vmi_pid_t pid(vmi_instance_t vmi, list::ListItem &proc) {
    addr_t pidAddr = getMemberAddr(proc, getPidOffset());
    return memory::read32KVA<vmi_pid_t>(vmi, pidAddr);
  }
};

}
}

#endif /* B8472E02_29E2_4814_885E_E04B8368B908 */
