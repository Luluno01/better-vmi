#ifndef DA206AA8_8228_4B2F_A2D7_F27134B84EE6
#define DA206AA8_8228_4B2F_A2D7_F27134B84EE6


#include <iostream>
#include <map>  // std::map
#include <functional>  // std::function
#include <vector>  // std::vector
#include <memory>  // std::shared_ptr
#include <exception>
#include <string>
#include <algorithm>  // std::find

#include <debug.hh>


template <typename... ArgTypes>
class EventCallback {
protected:
  /**
   * @brief If this callback should be removed once called.
   * 
   */
  bool once;
public:
  EventCallback(bool _once): once(_once) {};

  /**
   * @brief Get if this callback should be removed once called.
   * 
   * @return true 
   * @return false 
   */
  inline bool isOnce() {
    return once;
  }

  /**
   * @brief Set if this callback should be removed once called.
   * 
   * @param val 
   */
  inline void setOnce(bool val) {
    once = val;
  }

  /**
   * @brief The actual callback.
   * 
   * @param args 
   */
  virtual void operator()(ArgTypes... args) = 0;
  /**
   * @brief Describe this callback (for debugging).
   * 
   * @return std::string 
   */
  virtual std::string toString() = 0;

  virtual ~EventCallback() {};

  inline friend std::ostream
  &operator<<(std::ostream &os, const EventCallback<ArgTypes...> &self) {
    os << self.toString();
    return os;
  }
};

/**
 * @brief Shortcut for creating an event callback.
 * 
 */
template <typename... ArgTypes>
class LambdaEventCallback: public EventCallback<ArgTypes...> {
public:
  using RawCallback = std::function<void(ArgTypes...)>;
private:
  RawCallback callback;
  std::string desc;
public:
  LambdaEventCallback(bool once, RawCallback _callback, std::string _desc):
    EventCallback<ArgTypes...>(once), callback(_callback), desc(_desc) {};

  virtual void operator()(ArgTypes... arg) {
    callback(arg...);
  }

  virtual std::string toString() {
    return desc;
  }

  /**
   * @brief A shortcut for creating a shared pointer to `LambdaEventCallback`
   * from a lambda function.
   * 
   * @param once if this is a one-time callback.
   * @param callback 
   * @param desc 
   * @return std::shared_ptr<LambdaEventCallback<ArgTypes...>> 
   */
  inline static std::shared_ptr<LambdaEventCallback<ArgTypes...>>
  fromLambda(bool once, RawCallback callback, std::string desc) {
    return std::make_shared<LambdaEventCallback<ArgTypes...>>(
      once, callback, desc);
  }
};

template <typename KeyType, typename... ArgTypes>
class EventEmitter {
public:
  using Callback = EventCallback<ArgTypes...>;
  using LambdaCallback = LambdaEventCallback<ArgTypes...>;
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
   * @param args 
   * @return unsigned int the number of callback(s) invoked.
   */
  inline unsigned int emit(KeyType key, ArgTypes... args) {
    if (events.count(key) == 0) {
      // std::map::operator[] may insert a new entry
      return 0;
    }
    CallbackList callbacks = events[key];  // Make a copy
    for (CallbackPtr callback : callbacks) {
      try {
        (*callback)(args...);
      } catch (std::exception &err) {
        std::cerr
          << toString() << ": ignoring error: " << err.what() << std::endl
          << "  in callback " << callback->toString() << std::endl;
      }
      if (callback->isOnce()) {
        DBG() << toString()
          << ": removing once callback " << callback->toString() << std::endl;
        off(key, callback);
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
   * @return EventEmitter<KeyType, ArgTypes...>&
   */
  inline EventEmitter<KeyType, ArgTypes...>
  &on(KeyType key, CallbackPtr callback) {
    // std::map::operator[] creates an entry if does not exist
    events[key].push_back(callback);
    return *this;
  }

  /**
   * @brief Listen to an event specified by `key`.
   * 
   * Note that listeners added using this function can only be removed by
   * clearing all listener on the event specified by `key`. To get a shared
   * pointer to the created callback for later removal, use the overloaded
   * version.
   * 
   * @param key 
   * @param callback 
   * @param desc 
   * @return EventEmitter<KeyType, ArgTypes...>& 
   */
  inline EventEmitter<KeyType, ArgTypes...>
  &on(KeyType key, RawCallback callback, std::string desc) {
    return on(key, LambdaCallback::fromLambda(false, callback, desc));
  }

  /**
   * @brief Listen to an event specified by `key`, returning callback created
   * from the raw callback.
   * 
   * @tparam once if this is a once-time callback.
   * @param key 
   * @param callback 
   * @param desc 
   * @return CallbackPtr created callback for later removal.
   */
  template <bool once>
  inline CallbackPtr on(KeyType key, RawCallback callback, std::string desc) {
    auto wrapped = LambdaCallback::fromLambda(once, callback, desc);
    on(key, wrapped);
    return wrapped;
  }

  /**
   * @brief Listen to an event specified by `key` once, returning created
   * callback.
   * 
   * @param key 
   * @param callback 
   * @param desc 
   * @return CallbackPtr the wrapped callback used to remove the listener.
   */
  inline CallbackPtr once(
    KeyType key,
    RawCallback callback, std::string desc) {
    return on<true>(key, callback, desc);
  }

  /**
   * @brief Remove an event listener registered with `key`.
   * 
   * @param key 
   * @param callback the callback to remove, or nullptr to clear all the
   * listeners registered with `key`.
   * @return EventEmitter<KeyType, ArgTypes...>& 
   */
  inline EventEmitter<KeyType, ArgTypes...>
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
