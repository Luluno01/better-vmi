#ifndef F2F2EFCB_F099_48B3_BB79_54F23C669809
#define F2F2EFCB_F099_48B3_BB79_54F23C669809


#include <libvmi/libvmi.h>
#include <libvmi/events.h>

#include <functional>  // std::function
#include <vector>  // std::vector
#include <exception>

#include <guestutil/event/data.hh>
#include <guestutil/event/error.hh>
#include <EventEmitter.hh>
#include <debug.hh>
#include <pretty-print.hh>


namespace guestutil {
namespace event {
namespace memory {


class MemEventError: public std::exception {};

/**
 * @brief Error of failing to register the memory event.
 * 
 * Possible reasons of failure:
 * 
 * 1. Not initializing the VMI with `VMI_INIT_EVENT`.
 * 2. Another `MemEvent` already registered on the same GFN.
 * 
 */
class RegistrationError:
  public MemEventError, public event::RegistrationError {
public:
  virtual const char *what() const throw() {
    return "Failed to register the memory event "
      "(did you forget to initialize the VMI with VMI_INIT_EVENTS, "
      "or are you trying to register two memory events on the same frame?)";
  }
};

class UnregistrationError:
  public MemEventError, public event::UnregistrationError {
public:
  virtual const char *what() const throw() {
    return "Failed to unregister memory event";
  }
};

class MemEvent;
class MemEventRegistry;

/* CPUToEventMapping */

const char CPUToEventMappingName[] = "CPUToEventMappingName";
const uint32_t CPUToEventMappingTID = \
  reinterpret_cast<std::uintptr_t>(CPUToEventMappingName);

/**
 * @brief Mapping from vCPU number to `MemEvent` object that just turned on
 * singlestep on this vCPU. Implemented as a vector indexed by vCPU number.
 * 
 * `MemEvent` is owned by `MemEventRegistry`, here we use raw pointers instead
 * because the event data is also using raw reference to `MemEvent`.
 */
class CPUToEventMapping: public std::vector<MemEvent*> {
public:
  static uint32_t typeId;

  inline CPUToEventMapping(unsigned int nCPUs):
    std::vector<MemEvent*>(nCPUs) {};

  inline static CPUToEventMapping &fromEvent(vmi_event_t *event) {
    return event::EventData<CPUToEventMapping>::getPayloadFromEvent(
      CPUToEventMappingTID, event
    );
  }

  /**
   * @brief Set `event` as currently active memory event on `vCPU`.
   * 
   * @param vCPU 
   * @param event 
   */
  inline void setActive(unsigned int vCPU, MemEvent *event) {
    at(vCPU) = event;
  }

  inline void setActive(unsigned int vCPU, MemEvent &event) {
    setActive(vCPU, &event);
  }

  /**
   * @brief Set the memory event on the vCPU as done.
   * 
   * @param vCPU 
   */
  inline void setDone(unsigned int vCPU) {
    at(vCPU) = nullptr;
  }
};

uint32_t CPUToEventMapping::typeId = CPUToEventMappingTID;

/* MemEvent */

const char MemEventName[] = "MemEvent";
const uint32_t MemEventTID = \
  reinterpret_cast<std::uintptr_t>(MemEventName);

enum MemEventKey {
  /**
   * @brief Before the memory access.
   * 
   */
  BEFORE,
  /**
   * @brief After the memory access (actually a singlestep event).
   * 
   */
  AFTER,
  UNREGISTERED
};

/**
 * @brief Memory event for a single guest physical memory frame.
 * 
 * This is private class used by `MemEventRegistry`.
 * 
 * Note:
 * 
 * 1. Guest physical memory frames are indexed by GFN (guest frame number).
 * 2. `MemEvent` objects are managed by `MemEventRegistry` to maintain no more
 *    than 1 `MemEvent` object for each frame (limitation of LibVMI).
 * 3. Users should get `MemEvent` object via
 *    `MemEventRegistry::forVirtualAddress`, which creates and register the
 *    LibVMI memory event.
 * 4. Current implementation is incompatible with other functions that use
 *    singlestep event.
 * 
 * Usage: see `MemEventRegistry`.
 * 
 */
class MemEvent: public EventEmitter<MemEventKey, vmi_event_t*> {
public:
  static uint32_t typeId;

