/**
 * @file BreakpointRegistry.hh
 * @author yoUntitled (gnu.imm@outlook.com)
 * @brief Registry that creates and manages breakpoints.
 * @version 0.1
 * @date 2021-12-23
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifndef E6B175ED_865B_4D28_B152_8B42185A2B2D
#define E6B175ED_865B_4D28_B152_8B42185A2B2D

#include <libvmi/libvmi.h>
#include <libvmi/events.h>

#include <map>  // std::map
#include <cstdint>  // uintptr_t
#include <functional>  // std::function
#include <memory>  // std::shared_ptr
#include <vector>  // std::vector
#include <exception>

#include <guestutil/mem.hh>
#include <guestutil/event/data.hh>
#include <debug.hh>
#include <pretty-print.hh>


namespace guestutil {
namespace breakpoint {


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

class DisableAllError: public BreakpointRegistryError {
private:
  std::vector<memory::MemoryWriteError> errors;
public:
  DisableAllError(std::vector<memory::MemoryWriteError> errs): errors(errs) {};

  inline std::vector<memory::MemoryWriteError> getErrors() {
    return errors;
  }

  virtual const char *what() const throw() {
    return "Some/all breakpoints cannot be disabled";
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
    auto bp = bpIter->second;
    if (!(bp->isEnabled())) {
      /*
      Do not deliver this event to the breakpoint if it's disabled.
      By doing this filtering, the correctness relies on the assumption
      "once the breakpoint is enabled and the event loop is bumping, the
      breakpoint is only disabled when both the VM and the event loop are
      paused and all the events are drained".
      */
      intEvent.reinject = 1;
      return VMI_EVENT_RESPONSE_NONE;
    }
    // Otherwise, this event is triggered by our breakpoint
    intEvent.reinject = 0;
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
   * Note the VMI must be initialized with `VMI_INIT_EVENTS` flag set.
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

  /**
   * @brief Unregister the INT3 event w/o disabling breakpoints first.
   * 
   * Hint: use `disableAll` to disable all breakpoints.
   * 
   */
  inline void unregisterEvent() {
    if (!event) {
      throw BreakpointEventNotRegisteredError();
    }
    DBG() << "BreakpointRegistry::unregisterEvent()" << std::endl;
    vmi_clear_event(vmi, event, doAfterClearEvent);
    event = nullptr;  // Mark free-in-progress
  }

  /**
   * @brief Disable all registered breakpoints.
   * 
   */
  inline void disableAll() {
    DBG() << "Breakpoint::disableAll()" << std::endl;
    std::vector<memory::MemoryWriteError> errors;
    for (auto it : bps) {
      try {
        it.second->disable();
      } catch (memory::MemoryWriteError &err) {
        errors.push_back(err);
      }
    }
    if (errors.size()) {
      throw DisableAllError(errors);
    }
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
   * @brief Unset a breakpoint at kernel address `addr` and disable it.
   * 
   * Note that you must pause the event loop before making this change.
   * 
   * @param addr 
   * @return std::shared_ptr<Breakpoint> the unset breakpoint.
   */
  inline std::shared_ptr<Breakpoint> unsetBreakpoint(addr_t addr) {
    auto it = bps.find(addr);
    if (it != bps.end()) {
      auto bp = it->second;
      bp->disable();
      bps.erase(it);
      return bp;
    } else {
      return nullptr;
    }
  }

  /**
   * @brief Get the underlying map of registered breakpoints.
   * 
   * @return std::map<addr_t, std::shared_ptr<Breakpoint>>& 
   */
  inline std::map<addr_t, std::shared_ptr<Breakpoint>> &getBps() {
    return bps;
  }
};

uint32_t BreakpointRegistry::typeId = BreakpointRegistryTID;


}
}


#endif /* E6B175ED_865B_4D28_B152_8B42185A2B2D */
