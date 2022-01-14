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
// #include <guestutil/breakpoint/Instruction.hh>
#include <functional>  // std::function
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
    // readInstruction<CS_ARCH_X86, CS_MODE_64>(vmi, addr, 0, emul.data);
    memory::readKVA(vmi, addr, 15, emul.data);
    memory::write8KVA(vmi, addr, breakpointInstruction);
    enabled = true;
  }

  /**
   * @brief Disable this breakpoint by removing the injected breakpoint
   * instruction if any. Do nothing if the injection has not been done.
   * 
   * Note that there could still be pending events caused by this breakpoint.
   * They must be handled or the default behavior is reinjecting the interrupt,
   * which crashes the guest with "interrupt error". Alternatively, we can
   * pause the guest, drain all the events (by checking pending events using
   * `vmi_are_events_pending`), and then disable the breakpoint. That means you
   * have to call `event::Loop::schedulePause` and disable breakpoints in the
   * callback.
   * 
   */
  inline void disable() {
    if (enabled) {
      DBG() << "Breakpoint.disable()" << std::endl
            << "  addr        : " << F_PTR(addr) << std::endl
            << "  emul.data[0]: " << F_UH32(emul.data[0]) << std::endl;
      memory::write8KVA(vmi, addr, emul.data[0]);
      enabled = false;
    }
  }

  ~Breakpoint() {
    disable();
  }

  friend class BreakpointRegistry;
};


} // namespace breakpoint
}


#endif /* B67F90ED_CE43_4E58_9FA5_7A4CD43B0323 */