  inline static MemEvent &fromEvent(vmi_event_t *event) {
    return event::EventData<MemEvent>::getPayloadFromEvent(typeId, event);
  }
private:
  /**
   * @brief The vmi instance we are operating on.
   * 
   */
  vmi_instance_t vmi;
  /**
   * @brief The event object for this GFN.
   * 
   */
  vmi_event_t memEvent;
  /**
   * @brief The event data to be attached to `memEvent.data`.
   * 
   */
  event::EventData<MemEvent> memEventData;

  /**
   * @brief The SLAT with relaxed permission. The value is inherited from
   * `MemEventRegistry`.
   * 
   */
  uint16_t okaySlat;
  /**
   * @brief The SLAT with restricted permission. The value is inherited from
   * `MemEventRegistry`.
   * 
   */
  uint16_t trapSlat;
  /**
   * @brief Reference to the same member of `MemEventRegistry`.
   * 
   */
  CPUToEventMapping &perCPUActiveEvents;
  /**
   * @brief If the event is registered to LibVMI.
   * 
   */
  bool registered;
  /**
   * @brief If we are going to unregister the memory event (therefore we should
   * relax the memory access permission).
   * 
   */
  bool pendingUnregister;

  /**
   * @brief The actual memory event handler, which does the following:
   * 
   * 1. Invoke the callback.
   * 2. Turn on singlestep.
   * 3. Switch the SLAT of the particular CPU to the okay SLAT.
   * 
   * This handles the first part of the memory event to allow the execution to
   * continue.
   * 
   * Registered via `MemEvent::register` by `MemEventRegistry` for each
   * `MemEvent` object.
   * 
   * @param vmi 
   * @param event 
   * @return event_response_t 
   */
  static event_response_t onMemoryAccess(
    vmi_instance_t vmi,
    vmi_event_t *event
  ) {
    (void) vmi;  // Unused

    MemEvent &memEvent = fromEvent(event);

    // Invoke the callback
    memEvent.emit(MemEventKey::BEFORE, vmi, event);

    // Mark active
    memEvent.perCPUActiveEvents.setActive(event->vcpu_id, memEvent);

    // Switch to okay SLAT (switch back later in onSinglestep)
    event->slat_id = memEvent.okaySlat;

    // Tell LibVMI we want to switch SLAT and turn on singlestep
    // FIXME: this assumes singlestep is not turned on at the moment
    return VMI_EVENT_RESPONSE_NONE |
      VMI_EVENT_RESPONSE_SLAT_ID |
      VMI_EVENT_RESPONSE_TOGGLE_SINGLESTEP;
  }

  /**
   * @brief The actual singlestep event handler, which does the following:
   * 
   * 1. Invoke the callback.
   * 2. Turn off singlestep.
   * 3. Switch the SLAT of the particular CPU back to the trap SLAT.
   * 
   * This handles the second part of the memory event.
   * 
   * Registered directly by `MemEventRegistry` once during its lifetime.
   * 
   * @param vmi 
   * @param event 
   * @return event_response_t 
   */
  static event_response_t onSinglestep(
    vmi_instance_t vmi,
    vmi_event_t *event
  ) {
    (void) vmi;  // Unused

    CPUToEventMapping &perCPUEvents = CPUToEventMapping::fromEvent(event);
    uint32_t cpu = event->vcpu_id;

    MemEvent *memEvent = perCPUEvents.at(cpu);

    // Invoke the callback
    memEvent->emit(MemEventKey::AFTER, vmi, event);

    // Mark done
    perCPUEvents.setDone(cpu);

    if (memEvent->pendingUnregister) {
      memEvent->unregister();
      // Tell LibVMI we want to resume the execution normally
      // FIXME: this assume no one else wants to keep singlestep on
      return VMI_EVENT_RESPONSE_NONE | VMI_EVENT_RESPONSE_TOGGLE_SINGLESTEP;
    }

    // Switch back to trap SLAT
    event->slat_id = memEvent->trapSlat;

    // Tell LibVMI we want to switch SLAT and turn off singlestep
    // FIXME: this assumes no one else wants to keep singlestep on
    return VMI_EVENT_RESPONSE_NONE |
      VMI_EVENT_RESPONSE_SLAT_ID |
      VMI_EVENT_RESPONSE_TOGGLE_SINGLESTEP;
  }

