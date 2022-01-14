/**
 * @file pretty-print.hh
 * @author Untitled (gnu.imm@outlook.com)
 * @brief Pretty-print functionality.
 * @version 0.1
 * @date 2021-12-17
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifndef FC533CCF_A8F8_44A4_A1F0_FA13D55AFC3D
#define FC533CCF_A8F8_44A4_A1F0_FA13D55AFC3D


#include <libvmi/libvmi.h>
#include <libvmi/events.h>
#include <ostream>
#include <string>
#include <debug.hh>


namespace pp {


#define PP_MEMBER(prepend, obj, member) \
  prepend #member ": " << obj.member

#define PP_MEMBER_HEX(prepend, obj, member) \
  prepend #member ": " << F_HEX(obj.member)

#define PP_MEMBER_UINT8(prepend, obj, member) \
  prepend #member ": " << F_UINT8(obj.member)

#define PP_MEMBER_PTR(prepend, obj, member) \
  prepend #member ": " << F_PTR(obj.member)

#define PP_MEMBER_LN(prepend, obj, member) \
  PP_MEMBER(prepend, obj, member) << std::endl

#define PP_MEMBER_HEX_LN(prepend, obj, member) \
  PP_MEMBER_HEX(prepend, obj, member) << std::endl

#define PP_MEMBER_UINT8_LN(prepend, obj, member) \
  PP_MEMBER_UINT8(prepend, obj, member) << std::endl

#define PP_MEMBER_PTR_LN(prepend, obj, member) \
  PP_MEMBER_PTR(prepend, obj, member) << std::endl


template <interrupts_t inter, int indent>
class InterruptEvent {
private:
  interrupt_event_t &event;
public:
  InterruptEvent(interrupt_event_t *_event): event(*_event) {};
  InterruptEvent(interrupt_event_t &_event): event(_event) {};

  inline friend
  std::ostream &operator<<(std::ostream &os, const InterruptEvent &event) {
    std::string ind(indent, ' ');
    os << "{" << std::endl
       << ind << "  intr: ";
    auto &e = event.event;
    switch (e.intr) {
#define CASE(x) case x: os << #x << std::endl
      CASE(INT_INVALID); break;
      CASE(INT3);
        os << ind << PP_MEMBER_LN("  ", e, insn_length)
           << ind << PP_MEMBER_UINT8_LN("  ", e, reinject)
           << ind << PP_MEMBER_PTR_LN("  ", e, gla)
           << ind << PP_MEMBER_PTR_LN("  ", e, gfn)
           << ind << PP_MEMBER_PTR_LN("  ", e, offset);
        break;
      CASE(INT_NEXT);
        os << ind << PP_MEMBER_HEX_LN("  ", e, vector)
           << ind << PP_MEMBER_LN("  ", e, type)
           << ind << PP_MEMBER_HEX_LN("  ", e, error_code)
           << ind << PP_MEMBER_HEX_LN("  ", e, cr2);
        break;
#undef CASE
      default:
        os << "???" << std::endl;
    }
    os << ind << "}";
    return os;
  }
};

class MemoryAccess {
private:
  vmi_mem_access_t access;
public:
  MemoryAccess(vmi_mem_access_t _access): access(_access) {};

  inline friend
  std::ostream &operator<<(std::ostream &os, const MemoryAccess &access) {
    switch (access.access) {
#define CASE(x) case VMI_MEMACCESS_ ## x: os << #x
      CASE(INVALID); break;
      CASE(N); break;
      CASE(R); break;
      CASE(W); break;
      CASE(X); break;
      CASE(RW); break;
      CASE(RX); break;
      CASE(WX); break;
      CASE(RWX); break;
      CASE(W2X); break;
      CASE(RWX2N); break;
#undef CASE
      default:
        os << "???";
    }
    return os;
  }
};

template <int indent>
class MemoryEvent {
private:
  mem_access_event_t &event;
public:
  MemoryEvent(mem_access_event_t *_event): event(*_event) {};
  MemoryEvent(mem_access_event_t &_event): event(_event) {};

  inline friend
  std::ostream &operator<<(std::ostream &os, const MemoryEvent &event) {
    std::string ind(indent, ' ');
    auto &e = event.event;
    os << "{" << std::endl
       << ind << PP_MEMBER_PTR_LN("  ", e, gfn)
       << ind << "  in_access: " << MemoryAccess(e.in_access) << std::endl
       << ind << "  out_access: " << MemoryAccess(e.out_access) << std::endl
       << ind << PP_MEMBER_UINT8_LN("  ", e, gptw)
       << ind << PP_MEMBER_UINT8_LN("  ", e, gla_valid)
       << ind << PP_MEMBER_PTR_LN("  ", e, gla)
       << ind << PP_MEMBER_HEX_LN("  ", e, offset)
       << ind << "}";
    return os;
  }
};


#define PP_EVENT(type) print__ ## type
#define DEFINE_PP_EVENT(type) \
  inline std::ostream \
  &print__ ## type(std::ostream &os) const

/**
 * @brief Pretty print utility class for `vmi_event_t`.
 * 
 * @tparam eventType type of the event to print.
 * @tparam indent the indentation measured by the number of spaces.
 */
