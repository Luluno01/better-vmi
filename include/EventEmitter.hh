#ifndef DA206AA8_8228_4B2F_A2D7_F27134B84EE6
#define DA206AA8_8228_4B2F_A2D7_F27134B84EE6


#include <libvmi/libvmi.h>

#include <iostream>
#include <map>  // std::map
#include <functional>  // std::function
#include <vector>  // std::vector
#include <memory>  // std::shared_ptr
#include <exception>
#include <string>
#include <algorithm>  // std::find

#include <debug.hh>


template <typename ArgType>
class EventCallback {
public:
  /**
   * @brief The actual callback.
   * 
   * @param vmi 
   * @param event 
   */
  virtual void operator()(vmi_instance_t vmi, ArgType event) = 0;
  /**
   * @brief Describe this callback (for debugging).
   * 
   * @return std::string 
   */
  virtual std::string toString() = 0;

  virtual ~EventCallback() {};
};

/**
 * @brief Shortcut for creating an event callback.
 * 
 */
template <typename ArgType>
class LambdaEventCallback: public EventCallback<ArgType> {
public:
  using RawCallback = std::function<void(vmi_instance_t, ArgType)>;
private:
  RawCallback callback;
  std::string desc;
public:
  LambdaEventCallback(RawCallback _callback): callback(_callback) {};

  virtual void operator()(vmi_instance_t vmi, ArgType arg) {
    callback(vmi, arg);
  }

  virtual std::string toString() {
    return desc;
  }

  /**
   * @brief A shortcut for creating a shared pointer to `LambdaEventCallback`
   * from a lambda function.
   * 
   * @param callback 
   * @return std::shared_ptr<LambdaEventCallback<ArgType>> 
   */
  inline static std::shared_ptr<LambdaEventCallback<ArgType>>
  fromLambda(RawCallback callback) {
    return std::make_shared<LambdaEventCallback<ArgType>>(callback);
  }
};

template <typename KeyType, typename ArgType>
class EventEmitter {
public:
  using Callback = EventCallback<ArgType>;
  using LambdaCallback = LambdaEventCallback<ArgType>;
  using RawCallback = typename LambdaCallback::RawCallback;
  using CallbackPtr = std::shared_ptr<Callback>;
  using CallbackList = std::vector<CallbackPtr>;
  using EventMapping = std::map<KeyType, CallbackList>;
protected:
  /**
   * @brief Mapping from event keys to callback lists.
   * 
   * Entries with empty callback list are always removed.
   * 
   */
  EventMapping events;

  /**
   * @brief Emit an event identified by `key`.
   * 
   * @param key 
   * @param vmi 
   * @param arg 
   * @return unsigned int the number of callback(s) invoked.
   */
  inline unsigned int emit(
    KeyType key,
    vmi_instance_t vmi, ArgType arg
  ) {
    if (events.count(key) == 0) {
      // std::map::operator[] may insert a new entry
      return 0;
    }
    CallbackList callbacks = events[key];  // Make a copy
    for (CallbackPtr callback : callbacks) {
      try {
        (*callback)(vmi, arg);
      } catch (std::exception &err) {
        std::cerr
          << toString() << ": ignoring error: " << err.what() << std::endl
          << "  in callback " << callback->toString() << std::endl;
      }
    }
    return callbacks.size();
  }
public:
  EventEmitter(): events() {
    DBG() << "EventEmitter()" << std::endl;
  };

  virtual ~EventEmitter() {
    DBG() << "~EventEmitter()" << std::endl;
  }

  /**
   * @brief Get the number of registered events.
   * 
   * @return unsigned int 
   */
  inline unsigned int getNumEvents() const {
    return events.size();
  }

  /**
   * @brief Check if we have any listener for given event `key`.
   * 
   * @param key the event key.
   * @return true at least one listener is listening to the event specified by
   * `key`.
   * @return false no listener registered for given event `key`.
   */
  inline bool hasListener(KeyType key) const {
    return events.count(key) > 0;
  }

  /**
   * @brief Get the number of listeners for given event `key`.
   * 
   * @param key the event key.
   * @return unsigned int 
   */
  inline unsigned int getNumListeners(KeyType key) {
    auto it = events.find(key);
    if (it == events.end()) return 0;
    CallbackList &list = it->second;
    return list.size();
  }

  /**
   * @brief Get a vector of registered event keys.
   * 
   * @return std::vector<KeyType> 
   */
  inline std::vector<KeyType> getRegisteredEvents() const {
    std::vector<KeyType> keys {};
    keys.reserve(events.size());
    for (auto entry : events) {
      keys.push_back(entry.first);
    }
    return keys;
  }

  /**
   * @brief Listen to events specified by `key`.
   * 
   * @param key 
   * @param callback 
   * @return EventEmitter<KeyType, ArgType>&
   */
  inline EventEmitter<KeyType, ArgType>
  &on(KeyType key, CallbackPtr callback) {
    // std::map::operator[] creates an entry if does not exist
    events[key].push_back(callback);
    return *this;
  }

  /**
   * @brief Listen to events specified by `key`.
   * 
   * @param key 
   * @param callback 
   * @return EventEmitter<KeyType, ArgType>& 
   */
  inline EventEmitter<KeyType, ArgType>
  &on(KeyType key, RawCallback callback) {
    return on(key, LambdaCallback::fromLambda(callback));
  }

  /**
   * @brief Remove an event listener registered with `key`.
   * 
   * @param key 
   * @param callback the callback to remove, or nullptr to clear all the
   * listeners registered with `key`.
   * @return EventEmitter<KeyType, ArgType>& 
   */
  inline EventEmitter<KeyType, ArgType>
  &off(KeyType key, CallbackPtr callback) {
    if (!events.count(key)) {
      return *this;
    }
    CallbackList &callbacks = events[key];  // Grab a reference
    if (callback == nullptr) {
      events.erase(key);
    } else {
      auto toRemove = std::find(callbacks.begin(), callbacks.end(), callback);
      if (toRemove != callbacks.end()) {
        // Callback was registered, remove it
        callbacks.erase(toRemove);
        if (!callbacks.size()) {
          // Remove callback list if it's empty
          events.erase(key);
        }
      }
      // Otherwise, callback was never registered, do nothing
    }
    return *this;
  }

  /**
   * @brief Describe this event emitter (for debugging).
   * 
   * @return std::string 
   */
  virtual std::string toString() = 0;
};


#endif /* DA206AA8_8228_4B2F_A2D7_F27134B84EE6 */
