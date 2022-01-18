#ifndef D1D38F65_0176_43EB_A62B_C255623CCC08
#define D1D38F65_0176_43EB_A62B_C255623CCC08


#include <iostream>
#include <signal.h>
#include <EventEmitter.hh>
#include <debug.hh>


/**
 * @brief Signal source.
 * 
 */
class SignalSource: public EventEmitter<int, int> {
private:
  struct sigaction act;
  bool initialized;

  static void handleSignal(int signal) {
    auto &gThis = getSignalSource();
    gThis.emit(0, signal);  // Catch-all event
    gThis.emit(signal, signal);
  }

  SignalSource(): act{}, initialized(false) {
    DBG() << "SignalSource()" << std::endl;
  };

  SignalSource(SignalSource&) = delete;
  SignalSource(SignalSource&&) = delete;

public:
  static SignalSource &getSignalSource() {
    /**
     * @brief Global singleton.
     * 
     */
    static SignalSource gThis;
    return gThis;
  }

  inline SignalSource &init() {
    if (initialized) return *this;
    initialized = true;
    act.sa_handler = handleSignal;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    // Handle this many for now
    sigaction(SIGHUP, &act, nullptr);
    sigaction(SIGTERM, &act, nullptr);
    sigaction(SIGINT, &act, nullptr);
    sigaction(SIGALRM, &act, nullptr);
    return *this;
  }

  virtual ~SignalSource() {
    DBG() << "~SignalSource()" << std::endl;
  }

  virtual std::string toString() {
    return "SinglaSource";
  }
};


#endif /* D1D38F65_0176_43EB_A62B_C255623CCC08 */
