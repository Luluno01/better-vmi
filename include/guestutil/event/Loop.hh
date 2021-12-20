#ifndef C9D5657B_0418_45B4_8F4E_317136EB143A
#define C9D5657B_0418_45B4_8F4E_317136EB143A


#include <functional>  // std::function

#include <libvmi/libvmi.h>
#include <libvmi/events.h>

#include <debug.hh>
#include <guestutil/VM.hh>
#include <guestutil/event/error.hh>


namespace guestutil {
namespace event {


/**
 * @brief The non-thread-safe version event loop for single VM.
 * 
 */
class Loop {
private:
  vm::VM &vm;

  /* Loop states below */
  /**
   * @brief Error occurred. Once set, this life of this loop is ending.
   * 
   */
  EventError *err;
  /**
   * @brief If someone requested pausing the loop, we will set his callback here.
   * 
   */
  std::function<void()> onPausedCallback;
  /**
   * @brief The guy who issued the stop request (not cause by error). Once set,
   * this life of this loop is ending.
   * 
   */
  const char *stopRequestedBy;

  inline void handleError() {
    // TODO: possible cleanup?
  }

  template <typename Error>
  [[ noreturn ]] inline void throwErr() {
    handleError();
    err = new Error();
    throw *err;
  }

  inline void bumpOnce() {
    if (err) {
      throw BumpError();
    }
    if (vmi_events_listen(vm.getVMI(), 500) == VMI_FAILURE) {
      throwErr<ListenError>();
    }
  }

  /**
   * @brief 1) pause the VM, 2) drain the event queue, and 3) invoke the
   * callback. This routine is used to pause the event loop before making
   * changes to all the event-related stuff because there could be some pending
   * events induced by us that have to be handled, which could be
   * indistinguishable from the ones induced by the guest itself if we don't
   * pause the event loop (and the guest VM) first.
   * 
   */
  inline void handlePause() {
    vm.pause();
    int nPending = vmi_are_events_pending(vm.getVMI());
    DBG() << "Loop::handlePause() - draining " << nPending << " event(s)"
          << std::endl;
    while (nPending > 0) {
      // Otherwise, we still have pending events
      if (vmi_events_listen(vm.getVMI(), 500) == VMI_FAILURE) {
        // Error
        throwErr<PauseError>();
      }
      // Update
      nPending = vmi_are_events_pending(vm.getVMI());
    }
    // Check why we broke out of the above loop
    if (nPending < 0) {
      // Error
      throwErr<GetPendingError>();
    }
    if (stopRequestedBy) {
      // Someone requested stop when we are trying to pause the event loop
      throwErr<StoppingError>();
    }
    DBG() << "Loop::handlePause() - calling onPauseCallback" << std::endl;
    try {
      onPausedCallback();
    } catch (std::exception &err) {
      std::cerr << "Error in onPause callback: " << err.what() << std::endl;
      throwErr<PauseCallbackError>();
    }
  }
public:
  Loop(vm::VM &_vm): vm(_vm),
    err(nullptr), onPausedCallback(), stopRequestedBy(nullptr) {};
  ~Loop() {
    if (err) {
      delete err;
    }
  }

  /**
   * @brief Bump the event loop until we have an error or received a stop
   * signal.
   * 
   * @return EventError* error occurred if any
   */
  inline EventError *bump() {
    DBG() << "Loop::bump()" << std::endl;
    while (!err && !stopRequestedBy) {
      if (onPausedCallback) {
        handlePause();
        onPausedCallback = nullptr;
      } else {
        bumpOnce();
      }
    }
    return err;
  }

  /**
   * @brief **Asynchronously** pause the event loop and execute the callback
   * within the loop, then resume the loop.
   * 
   * Calling this method will, asynchronously, 1) pause the VM, 2) drain the
   * event queue, and 3) invoke the callback.
   * 
   * @param callback 
   * @param who who is making this request? This is only used for debugging.
   */
  inline void schedulePause(std::function<void()> callback, const char *who) {
    if (onPausedCallback) {
      throw PausePendingError();
    }
    DBG() << "Loop::schedulePause(std::function<void()>, const char*)"
          << std::endl
          << "  Pause requested by " << who << std::endl;
    onPausedCallback = callback;
  }

  /**
   * @brief Send stop signal.
   * 
   * @param who who send the stop signal.
   */
  inline void stop(const char *who) {
    DBG() << "Loop::stop(const char *)" << std::endl
          << "  Stop requested by " << who << std::endl;
    stopRequestedBy = who;
  }

  /**
   * @brief Check if the event loop had an error.
   * 
   * @return true 
   * @return false 
   */
  inline bool hasError() const {
    return err != nullptr;
  }

  /**
   * @brief Get the last event loop error (also the only error, because we will
   * stop once we have an error).
   * 
   * @return EventError* 
   */
  inline EventError *getError() const {
    return err;
  }

  inline const char *getStopRequestedBy() const {
    return stopRequestedBy;
  }
};


}
}


#endif /* C9D5657B_0418_45B4_8F4E_317136EB143A */
