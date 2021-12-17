#ifndef F6E1E89D_501F_4FBF_AD27_0A7890839220
#define F6E1E89D_501F_4FBF_AD27_0A7890839220

#include <libvmi/libvmi.h>
#include <exception>


namespace guestutil {
namespace symbol {


class SymbolTranslationError: std::exception {
public:
  const char *what() const throw() {
    return "Failed to translate symbol";
  }
};

inline addr_t translateKernelSymbol(vmi_instance_t vmi, const char *symbol) {
  addr_t addr;
  if (vmi_translate_ksym2v(vmi, symbol, &addr) == VMI_FAILURE) {
    throw SymbolTranslationError();
  }
  return addr;
}


}
}

#endif /* F6E1E89D_501F_4FBF_AD27_0A7890839220 */
