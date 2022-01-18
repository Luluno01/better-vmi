#include <iostream>
#include <exception>
#include <memory>

#include <libvmi/libvmi.h>
#include <libvmi/events.h>

#include <guestutil/VM.hh>
#include <guestutil/mem.hh>
#include <guestutil/event/Loop.hh>
#include <guestutil/event/MemEventRegistry.hh>
#include <EventEmitter.hh>
#include <signal.hh>

#include <debug.hh>
#include <pretty-print.hh>


using namespace guestutil;


class MemEventCallback: public EventCallback<vmi_instance_t, vmi_event_t*> {
private:
  event::Loop &loop;
  event::memory::MemEventRegistry &reg;
  int times;
  bool interrupted;
public:
  MemEventCallback(
    event::Loop &_loop,
    event::memory::MemEventRegistry &_reg,
    std::shared_ptr<event::memory::MemEvent> memEvent
  ):
    EventCallback<vmi_instance_t, vmi_event_t*>(false),
    loop(_loop), reg(_reg), times(0), interrupted(false) {
    memEvent->once(event::memory::MemEventKey::UNREGISTERED, [this](vmi_instance_t, vmi_event_t*) {
      std::cout << "Memory event unregistered, pausing the event loop for stop" << std::endl;
      loop.schedulePause([this]() {
        std::cout << "Event loop paused and drained, stopping it" << std::endl;
        std::cout << "Memory event triggered " << F_D32(times) << " times" << std::endl;
        loop.stop("MemEventCallback");
      }, "MemEventCallback");
    }, "MemEventCallback::onUnregistered");
    addr_t gfn = memEvent->getGFN();
    SignalSource::getSignalSource().once(0, [this, gfn](int) {
      if (interrupted) return;
      interrupted = true;
      std::cout << "Unregistering memory event" << std::endl;
      reg.unregisterForGFN(gfn);
    }, "MemEventCallback::onInterrupted");
  };

  virtual void operator()(vmi_instance_t, vmi_event_t *event) {
    auto vmiMemEvent = event->mem_event;
    std::cout << F_D32(times) << '\t'
      << F_D32(event->vcpu_id) << '\t'
      << std::left << std::setfill(' ') << std::setw(3) << pp::MemoryAccess(vmiMemEvent.out_access) << ' '
      << F_UH64(vmiMemEvent.gla) << std::endl;
    times++;
  }

  virtual std::string toString() {
    return "MemEventCallback in altp2m-mem-event";
  }
};

int doTheJob() {
  vm::VM vm("debian11", VMI_INIT_EVENTS);
  std::cout << "VMI initialized." << std::endl;

  // Resume in case it is already paused
  vm.tryResume();

  vmi_instance_t vmi = vm.getVMI();

  vm.pause();
  std::cout << "VM temporarily paused" << std::endl;

  std::cout << "Target VM ID: " << vm.id() << std::endl;

  std::cout << "Creating loop" << std::endl;
  event::Loop loop(vm);

  std::cout << "Creating memory event registry" << std::endl;
  event::memory::MemEventRegistry reg(vmi);
  reg.init();
  std::cout << "MemEventRegistry initialized" << std::endl;

  addr_t gfn = memory::ksymToGFN(vmi, "init_task");

  std::function<void()> unregisterMemEvent = [&reg, gfn]() {
    reg.unregisterForGFN(gfn);
  };

  auto memEvent = reg.registerForGFN(gfn);
  memEvent->on(event::memory::MemEventKey::BEFORE, std::make_shared<MemEventCallback>(loop, reg, memEvent));

  vm.resume();
  try {
    event::EventError *err = loop.bump();
    if (err) {
      std::cout << "Event loop exited with error: " << err->what() << std::endl;
    }
  } catch (event::EventError &e) {
    std::cout << "Event loop errored: " << e.what() << std::endl;
  }

  std::cout << "Pending events: " << vmi_are_events_pending(vmi) << std::endl;

  vm.tryResume();

  // `vm::VM` destructor will do the clean-up
  return 0;
}

int main() {
  SignalSource::getSignalSource().init();
  try {
    return doTheJob();
  } catch (std::exception &e) {
    std::cout << "An error has occurred" << std::endl;
    throw;
  }
}
