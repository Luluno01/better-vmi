#include <iostream>
#include <exception>
#include <memory>
#include <libvmi/libvmi.h>

#include <guestutil/VM.hh>
#include <guestutil/ProcessList.hh>
#include <guestutil/mem.hh>
#include <guestutil/mem/TempMem.hh>
#include <guestutil/breakpoint/Breakpoint.hh>
#include <guestutil/breakpoint/BreakpointRegistry.hh>
#include <guestutil/event/Loop.hh>


using namespace guestutil;

int doTheJob() {
  vm::VM vm("debian11", VMI_INIT_EVENTS);
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

  memory::ReadUInt16KVA reader(vmi);
  memory::WriteUInt16KVA writer(vmi);
  memory::TempMemUInt16 tmpMem { reader, writer };
  uint16_t magic = 0x0000;
  uint16_t oldVal = reader(swapperProc.getVA());
  if (tmpMem.apply(swapperProc.getVA(), magic) != oldVal) {
    throw std::runtime_error("Wrong old value");
  }
  if (reader(swapperProc.getVA()) != magic) {
    throw std::runtime_error("Wrong temp value");
  }
  tmpMem.undo();
  if (reader(swapperProc.getVA()) != oldVal) {
    throw std::runtime_error("Wrong value after undo()");
  }

  // breakpoint::Breakpoint bp { vmi, symbol::translateKernelSymbol(vmi, "__x64_sys_write") };

  std::cout << "Creating loop" << std::endl;
  event::Loop loop(vm);
  std::cout << "Creating breakpoint registry" << std::endl;
  breakpoint::BreakpointRegistry reg(vmi);
  reg.registerEvent();
  addr_t addrWrite = symbol::translateKernelSymbol(vmi, "__x64_sys_write");
  addr_t addrRead = symbol::translateKernelSymbol(vmi, "__x64_sys_read");
  int i = 0;
  int j = 0;
  std::cout << "Creating onPause" << std::endl;
  std::shared_ptr<breakpoint::Breakpoint> writeBp;
  std::function<void()> disableWrite = [&writeBp, &vm]() {
    writeBp->disable();
    vm.resume();
  };
  std::function<void()> stopLoop = [&reg, &loop]() {
    reg.disableAll();
    reg.unregisterEvent();
    loop.stop("onPause");
  };
  std::cout << "Setting breakpoint" << std::endl;
  writeBp = reg.setBreakpoint(addrWrite, [&i, &loop, &disableWrite](vmi_event_t *event) {
    std::cout << i << " vCPU " << event->vcpu_id << " hit breakpoint __x64_sys_write @ " << event->interrupt_event.gla << std::endl;
    if (++i == 10) {
      loop.schedulePause(disableWrite, "breakpoint __x64_sys_write");
    }
  });
  writeBp->enable();
  reg.setBreakpoint(addrRead, [&j, &loop, &stopLoop](vmi_event_t *event) {
    std::cout << j << " vCPU " << event->vcpu_id << " hit breakpoint __x64_sys_read @ " << event->interrupt_event.gla << std::endl;
    if (++j == 20) {
      loop.schedulePause(stopLoop, "breakpoint __x64_sys_read");
    }
  })->enable();
  vm.resume();
  event::EventError *err = loop.bump();
  if (err) {
    std::cout << "Event loop exited with error: " << err->what() << std::endl;
  }

  std::cout << "Pending events: " << vmi_are_events_pending(vmi) << std::endl;

  vm.resume();

  // `vm::VM` destructor will do the clean-up
  return 0;
}

int main() {
  try {
    return doTheJob();
  } catch (std::exception &e) {
    std::cout << "An error has occurred" << std::endl;
    throw;
  }
}