template <vmi_event_type_t eventType, int indent>
class Event {
private:
  vmi_event_t &event;

#define PROLOGUE() \
  std::string ind(indent, ' ')

  DEFINE_PP_EVENT(VMI_EVENT_INVALID) {
    PROLOGUE();
    os << ind << "vmi_event_t INVALID {}";
    return os;
  }

  DEFINE_PP_EVENT(VMI_EVENT_MEMORY) {
    PROLOGUE();
    os << ind << "vmi_event_t MEMORY {" << std::endl
       << ind << PP_MEMBER_LN("  ", event, slat_id)
       << ind << PP_MEMBER_PTR_LN("  ", event, data)
       << ind << "  mem_event: " << MemoryEvent<indent + 2>(event.mem_event) << std::endl
       << ind << "}";
    return os;
  }

  DEFINE_PP_EVENT(VMI_EVENT_REGISTER) {
    PROLOGUE();
    os << ind << "vmi_event_t REGISTER {}";
    return os;
  }

  DEFINE_PP_EVENT(VMI_EVENT_SINGLESTEP) {
    PROLOGUE();
    os << ind << "vmi_event_t SINGLESTEP {}";
    return os;
  }

  DEFINE_PP_EVENT(VMI_EVENT_INTERRUPT) {
    PROLOGUE();
    os << ind << "vmi_event_t INTERRUPT {" << std::endl
       << ind << PP_MEMBER_LN("  ", event, slat_id)
       << ind << PP_MEMBER_LN("  ", event, next_slat_id)
       << ind << PP_MEMBER_LN("  ", event, data)
       << ind << PP_MEMBER_PTR_LN("  ", event, callback)
       << ind << PP_MEMBER_LN("  ", event, vcpu_id)
       << ind << PP_MEMBER_LN("  ", event, page_mode)  // FIXME
       << ind << "  interrupt_event: ";
    // This ugly switch is used to bypass the consexpr requirement of
    // `InterruptEvent`
    interrupt_event_t &intEvent = event.interrupt_event;
    switch (intEvent.intr) {
      case INT_INVALID: os << InterruptEvent<INT_INVALID, indent + 2>(intEvent); break;
      case INT3: os << InterruptEvent<INT3, indent + 2>(intEvent); break;
      case INT_NEXT: os << InterruptEvent<INT_NEXT, indent + 2>(intEvent); break;
      default: os << InterruptEvent<0xff, indent + 2>(intEvent);
    }
    os << std::endl
       << ind << '}';
    return os;
  }