  /**
   * @brief The `vmi_event_free_t` stuff.
   * 
   * @param event 
   * @param rc 
   */
  static void onEventCleared(vmi_event_t *event, status_t rc) {
    MemEvent & memEvent = fromEvent(event);
    if (rc == VMI_FAILURE) {
      std::cerr
        << "Warning: vmi_clear_event failed to clear MemEvent"
        << std::endl;
    }
    // `MemEvent` does not free itself, because it is owned by
    // `MemEventRegistry`
    if (!memEvent.emit(MemEventKey::UNREGISTERED, memEvent.vmi, event)) {
      std::cerr <<
        "Warning: no one is listening to UNREGISTERED event on MemEvent, "
        "MemEventRegistry should do so and free the object" << std::endl;
    }
  }

  /**
   * @brief Register the event.
   * 
   * Possible reasons of failure:
   * 
   * 1. Not initializing the VMI with `VMI_INIT_EVENT`.
   * 2. Another `MemEvent` already registered on the same GFN.
   * 
   */
  inline void regsiter() {
    DBG() << "MemEvent::register()" << std::endl
          << pp::Event<VMI_EVENT_MEMORY, 2>(memEvent) << std::endl;
    // Register the memory event itself
    if (vmi_register_event(vmi, &memEvent) == VMI_FAILURE) {
      throw RegistrationError();
    }
    registered = true;
  }

  /**
   * @brief Unregister the event.
   * 
   */
  inline void unregister() {
    DBG() << "MemEvent::unregister()" << std::endl;
    registered = false;
    // Make sure LibVMI is operating on the correct SLAT
    // Not sure if this is how it works
    memEvent.slat_id = trapSlat;
    if (vmi_clear_event(vmi, &memEvent, onEventCleared) == VMI_FAILURE) {
      throw UnregistrationError();
    }
  }

  /**
   * @brief Try unregister the event.
   * 
   * @return status_t the status of this operation.
   */
  inline status_t tryUnregister() {
    if (!registered) return VMI_FAILURE;
    status_t res = vmi_clear_event(vmi, &memEvent, onEventCleared);
    DBG() << "MemEvent::tryUnregister(): " <<
      (res == VMI_FAILURE ? "failed" : "successful") << std::endl;
    return res;
  }

public:
  /**
   * @brief Construct a new `MemEvent` object, but don't register it yet.
   * 
   * This is intended to be called by `MemEventRegistry` with `make_shared`.
   * 
   * @param vmi the vmi object from LibVMI.
   * @param _okaySlat the ID of the SLAT which relaxes the permission.
   * @param _trapSlat the ID of the SLAT on which the event should be
   * registered (the permission will be changed when registered).
   * @param _perCPUActiveEvents the mapping from `MemEventRegistry`.
   * @param gfn the guest frame number of memory this event is in charge of.
   * @param access the accesses **to intercept** when setting up.
   */
  MemEvent(
    vmi_instance_t _vmi,
    uint16_t _okaySlat, uint16_t _trapSlat,
    CPUToEventMapping &_perCPUActiveEvents,
    addr_t gfn,
    vmi_mem_access_t access
  ): vmi(_vmi), memEvent{}, memEventData{typeId, *this},
    okaySlat(_okaySlat), trapSlat(_trapSlat),
    perCPUActiveEvents(_perCPUActiveEvents),
    registered(false), pendingUnregister(false) {
    SETUP_MEM_EVENT(
      &memEvent,
      gfn, access,
      onMemoryAccess,
      false
    );
    memEvent.slat_id = _trapSlat;
    memEvent.data = &memEventData;
  }

  ~MemEvent() {
    tryUnregister();
  }

  /**
   * @brief Check if this event is registered or not.
   * 
   * @return true registered.
   * @return false not yet registered, or unregistered.
   */
  inline bool isRegistered() {
    return registered;
  }

  /**
   * @brief Get the GFN on which this `MemEvent` is registered.
   * 
   * @return addr_t 
   */
  inline addr_t getGFN() {
    return memEvent.mem_event.gfn;
  }

  /**
   * @brief Schedule unregistration asynchronously, emit an `UNREGISTERED`
   * event when done.
   * 
   */
  inline void scheduleUnregister() {
    pendingUnregister = true;
  }

  virtual std::string toString() {
    return "MemEvent";
  }

  friend class MemEventRegistry;
};

uint32_t MemEvent::typeId = MemEventTID;


}
}
}

#endif /* F2F2EFCB_F099_48B3_BB79_54F23C669809 */
