#ifndef FDAB1A86_4A8F_4B6B_8CA6_58447A7FC6DD
#define FDAB1A86_4A8F_4B6B_8CA6_58447A7FC6DD


#include <libvmi/libvmi.h>
#include <libvmi/events.h>
#include <libvmi/slat.h>
extern "C" {
#include <xenctrl.h>
// #include <xen/xen.h>
#include <xen/hvm/params.h>
}

#include <map>  // std::map
#include <memory>  // std::shared_ptr
#include <exception>

#include <guestutil/mem.hh>
#include <guestutil/event/MemEvent.hh>
#include <guestutil/event/singlestep.hh>
#include <EventEmitter.hh>
#include <debug.hh>


namespace guestutil {
namespace event {
namespace memory {


enum Operation {
  REINIT_CHECK,
  XC_INTERFACE_OPEN,
  XC_HVM_PARAM_GET_ALTP2M,
  XC_HVM_PARAM_SET_ALTP2M,
  VMI_SLAT_SET_DOMAIN_STATE,
  VMI_SLAT_CREATE,
  VMI_SLAT_SWITCH,
  VMI_SLAT_DESTROY,
  XC_INTERFACE_CLOSE,
  VMI_REGISTER_EVENT
};

class RegistryError: public std::exception {
private:
  const char *_what;
public:
  RegistryError(const char *what): _what(what) {};
  virtual const char *what() const throw() {
    return _what;
  }
};

class RegistryInitError: public RegistryError {
public:
  /**
   * @brief Operation that failed.
   * 
   */
  Operation op;

