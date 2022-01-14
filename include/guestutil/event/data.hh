#ifndef AF8C4855_C9D6_43C1_A830_6119D9A74353
#define AF8C4855_C9D6_43C1_A830_6119D9A74353


#include <libvmi/libvmi.h>
#include <libvmi/events.h>
#include <guestutil/event/error.hh>


namespace guestutil {
namespace event {


class EventDataError: public EventError {
public:
  virtual const char *what() const throw() {
    return "Invalid event data";
  }
};

class NullEventDataError: public EventError {
public:
  virtual const char *what() const throw() {
    return "Null event data";
  }
};

class UnexpectedEventDataSentinelError: public EventError {
public:
  virtual const char *what() const throw() {
    return "Unexpected event data sentinel";
  }
};


template <typename Payload>
class EventData {
private:
  uint32_t sentinel;
  Payload &payload;
public:
  EventData(uint32_t _sentinel, Payload &_payload):
    sentinel(_sentinel), payload(_payload) {
    DBG() << "EventData(): " << this << std::endl;
  };

  ~EventData() {
    DBG() << "~EventData(): " << this << std::endl;
  }

  inline uint32_t getSentinel() {
    return sentinel;
  }

  inline Payload &getPayload() {
    return payload;
  }

  /**
   * @brief Get the `EventData` object from `vmi_event_t` object.
   * 
   * @param sentinel 
   * @param event 
   * @return EventData<Payload>& 
   */
  inline static EventData<Payload> &fromEvent(uint32_t sentinel, vmi_event_t *event) {
    EventData<Payload> *data = reinterpret_cast<EventData*>(event->data);
    if (!data) {
      throw NullEventDataError();
    }
    if (data->getSentinel() != sentinel) {
      throw UnexpectedEventDataSentinelError();
    }
    return *data;
  }

  /**
   * @brief Get the `Payload` object from `vmi_event_t` object.
   * 
   * @param sentinel The unique identifier for the payload object (per-object
   * ID) or the payload type (per-type ID), depending on the caller's design.
   * @param event The event object from which to extract the payload.
   * @return Payload& 
   */
  inline static Payload &getPayloadFromEvent(uint32_t sentinel, vmi_event_t *event) {
    return fromEvent(sentinel, event).getPayload();
  }
};


}
}

#endif /* AF8C4855_C9D6_43C1_A830_6119D9A74353 */