  DEFINE_PP_EVENT(VMI_EVENT_GUEST_REQUEST) {
    PROLOGUE();
    os << ind << "vmi_event_t GUEST_REQUEST {}";
    return os;
  }

  DEFINE_PP_EVENT(VMI_EVENT_CPUID) {
    PROLOGUE();
    os << ind << "vmi_event_t CPUID {}";
    return os;
  }

  DEFINE_PP_EVENT(VMI_EVENT_DEBUG_EXCEPTION) {
    PROLOGUE();
    os << ind << "vmi_event_t DEBUG_EXCEPTION {}";
    return os;
  }

  DEFINE_PP_EVENT(VMI_EVENT_PRIVILEGED_CALL) {
    PROLOGUE();
    os << ind << "vmi_event_t PRIVILEGED_CALL {}";
    return os;
  }

  DEFINE_PP_EVENT(VMI_EVENT_DESCRIPTOR_ACCESS) {
    PROLOGUE();
    os << ind << "vmi_event_t DESCRIPTOR_ACCESS {}";
    return os;
  }

  DEFINE_PP_EVENT(VMI_EVENT_FAILED_EMULATION) {
    PROLOGUE();
    os << ind << "vmi_event_t FAILED_EMULATION {}";
    return os;
  }

  DEFINE_PP_EVENT(VMI_EVENT_DOMAIN_WATCH) {
    PROLOGUE();
    os << ind << "vmi_event_t DOMAIN_WATCH {}";
    return os;
  }

  DEFINE_PP_EVENT(DEFAULT) {
    PROLOGUE();
    os << ind << "vmi_event_t ??? {}";
    return os;
  }

#undef PROLOGUE

public:
  Event(vmi_event_t *_event): event(*_event) {};
  Event(vmi_event_t &_event): event(_event) {};

  inline friend
  std::ostream &operator<<(std::ostream &os, const Event &ppEvent) {
    switch (eventType) {
      case VMI_EVENT_INVALID:
        return ppEvent.PP_EVENT(VMI_EVENT_INVALID)(os);
      case VMI_EVENT_MEMORY:
        return ppEvent.PP_EVENT(VMI_EVENT_MEMORY)(os);
      case VMI_EVENT_REGISTER:
        return ppEvent.PP_EVENT(VMI_EVENT_REGISTER)(os);
      case VMI_EVENT_SINGLESTEP:
        return ppEvent.PP_EVENT(VMI_EVENT_SINGLESTEP)(os);
      case VMI_EVENT_INTERRUPT:
        return ppEvent.PP_EVENT(VMI_EVENT_INTERRUPT)(os);
      case VMI_EVENT_GUEST_REQUEST:
        return ppEvent.PP_EVENT(VMI_EVENT_GUEST_REQUEST)(os);
      case VMI_EVENT_CPUID:
        return ppEvent.PP_EVENT(VMI_EVENT_CPUID)(os);
      case VMI_EVENT_DEBUG_EXCEPTION:
        return ppEvent.PP_EVENT(VMI_EVENT_DEBUG_EXCEPTION)(os);
      case VMI_EVENT_PRIVILEGED_CALL:
        return ppEvent.PP_EVENT(VMI_EVENT_PRIVILEGED_CALL)(os);
      case VMI_EVENT_DESCRIPTOR_ACCESS:
        return ppEvent.PP_EVENT(VMI_EVENT_DESCRIPTOR_ACCESS)(os);
      case VMI_EVENT_FAILED_EMULATION:
        return ppEvent.PP_EVENT(VMI_EVENT_FAILED_EMULATION)(os);
      case VMI_EVENT_DOMAIN_WATCH:
        return ppEvent.PP_EVENT(VMI_EVENT_DOMAIN_WATCH)(os);
      default:
        return ppEvent.PP_EVENT(DEFAULT)(os);
    }
  }
};


}

#endif /* FC533CCF_A8F8_44A4_A1F0_FA13D55AFC3D */