  /**
   * @brief Construct a new Registry Init Error object.
   * 
   * @param what the error message.
   * @param _op the failed operation.
   */
  RegistryInitError(Operation _op, const char *what):
    RegistryError(what), op(_op) {};
};

class FrameAlreadyRegisteredError: public RegistryError {
public:
  FrameAlreadyRegisteredError():
    RegistryError("MemEvent already registered on this frame") {};
};


enum RegistryEvent {
  /**
   * @brief A `MemEvent` is unregistered.
   * 
   * Actual event argument: uint16_t gfn.
   * 
   * Normally users don't need to listen to this event as long as they have
   * drained the event queue and unregistered all the events. In that case we
   * can safely free all the objects.
   */
  MEM_EVENT_UNREGISTERED
};

/**
 * @brief Registry of `MemEvent`.
 * 
 * Usage:
 * 
 * ```C++
 * MemEventRegistry reg(vmi);
 * // ...
 * vm.pause();
 * reg.init();
 * addr_t gfn = memory::kvaToGFN(vmi, ...);
 * auto memEvent = reg.registerForGFN(gfn);
 * memEvent->on(MemEventKey::BEFORE, ...);
 * vm.resume();
 * ```
 * 
 */
class MemEventRegistry:
  public EventEmitter<RegistryEvent, vmi_instance_t, void*> {
private:
  using FrameToEventMapping = std::map<addr_t, std::shared_ptr<MemEvent>>;

  /**
   * @brief The callback called when the event is safe to be removed from the
   * registry.
   * 
   * If the event to be unregistered has no pending event in the LibVMI's
   * queue, this will be called right before `vmi_clear_event` returns.
   * Otherwise, this is delayed (by LibVMI) until all pending events are
   * handled.
   * 
   */
  class UnregisteredCallback:
    public EventCallback<vmi_instance_t, vmi_event_t*> {
  private:
    MemEventRegistry &reg;
    addr_t gfn;
  public:
    UnregisteredCallback(MemEventRegistry &_reg, addr_t _gfn):
      reg(_reg), gfn(_gfn) {};

    virtual void operator()(vmi_instance_t vmi, vmi_event_t *) {
      DBG() << "MemEventRegistry::UnregisteredCallback()" << std::endl
            << "  gfn: " << F_SHORT_UH64(gfn) << std::endl;
      if (!reg.perFrameEvents.erase(gfn)) {
        std::cerr << "Warning: MemEvent on GFN " << F_SHORT_UH64(gfn)
          << " was already removed unexpectedly from MemEventRegistry"
          << std::endl;
      }
      reg.emitUnregistered(vmi, gfn);
    }

    virtual std::string toString() {
      return "MemEventRegistry::UnregisteredCallback";
    }
  };

  vmi_instance_t vmi;

  /**
   * @brief The XENCtrl interface to enable altp2m.
   * 
   */
  xc_interface *xc;

  /**
   * @brief The catch-all singlestep event (the handler is
   * `MemEvent::onSinglestep`).
   * 
   */
  vmi_event_t ssEvent;
  /**
   * @brief Trap SLAT ID as the singlestep event data.
   * 
   */
  event::EventData<CPUToEventMapping> ssEventData;
  /**
   * @brief Mapping from vCPU number to `MemEvent` object that just turned on
   * singlestep on this vCPU. Implemented as a vector indexed by vCPU number.
   * 
   */
  CPUToEventMapping perCPUActiveEvents;

  /**
   * @brief Frame number => `MemEvent` mapping.
   * 
   * `MemEvent` objects are created on demand, and destroyed upon
   * unregistration.
   * 
   */
  FrameToEventMapping perFrameEvents;

  /**
   * @brief The SLAT with relaxed permission.
   * 
   * Set to 0 (default SLAT) in `init`, i.e., we assume no one else has is
   * using altp2m.
   */
  uint16_t okaySlat;
  /**
   * @brief The SLAT with restricted permission.
   * 
   * Created in `init`.
   */
  uint16_t trapSlat;

  /**
   * @brief Get the `MemEvent` object for memory frame indexed by `gfn` (Guest
   * Frame Number).
   * 
   * @param gfn 
   * @return std::shared_ptr<MemEvent> found `MemEvent` if any, null otherwise.
   */
  inline std::shared_ptr<MemEvent> forFrame(addr_t gfn) {
    auto it = perFrameEvents.find(gfn);
    if (it == perFrameEvents.end()) {
      return nullptr;
    } else {
      return it->second;
    }
  }

  inline unsigned int emitUnregistered(vmi_instance_t vmi, uint16_t gfn) {
    return emit(
      RegistryEvent::MEM_EVENT_UNREGISTERED,
      vmi, reinterpret_cast<void*>(gfn)
    );
  }
public:
  MemEventRegistry(vmi_instance_t _vmi):
    vmi(_vmi), xc(nullptr),
    ssEvent{}, ssEventData{CPUToEventMappingTID, perCPUActiveEvents},
    perCPUActiveEvents{vmi_get_num_vcpus(vmi)},
    okaySlat(0), trapSlat(0) {};

  ~MemEventRegistry() {
    DBG() << "~MemEventRegistry()" << std::endl;
    if (xc) {
      if (xc_interface_close(xc) < 0) {
        DBG() << "  xc_interface_close failed, ignoring" << std::endl;
      } else {
        DBG() << "  xc_interface closed" << std::endl;
      }
      xc = nullptr;
    }
    DBG() << "  Switch back to okay SLAT: ";
    if (vmi_slat_switch(vmi, okaySlat) == VMI_FAILURE) {
      DBG() << "failed, ignoring" << std::endl;
    } else {
      DBG() << "okay" << std::endl;
    }
    if (trapSlat != 0) {
      DBG() << "  Destroy SLAT " << F_D32(trapSlat) << ": ";
      if (vmi_slat_destroy(vmi, trapSlat) == VMI_FAILURE) {
        DBG() << "failed, ignoring" << std::endl;
      } else {
        DBG() << "okay" << std::endl;
      }
      trapSlat = 0;
    }
    DBG() << "  Clear singlestep event: ";
    if (vmi_clear_event(vmi, &ssEvent, nullptr) == VMI_FAILURE) {
      DBG() << "failed, possibly not registered, ignoring" << std::endl;
    } else {
      DBG() << "okay" << std::endl;
    }
    for (auto memEvent: perCPUActiveEvents) {
      if (memEvent != nullptr) {
        std::cerr << "Warning: MemEventRegistry is destroyed with "
          "active memory event on frame "
          << F_SHORT_HEX(memEvent->getGFN())
          << " waiting for a subsequent singlestep event" << std::endl;
      }
    }
    for (auto entry : perFrameEvents) {
      auto memEvent = entry.second;
      if (memEvent->isRegistered()) {
        std::cerr << "Warning: MemEventRegistry is destroyed with "
          " registered memory event on frame "
          << F_SHORT_HEX(memEvent->getGFN()) << ", "
          << "please unregister all memory events first before "
          "destorying MemEventRegistry" << std::endl;
      }
    }
  }

private:
  /**
   * @brief Check if we have already initialized.
   * 
   */
  inline void initCheck() {
    if (xc) throw RegistryInitError(
      REINIT_CHECK,
      "Attempt to reinitialize MemEventRegistry"
    );
  }

  /**
   * @brief Initialize XenCtrl.
   * 
   */
  inline void initXenCtrl() {
    if (!(xc = xc_interface_open(nullptr, nullptr, 0))) {
      throw RegistryInitError(
        XC_INTERFACE_OPEN,
        "Failed to open xc_interface, "
        "did you added altp2m=true to the Xen boot command line options?"
      );
    }
  }

  /**
   * @brief Register a catch-all singlestep event.
   * 
   */
  inline void initSinglestep() {
    ssEvent.data = &ssEventData;
    singlestep::registerCatchAllSinglestepEvent(
      vmi,
      ssEvent,
      MemEvent::onSinglestep
    );
  }

  /**
   * @brief Enable altp2m.
   * 
   */
  inline void initAltp2m() {
    // Check if we can enable altp2m
    DBG() << "MemEventRegistry::initAltp2m()" << std::endl;
    uint64_t altp2mParam;
    int hvmParamGetResult = xc_hvm_param_get(
      xc,
      vmi_get_vmid(vmi),
      HVM_PARAM_ALTP2M,
      &altp2mParam
    );
    if (hvmParamGetResult < 0) {
      throw RegistryInitError(
        XC_HVM_PARAM_GET_ALTP2M,
        "Failed to get altp2m param"
      );
    }
    // Set altp2m param if necessary
    switch (altp2mParam) {
      case XEN_ALTP2M_limited: {
        // Have to reboot because somehow the altp2m can be set only once
        throw RegistryInitError(
          XC_HVM_PARAM_SET_ALTP2M,
          "Altp2m was set to XEN_ALTP2M_limited, please reboot the guest"
        );
        break;
      }
      case XEN_ALTP2M_disabled: {
        if (xc_hvm_param_set(
          xc,
          vmi_get_vmid(vmi),
          HVM_PARAM_ALTP2M, XEN_ALTP2M_external
        ) < 0) {
          throw RegistryInitError(
            XC_HVM_PARAM_SET_ALTP2M,
            "Failed to set altp2m param, "
            "did you added altp2m=true to the Xen boot command line options?"
          );
        }
        break;
      }
      default: break;  // No need to change
    }
    // Set altp2m domain state
    if (vmi_slat_set_domain_state(vmi, true) == VMI_FAILURE) {
      throw RegistryInitError(
        VMI_SLAT_SET_DOMAIN_STATE,
        "Failed to set altp2m domain state, please try rebooting the guest"
      );
    }
  }

  /**
   * @brief Create trap SLAT and switch to it.
   * 
   */
  inline void initSlat() {
    // Create a new SLAT for trapping
    // Initially, the new SLAT has the same permission
    // with the default one I suppose?
    if (vmi_slat_create(vmi, &trapSlat) == VMI_FAILURE) {
      throw RegistryInitError(
        VMI_SLAT_CREATE,
        "Failed to create a new SLAT (altp2m view)"
      );
    }
    okaySlat = 0;
    DBG() << "MemEventRegistry::init()" << std::endl
          << "  trapSlat: " << F_D32(trapSlat) << std::endl;
    // Switch to the trap SLAT
    if (vmi_slat_switch(vmi, trapSlat) == VMI_FAILURE) {
      throw RegistryInitError(
        VMI_SLAT_SWITCH,
        "Failed to switch to the trap SLAT"
      );
    }
  }

public:
  /**
   * @brief Initialize the environment for memory event.
   * 
   * Note the following:
   * 
   * 1. The altp2m feature of Xen Project must be enabled at boot time by
   *    adding `altp2m=true` to the boot command line options.
   * 2. Somehow, HVM_PARAM_ALTP2M can be set once during the lifetime of a
   *    guest domain. You have to reboot the guest if you want to change it a
   *    second time.
   * 
   */
  inline void init() {
    // Sanity check
    initCheck();
    // Open XC interface
    initXenCtrl();
    // Register a catch-all singlestep event
    initSinglestep();
    // Enable altp2m
    initAltp2m();
    // Create and switch to a trap SLAT
    initSlat();
  }

  /**
   * @brief Register memory event on given frame indexed by `gfn`.
   * 
   * Important notes:
   * 
   * 1. Intercepts R/W accesses only.
   * 2. Does not handle context switch or change of page table.
   * 
   * @param gfn guest frame number.
   * @return std::shared_ptr<MemEvent> 
   */
  inline std::shared_ptr<MemEvent> registerForGFN(addr_t gfn) {
    auto emplaceResult = perFrameEvents.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(gfn),
      std::forward_as_tuple(std::make_shared<MemEvent>(
        vmi,
        okaySlat, trapSlat,
        perCPUActiveEvents,
        gfn,
        VMI_MEMACCESS_RW
      ))
    );
    if (!emplaceResult.second /* Inserted? */) {
      throw FrameAlreadyRegisteredError();
    }

    // This will register a memory event on the GFN
    // by removing R/W permissions from the trap SLAT
    // (takes effect immediately)
    std::shared_ptr<MemEvent> memEvent = emplaceResult.first->second;
    memEvent->regsiter();

    // The catch-all singlestep event is already registered in `init`
  
    return memEvent;
  }

  /**
   * @brief Unregister the memory event on given `gfn`.
   * 
   * This function does the following:
   * 
   * 1. Tell the corresponding `MemEvent` to unregister itself asynchronously.
   * 2. Register a callback to the `MemEvent`, such that when it is
   *    unregistered, we can remove the `MemEvent` from this registry.
   * 
   * @param gfn 
   * @return true event unregistering
   * @return false no memory event was registered on this frame.
   */
  inline bool unregisterForGFN(addr_t gfn) {
    auto it = perFrameEvents.find(gfn);
    if (it == perFrameEvents.end()) return false;
    auto event = it->second;
    event->on(
      MemEventKey::UNREGISTERED,
      std::make_shared<UnregisteredCallback>(*this, gfn)
    );
    event->scheduleUnregister();
    return true;
  }

  virtual std::string toString() {
    return "MemEventRegistry";
  }
};


}
}
}

#endif /* FDAB1A86_4A8F_4B6B_8CA6_58447A7FC6DD */
