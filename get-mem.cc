#include <iostream>

#include <libvmi/libvmi.h>
#include <libvmi/events.h>

#include <guestutil/VM.hh>
#include <guestutil/mem.hh>
#include <guestutil/mem/layout.hh>

#include <debug.hh>


using namespace guestutil;


template <bool verbose>
void printRange(vmi_instance_t vmi, memory::layout::VirtRange &range) {
  int i = 0;
  int mapped = 0;
  range.forEachPageNum([vmi, &i, &mapped](addr_t pageNum) {
    memory::translation::PageNum pn(pageNum);
    try {
      auto gfn = pn.toGFN(vmi);
      if (verbose) std::cout << F_DEC(i) << '\t' << pn.toVirtAddr() << " => " << gfn << std::endl;
      mapped++;
    } catch (std::exception &e) {
      ;  // Do nothing
    }
    i++;
    return false;
  });
  std::cout << "Mapped pages: " << F_DEC(mapped) << std::endl;
}

int main() {
  vm::VM vm("debian11", VMI_INIT_EVENTS);
  std::cout << "VMI initialized." << std::endl;

  // Resume in case it is already paused
  vm.tryResume();

  vmi_instance_t vmi = vm.getVMI();

  vm.pause();
  std::cout << "VM temporarily paused" << std::endl;

  std::cout << "Target VM ID: " << vm.id() << std::endl;

  addr_t kva = memory::ksymToKVA(vmi, "init_task");
  std::cout << F_HEX(kva) << std::endl;

  unsigned int nCPUs = vm.numVCPUs();
  uint64_t value = 0xffffffffffffffff;
  for (unsigned int vCPU = 0; vCPU < nCPUs; vCPU++) {
    std::cout << F_DEC(vCPU) << ' ';
    if (vmi_get_vcpureg(vm.getVMI(), &value, GDTR_BASE, vCPU) == VMI_FAILURE) {
      std::cout << "?:";
    } else {
      std::cout << F_SHORT_HEX(value) << ':';
    }
    if (vmi_get_vcpureg(vm.getVMI(), &value, GDTR_LIMIT, vCPU) == VMI_FAILURE) {
      std::cout << "?";
    } else {
      std::cout << F_SHORT_HEX(value);
    }
    std::cout << std::endl;
  }

  memory::layout::VirtRange text(0xffffffff80000000ull, 0xffffffff9fffffffull + 1, nullptr);
  memory::layout::VirtRange modules(0xffffffffa0000000ull, 0xfffffffffeffffffull + 1, nullptr);
  printRange<true>(vmi, text);
  printRange<true>(vmi, modules);
  // In case the lookup triggers events?
  std::cout << "Pending events: " << vmi_are_events_pending(vmi) << std::endl;

  vm.tryResume();

  return 0;
}
