#ifndef B30C05A6_FFB8_4F96_9201_435C161B0C27
#define B30C05A6_FFB8_4F96_9201_435C161B0C27


#include <exception>


namespace guestutil {
namespace event {


class EventError: public std::exception {};

class ListenError: public EventError {
public:
  virtual const char *what() const throw() {
    return "Failed to listen for events";
  }
};

class BumpError: public EventError {
public:
  virtual const char *what() const throw() {
    return "An error has occurred during or before bumping";
  }
};

class PauseError: public EventError {
public:
  virtual const char *what() const throw() {
    return "Failed to pause the event loop";
  }
};

class PauseCallbackError: public PauseError {
public:
  virtual const char *what() const throw() {
    return "An error has occurred in the onPause callback";
  }
};

class PausePendingError: public PauseError {
public:
  virtual const char *what() const throw() {
    return "A pause request is in flight";
  }
};

class GetPendingError: public EventError {
public:
  virtual const char *what() const throw() {
    return "Failed to get the number of pending events";
  }
};

class StoppingError: public EventError {
public:
  virtual const char *what() const throw() {
    return "The event loop is stopping";
  }
};

}
}

#endif /* B30C05A6_FFB8_4F96_9201_435C161B0C27 */
