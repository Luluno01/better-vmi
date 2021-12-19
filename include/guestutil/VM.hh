#ifndef C1538E16_FB8C_4457_9C87_EB4A560C1CB3
#define C1538E16_FB8C_4457_9C87_EB4A560C1CB3

#include <libvmi/libvmi.h>
#include <exception>
#include <debug.hh>


namespace guestutil {
namespace vm {


class VMError: public std::exception {
public:
  virtual const char *what() const throw() {
    return "VM error";
  }
};

class VMIInitError: public VMError {
public:
  vmi_init_error_t error;
  VMIInitError(vmi_init_error_t err): error(err) {};
  virtual const char *what() const throw() {
    return "Failed to init LibVMI";
  }
};

class PauseError: public VMError {
public:
  virtual const char *what() const throw() {
    return "Failed to pause VM";
  }
};

class ResumeError: public VMError {
public:
  virtual const char *what() const throw() {
    return "Failed to resume VM";
  }
};

class VM {
protected:
  vmi_instance_t vmi;
  /**
   * @brief Init data, managed by us
   * 
   */
  vmi_init_data_t *initData;  // Unused for now
  /**
   * @brief The actual init flags used during initialization.
   * 
   */
  uint64_t initFlags;
public:
  VM() {
    DBG() << "VM()";
    vmi = {0};
    initData = nullptr;
    initFlags = 0;
  }

  VM(const char *name, uint64_t initFlags = 0) {
    DBG() << "VM(const char *name, uint64_t initFlags)" << std::endl;
    vmi = {0};
    initData = nullptr;
    init(
      name, VMI_INIT_DOMAINNAME | initFlags, initData,
      VMI_CONFIG_GLOBAL_FILE_ENTRY, nullptr
    );
  }

  inline void init(
    const void *domain, uint64_t initFlags, vmi_init_data_t *initData,
    vmi_config_t configMode, void *config
  ) {
    vmi_init_error_t err = VMI_INIT_ERROR_NONE;
    this->initFlags = initFlags;
    if (vmi_init_complete(
      &vmi,
      domain, initFlags,
      initData,
      configMode,
      config,
      &err
    ) == VMI_FAILURE) {
      vmi = {0};
      this->initFlags = 0;
      throw VMIInitError(err);
    }
  }

  inline vmi_instance_t &getVMI() {
    return vmi;
  }

  inline uint64_t getInitFlags() {
    return initFlags;
  }

  inline bool isEventEnabled() {
    return getInitFlags() & VMI_INIT_EVENTS;
  }

  inline void pause() {
    if (vmi_pause_vm(vmi) == VMI_FAILURE) {
      throw PauseError();
    }
  }

  inline void resume() {
    if (vmi_resume_vm(vmi) == VMI_FAILURE) {
      throw ResumeError();
    }
  }

  inline status_t tryResume() {
    return vmi_resume_vm(vmi);
  }

  inline uint64_t id() {
    return vmi_get_vmid(vmi);
  }

  ~VM() {
    DBG() << "~VM()" << std::endl;
    vmi_resume_vm(vmi);
    vmi_destroy(vmi);
    if (initData) {
      delete initData;
      initData = nullptr;
    }
  }
};


}
}

#endif /* C1538E16_FB8C_4457_9C87_EB4A560C1CB3 */
