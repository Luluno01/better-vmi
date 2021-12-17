#ifndef E003AF20_0A64_43A6_87B7_368EEE34B54B
#define E003AF20_0A64_43A6_87B7_368EEE34B54B

#include <libvmi/libvmi.h>
#include <guestutil/mem.hh>
#include <capstone/capstone.h>
#include <vector>  // std::vector
#include <exception>
#include <cstring>  // std::memcpy


namespace guestutil {
namespace breakpoint {


class InstructionError: public std::exception {};

class CSOpenError: public InstructionError {
public:
  virtual const char *what() const throw() {
    return "cs_open() failed";
  }
};

class DisassembleError: public InstructionError {
public:
  virtual const char *what() const throw() {
    return "Failed to disassemble the instruction";
  }
};

/**
 * @brief Wrapper of **ONE** guest instruction for emulation.
 * 
 * @tparam CSArch Capstone arch, e.g., CS_ARCH_X86.
 * @tparam CSMode Capstone mode, e.g., CS_MODE_64.
 */
template <cs_arch CSArch, cs_mode CSMode>
class Instruction {
private:
  /**
   * @brief The raw instruction buffer.
   * 
   * The instruction is assumed at most 15-byte long. Allocate 16 bytes here.
   * 
   * @see https://stackoverflow.com/questions/14698350/x86-64-asm-maximum-bytes-for-an-instruction/18972014
   */
  std::vector<uint8_t> insnData;
public:
  Instruction(): insnData(16) {};

  inline std::vector<uint8_t> getInsnData() const {
    return insnData;
  }

  /**
   * @brief Load 15 bytes from the virtual address.
   * 
   * @param vmi 
   * @param va 
   */
  inline void load(vmi_instance_t vmi, addr_t va, vmi_pid_t pid) {
    uint8_t *mInsnData = insnData.data();
    // Step 1: fetch 15 bytes of data
    memory::readVA(vmi, va, pid, 15, mInsnData);
    // Step 2: try decode the instruction data using Capstone
    csh handle;
    cs_insn *insn;
    size_t count;
    if (cs_open(CSArch, CSMode, &handle) != CS_ERR_OK) {
      throw CSOpenError();
    }
    count = cs_disasm(handle, mInsnData, 15, va, 1, &insn);
    if (!count) {
      cs_close(&handle);
      throw DisassembleError();
    }
    // Step 3: the `size` to the length of the first decoded instruction
    // because that's all we want (for now)
    insnData.resize(insn[0].size);
    // Step 4: clean up
    cs_free(insn, count);
    cs_close(&handle);
  }

  /**
   * @brief Write the instruction to a buffer.
   * 
   * @param buffer 
   */
  inline void writeTo(uint8_t *buffer) {
    std::memcpy(buffer, insnData.data(), insnData.size());
  }
};

/**
 * @brief Read one instruction into `buffer` from the virtual address `va` of
 * the process with process ID `pid`.
 * 
 * @tparam CSArch Capstone arch, e.g., CS_ARCH_X86.
 * @tparam CSMode Capstone mode, e.g., CS_MODE_64.
 * @param[in] vmi 
 * @param[in] va 
 * @param[in] pid 
 * @param[out] buffer the destination instruction buffer, which should be at
 * least a 15-byte array.
 */
template <cs_arch CSArch, cs_mode CSMode>
inline void readInstruction(
  vmi_instance_t vmi,
  addr_t va,
  vmi_pid_t pid,
  uint8_t *buffer
) {
  // Step 1: fetch 15 bytes of data
  memory::readVA(vmi, va, pid, 15, buffer);
  // Step 2: try decode the instruction data using Capstone
  // csh handle;
  // cs_insn *insn;
  // size_t count;
  // if (cs_open(CSArch, CSMode, &handle) != CS_ERR_OK) {
  //   throw CSOpenError();
  // }
  // count = cs_disasm(handle, buffer, 15, va, 1, &insn);
  // if (!count) {
  //   cs_close(&handle);
  //   throw DisassembleError();
  // }
  // // Step 3: clear trainling data?
  // // std::memset(buffer + insn[0].size, 0, 15);
  // // Step 4: clean up
  // cs_free(insn, count);
  // cs_close(&handle);
}


}
}

#endif /* E003AF20_0A64_43A6_87B7_368EEE34B54B */
