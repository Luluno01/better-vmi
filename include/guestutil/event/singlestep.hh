/**
 * @file singlestep.hh
 * @author Untitled (gnu.imm@outlook.com)
 * @brief Singlestep event.
 * @version 0.1
 * @date 2021-12-23
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifndef DDA083F2_DCBE_47F6_B7AB_A42405E6D9A3
#define DDA083F2_DCBE_47F6_B7AB_A42405E6D9A3


#include <libvmi/libvmi.h>
#include <libvmi/events.h>

#include <exception>

#include <guestutil/event/error.hh>


namespace guestutil {
namespace event {
namespace singlestep {


class SinglestepError: public std::exception {};

/**
 * @brief Error of failing to register a singlestep event.
 * 
 * Possible reasons:
 * 
 * 1. Not initializing the VMI with `VMI_INIT_EVENTS`.
 * 2. Trying to register two singlestep events on the same vCPU.
 * 
 */
class RegistrationError:
  public SinglestepError, public event::RegistrationError {
public:
  virtual const char *what() const throw() {
    return "Failed to register the singlestep event "
      "(did you forget to initialize the VMI with VMI_INIT_EVENTS, "
      "or are you trying to register two singlestep events on the same vCPU?)";
  }
};

/**
 * @brief Register a catch-all singelstep event on all vCPUs.
 * 
 * @param vmi the VMI instance.
 * @param event the singlestep event object, managed by the caller.
 * @param onSinglestep the raw singlestep event callback function.
 */
inline void registerCatchAllSinglestepEvent(
  vmi_instance_t vmi,
  vmi_event_t &event,
  event_callback_t onSinglestep
) {
  SETUP_SINGLESTEP_EVENT(&event, 0, onSinglestep, false);
  unsigned int nVCPUs = vmi_get_num_vcpus(vmi);
  for (unsigned int vCPU = 0; vCPU < nVCPUs; vCPU++) {
    SET_VCPU_SINGLESTEP(event.ss_event, vCPU);
  }
  if (vmi_register_event(vmi, &event) == VMI_FAILURE) {
    vmi_clear_event(vmi, &event, nullptr);
    throw RegistrationError();
  }
}


}
}
}


#endif /* DDA083F2_DCBE_47F6_B7AB_A42405E6D9A3 */
