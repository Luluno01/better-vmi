#ifndef D7364E74_200E_477B_B230_D3887D69117B
#define D7364E74_200E_477B_B230_D3887D69117B

#include <libvmi/libvmi.h>
#include <exception>


namespace guestutil {
namespace offset {


class GetOffsetError: std::exception {
public:
  const char *what() const throw() {
    return "Failed to get offset";
  }
};

/**
 * @brief Get the memory offset associated with the given offset_name.
 * 
 * @param vmi 
 * @param offsetName 
 * @return addr_t 
 */
inline addr_t getOffset(vmi_instance_t vmi, const char *offsetName) {
  addr_t addr;
  if (vmi_get_offset(vmi, offsetName, &addr) == VMI_FAILURE) {
    throw GetOffsetError();
  }
  return addr;
}


}
}

#endif /* D7364E74_200E_477B_B230_D3887D69117B */
