#include <iostream>
#include <exception>
// #include <sys/mman.h>
#include <inttypes.h>

#include <libvmi/libvmi.h>
#include <guestutil/VM.hh>
#include <guestutil/ProcessList.hh>
#include <guestutil/mem.hh>
#include <guestutil/mem/TempMem.hh>
#include <guestutil/breakpoint/Breakpoint.hh>


using namespace guestutil;

int doTheJob() {
  vm::VM vm("debian11");
  std::cout << "VMI initialized." << std::endl;

  // Resume in case it is already paused
  vm.tryResume();

  vmi_instance_t &vmi = vm.getVMI();

  process::ProcessList procList;
  procList = process::ProcessList::fromVMI(vmi);

  // std::cout << "init_task: " << reinterpret_cast<void*>(procList.getFirst())

  vm.pause();
  std::cout << "VM temporarily paused" << std::endl;

  std::cout << "Target VM ID: " << vm.id() << std::endl;

  list::ListItem swapperProc = procList.getFirst();
  std::cout << '[' << std::right << std::setw(5) << procList.pid(vmi, swapperProc) << "] " << procList.name(vmi, swapperProc) << " (->tasks addr: " << reinterpret_cast<void *>(swapperProc.getVA()) << ')' << std::endl;
  procList.forEach(vmi, [&vmi, &procList](list::ListItem procEntry) {
    std::cout << '[' << std::right << std::setw(5) << procList.pid(vmi, procEntry) << "] " << procList.name(vmi, procEntry) << " (->tasks addr: " << reinterpret_cast<void *>(procEntry.getVA()) << ')' << std::endl;
    return false;
  });

  vm.resume();

  // `vm::VM` destructor will do the clean-up
  return 0;
}

int main() {
  try {
    doTheJob();
  } catch (const std::exception &) { throw; }  // Use try-catch to ensure stack variables are properly destructed
}
