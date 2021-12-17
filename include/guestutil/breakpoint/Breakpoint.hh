/**
 * @file Breakpoint.hh
 * @author Untitled (gnu.imm@outlook.com)
 * @brief Breakpoint utilities.
 * @version 0.1
 * @date 2021-12-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifndef B67F90ED_CE43_4E58_9FA5_7A4CD43B0323
#define B67F90ED_CE43_4E58_9FA5_7A4CD43B0323

#include <libvmi/libvmi.h>
#include <libvmi/events.h>
#include <guestutil/mem.hh>
#include <guestutil/mem/TempMem.hh>
#include <guestutil/event/data.hh>
#include <guestutil/breakpoint/Instruction.hh>
#include <cstdint>  // uintptr_t
#include <map>  // std::map
#include <functional>  // std::function
#include <memory>  // std::shared_ptr
#include <exception>
#include <debug.hh>
#include <pretty-print.hh>


namespace guestutil {
namespace breakpoint {


class BreakpointRegistry;

uint8_t breakpointInstruction = 0xCC;

/**
 * @brief Software breakpoint for kernel space. The x86 instruction 0xCC (int3)
 * is used. This is a capture-all implementation, CR3 filtering not included.
 * 
 * Note that the breakpoint does not take care of bumping the event loop. All
 * it does is, when enabled, inject the breakpoint, register proper callback,
 * and recover the instruction before destruction or when explicitly disabled.
 * 
 */
class Breakpoint {
private:
  vmi_instance_t vmi;
  /**
   * @brief Target kernel virtual address.
   * 
   */
  addr_t addr;
  /**
   * @brief The instruction emulation struct.
   * 
   */
  emul_insn_t emul;
  /**
   * @brief If this breakpoint is enabled.
   * 
   */
  bool enabled;
protected:
  /**
   * @brief Callback called by breakpoint registry on an INT3 event that
   * matches the breakpoint address.
   * 
   */
  std::function<void(vmi_event_t*)> onHit;
  // virtual void onHit(vmi_event_t *event) = 0;
public:
  /**
   * @brief Construct a new `Breakpoint` object. Called by
   * `BreakpointRegistry::setBreakpoint`.
   * 
   * @param _vmi 
   * @param _addr 
   * @param _onHit 
   */
  Breakpoint(
    vmi_instance_t _vmi,
    addr_t _addr,
    std::function<void(vmi_event_t*)> _onHit
  ):
    vmi(_vmi),
    addr(_addr),
    emul{
      .dont_free = 1,  /* Because it's part of this object */
      ._pad = {0},
      .data = {0}
    },
    enabled(false),
    onHit(_onHit) {};

  inline addr_t getAddr() {
    return addr;
  }

  inline bool isEnabled() {
    return enabled;
  }

  /**
   * @brief Enable this breakpoint by injecting a software breakpoint
   * instruction.
   * 
   */
  inline void enable() {
    DBG() << "Breakpoint.enable()" << std::endl
          << "  addr: " << F_PTR(addr) << std::endl;
    readInstruction<CS_ARCH_X86, CS_MODE_64>(vmi, addr, 0, emul.data);
    memory::write8KVA(vmi, addr, breakpointInstruction);
    enabled = true;
  }

  /**
   * @brief Disable this breakpoint by removing the injected breakpoint
   * instruction if any. Do nothing if the injection has not been done.
   * 
   */
  inline void disable() {
    if (enabled) {
      DBG() << "Breakpoint.disable()" << std::endl
            << "  addr        : " << F_PTR(addr) << std::endl
            << "  emul.data[0]: " << F_HEX(emul.data[0]) << std::endl;
      memory::write8KVA(vmi, addr, emul.data[0]);
      enabled = false;
    }
  }

  ~Breakpoint() {
    disable();
  }

  friend class BreakpointRegistry;
};

class BreakpointRegistryError: public std::exception {};

class BreakpointAlreadySetError: public BreakpointRegistryError {
public:
  virtual const char *what() const throw() {
    return "A breakpoint at the same address is already set";
  }
};

class BreakpointEventRegistrationError: public BreakpointRegistryError {
public:
  virtual const char *what() const throw() {
    return "Failed to register a breakpoint event";
  }
};

class BreakpointEventAlreadyRegisteredError: public BreakpointRegistryError {
public:
  virtual const char *what() const throw() {
    return "The breakpoint event is already registered";
  }
};

class BreakpointEventNotRegisteredError: public BreakpointRegistryError {
public:
  virtual const char *what() const throw() {
    return "The breakpoint event is not yet registered";
  }
};

const char BreakpointRegistryName[] = "BreakpointRegistry";
const uint32_t BreakpointRegistryTID = \
  reinterpret_cast<std::uintptr_t>(BreakpointRegistryName);

/**
 * @brief Per `vmi_instance_t` breakpoint registry.
 * 
 */
class BreakpointRegistry {
private:
  vmi_instance_t vmi;
  /**
   * @brief Breakpoint objects.
   * 
   */
  std::map<addr_t, std::shared_ptr<Breakpoint>> bps;
  /**
   * @brief The capture-all INT3 event object we are going to use.
   * 
   */
  vmi_event_t *event;

  /**
   * @brief The callback for `vmi_clear_event` that frees the event
   * 
   * @param event 
   * @param rc 
   */
  static void doAfterClearEvent(vmi_event_t *event, status_t rc) {
    DBG() << "BreakpointRegistry::doAfterClearEvent()" << std::endl;
    if (rc == VMI_FAILURE) {
      throw std::runtime_error("Failed to clear breakpoint event");
    } else {
      // Allocated in `registerEvent`
      try {
        delete &event::EventData<BreakpointRegistry>::fromEvent(typeId, event);
      } catch (event::EventDataError &err) {
        delete event;  // Free the event anyway
        throw;
      }
      // Allocated in `registerEvent`
      delete event;
    }
  }

  /**
   * @brief The actual INT3 handler. Core logic goes here.
   * 
   * @param vmi 
   * @param event 
   * @return event_response_t 
   */
  static event_response_t onInt3(vmi_instance_t vmi, vmi_event_t *event) {
    (void) vmi;  /* Unused because we already have one in
                    `EventData<BreakpointRegistry> event->data` */
    BreakpointRegistry &reg = BreakpointRegistry::fromEvent(event);
    auto &bps = reg.bps;
    auto &intEvent = event->interrupt_event;
    auto bpIter = bps.find(intEvent.gla);
    if (bpIter == bps.end()) {
      // Not correspond to any registered breakpoint, reinject the INT3 event
      // to the guest
      intEvent.reinject = 1;
      // We are using at least Xen 4.11, so no need to fix
      // `event->interrupt_event.insn_length`.
      // See libvmi/examples/breakpoint-emulate-example.c
      return VMI_EVENT_RESPONSE_NONE;
    }
    // Otherwise, this event is triggered by our breakpoint
    intEvent.reinject = 0;
    auto bp = bpIter->second;
    // Invoke the callback
    bp->onHit(event);
    // Emulate original instruction;
    event->emul_insn = &(bp->emul);
    // Again, we are using at least Xen 4.11, blah blah blah (see above)
    return VMI_EVENT_RESPONSE_SET_EMUL_INSN;
  }
protected:
public:
  static uint32_t typeId;

  inline static BreakpointRegistry &fromEvent(vmi_event_t *event) {
    return event::EventData<BreakpointRegistry>::getPayloadFromEvent(
      typeId, event);
  }

  BreakpointRegistry() = delete;

  BreakpointRegistry(vmi_instance_t _vmi): vmi(_vmi), bps(), event(nullptr) {};

  /**
   * @brief Register the INT3 event.
   * 
   */
  inline void registerEvent() {
    DBG() << "BreakpointRegistry::registerEvent()" << std::endl;
    if (event) {
      throw BreakpointEventAlreadyRegisteredError();
    }
    // Freed in `doAfterClearEvent`
    event = new vmi_event_t();
    DBG() << "  new vmi_event_t: " << event << std::endl;
    SETUP_INTERRUPT_EVENT(event, onInt3);
    // Freed in `doAfterClearEvent`
    event->data = new event::EventData<BreakpointRegistry>(typeId, *this);
    // DBG() << pp::Event<VMI_EVENT_INTERRUPT, 0>(event) << std::endl;
    if (vmi_register_event(vmi, event) == VMI_FAILURE) {
      delete reinterpret_cast<event::EventData<BreakpointRegistry>*>(
        event->data);
      delete event;
      throw BreakpointEventRegistrationError();
    }
  }

  inline void unregisterEvent() {
    if (!event) {
      throw BreakpointEventNotRegisteredError();
    }
    DBG() << "BreakpointRegistry::unregisterEvent()" << std::endl;
    vmi_clear_event(vmi, event, doAfterClearEvent);
    event = nullptr;  // Mark free-in-progress
  }

  ~BreakpointRegistry() {
    if (event) unregisterEvent();
  }

  /**
   * @brief Set a breakpoint at kernel address `addr`. Also register a callback
   * `onHit` when the breakpoint is hit.
   * 
   * Note that this method will not enable the created breakpoint.
   * 
   * @param addr 
   * @param onHit 
   * @return std::shared_ptr<Breakpoint> 
   */
  inline std::shared_ptr<Breakpoint> setBreakpoint(
    addr_t addr,
    std::function<void(vmi_event_t*)> onHit
  ) {
    auto emplaceResult = bps.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(addr),
      std::forward_as_tuple(std::make_shared<Breakpoint>(vmi, addr, onHit))
    );
    if (!emplaceResult.second) {
      // Not inserted
      throw BreakpointAlreadySetError();
    }
    return emplaceResult.first->second;
  }

  /**
   * @brief Unset a breakpoint at kernel address `addr`.
   * 
   * @param addr 
   * @return std::shared_ptr<Breakpoint> the unset breakpoint.
   */
  inline std::shared_ptr<Breakpoint> unsetBreakpoint(addr_t addr) {
    auto it = bps.find(addr);
    if (it != bps.end()) {
      auto bp = it->second;
      bps.erase(it);
      return bp;
    } else {
      return nullptr;
    }
  }
};

uint32_t BreakpointRegistry::typeId = BreakpointRegistryTID;


} // namespace breakpoint
}


#endif /* B67F90ED_CE43_4E58_9FA5_7A4CD43B0323 */
