#include "cpu.h"
#include "../os/os.h"
#include "terminal.h"


// ---------------------------------------

namespace Arch {

// ---------------------------------------

static const char *get_reg_name_str(const uint16_t code) {
  static constexpr auto strs = std::to_array<const char *>(
      {"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7"});

  mylib_assert_msg(code < strs.size(), "invalid register code")

      return strs[code];
}

// ---------------------------------------

Cpu::Cpu(Computer &computer) : Device(computer) {
  for (auto &r : this->gprs)
    r = 0;
}

Cpu::~Cpu() {}

void Cpu::run_cycle() {
  enum class InstrType : uint16_t { R = 0, I = 1 };

  if (this->has_interrupt) { // check first if external interrupt
    this->has_interrupt = false;
    OS::interrupt(this->interrupt_code);
    return;
  }

  this->backup_pc = this->pc;

  try {
    const Instruction instruction = this->vmem_read_instruction(this->pc);

    dprintln("\tPC = ", this->pc, " instr 0x", std::hex,
             instruction.to_underlying(), std::dec, " binary ",
             instruction.to_underlying());

    this->pc++;

    const InstrType type = static_cast<InstrType>(instruction[15]);

    if (type == InstrType::R)
      this->execute_r(instruction);
    else
      this->execute_i(instruction);
  } catch (const CpuException &e) {
    this->pc = this->backup_pc;
    this->cpu_exception = e;

    OS::interrupt(InterruptCode::CpuException);
  }

  this->dump();
}

void Cpu::turn_off() { this->computer.turn_off(); }

bool Cpu::interrupt(const InterruptCode interrupt_code) {
  if (this->has_interrupt)
    return false;
  this->interrupt_code = interrupt_code;
  this->has_interrupt = true;
  return true;
}

void Cpu::force_interrupt(const InterruptCode interrupt_code) {
  mylib_assert(this->has_interrupt == false) this->interrupt(interrupt_code);
}

void Cpu::execute_r(const Instruction instruction) {
  enum class OpcodeR : uint16_t {
    Add = 0,
    Sub = 1,
    Mul = 2,
    Div = 3,
    Cmp_equal = 4,
    Cmp_neq = 5,
    Load = 15,
    Store = 16,
    Syscall = 63
  };

  const OpcodeR opcode = static_cast<OpcodeR>(instruction[{9, 6}]);
  const uint16_t dest = instruction[{6, 3}];
  const uint16_t op1 = instruction[{3, 3}];
  const uint16_t op2 = instruction[{0, 3}];

  switch (opcode) {
    using enum OpcodeR;

  case Add:
    dprintln("\tadd ", get_reg_name_str(dest), ", ", get_reg_name_str(op1),
             ", ", get_reg_name_str(op2));
    this->gprs[dest] = this->gprs[op1] + this->gprs[op2];
    break;

  case Sub:
    dprintln("\tsub ", get_reg_name_str(dest), ", ", get_reg_name_str(op1),
             ", ", get_reg_name_str(op2));
    this->gprs[dest] = this->gprs[op1] - this->gprs[op2];
    break;

  case Mul:
    dprintln("\tmul ", get_reg_name_str(dest), ", ", get_reg_name_str(op1),
             ", ", get_reg_name_str(op2));
    this->gprs[dest] = this->gprs[op1] * this->gprs[op2];
    break;

  case Div:
    dprintln("\tdiv ", get_reg_name_str(dest), ", ", get_reg_name_str(op1),
             ", ", get_reg_name_str(op2));
    this->gprs[dest] = this->gprs[op1] / this->gprs[op2];
    break;

  case Cmp_equal:
    dprintln("\tcmp_equal ", get_reg_name_str(dest), ", ",
             get_reg_name_str(op1), ", ", get_reg_name_str(op2));
    this->gprs[dest] = (this->gprs[op1] == this->gprs[op2]);
    break;

  case Cmp_neq:
    dprintln("\tcmp_neq ", get_reg_name_str(dest), ", ", get_reg_name_str(op1),
             ", ", get_reg_name_str(op2));
    this->gprs[dest] = (this->gprs[op1] != this->gprs[op2]);
    break;

  case Load:
    dprintln("\tload ", get_reg_name_str(dest), ", [", get_reg_name_str(op1),
             "]");
    this->gprs[dest] = this->vmem_read(this->gprs[op1]);
    break;

  case Store:
    dprintln("\tstore [", get_reg_name_str(op1), "], ", get_reg_name_str(op2));
    this->vmem_write(this->gprs[op1], this->gprs[op2]);
    break;

  case Syscall:
    dprintln("\tsyscall");
    OS::syscall();
    break;

  default:
    throw CpuException{.type = CpuException::Type::GPFinvalidInstruction,
                       .vaddr = this->backup_pc};
  }
}

void Cpu::execute_i(const Instruction instruction) {
  enum class OpcodeI : uint16_t { Jump = 0, Jump_cond = 1, Mov = 3 };

  const OpcodeI opcode = static_cast<OpcodeI>(instruction[{13, 2}]);
  const uint16_t reg = instruction[{10, 3}];
  const uint16_t imed = instruction[{0, 9}];

  switch (opcode) {
    using enum OpcodeI;

  case Jump:
    dprintln("\tjump ", imed);
    this->pc = imed;
    break;

  case Jump_cond:
    dprintln("\tjump_cond ", get_reg_name_str(reg), ", ", imed);
    if (this->gprs[reg] == 1)
      this->pc = imed;
    break;

  case Mov:
    dprintln("\tmov ", get_reg_name_str(reg), ", ", imed);
    this->gprs[reg] = imed;
    break;

  default:
    throw CpuException{.type = CpuException::Type::GPFinvalidInstruction,
                       .vaddr = this->backup_pc};
  }
}

uint16_t Cpu::vmem_to_phys(const uint16_t vaddr,
                           const MemAccessType access_type) {
  uint16_t paddr;

  switch (this->vmem_mode) {
  case VmemMode::Disabled:
    paddr = vaddr;
    break;

  case VmemMode::BaseLimit:
    if (vaddr >= this->vmem_size) {
      throw CpuException{.type = CpuException::Type::VmemPageFault,
                         .vaddr = vaddr};
    }

    paddr = vaddr + this->vmem_paddr_base;
    break;

  case VmemMode::Paging: {
    mylib_assert(this->page_table != nullptr)

        PageTableEntry &pte =
            (*this->page_table)[vaddr >> Config::page_size_bits];

    // first, do some protection checks

    if (pte[PteField::Present] == 0) {
      throw CpuException{.type = CpuException::Type::VmemPageFault,
                         .vaddr = vaddr};
    }

    if (access_type == MemAccessType::Read && pte[PteField::Readable] == 0) {
      throw CpuException{.type = CpuException::Type::VmemGPFnotReadable,
                         .vaddr = vaddr};
    }

    if (access_type == MemAccessType::Write && pte[PteField::Writable] == 0) {
      throw CpuException{.type = CpuException::Type::VmemGPFnotWritable,
                         .vaddr = vaddr};
    }

    if (access_type == MemAccessType::Execute &&
        pte[PteField::Executable] == 0) {
      throw CpuException{.type = CpuException::Type::VmemGPFnotExecutable,
                         .vaddr = vaddr};
    }

    // everything ok, perform the address translation

    pte.set(PteField::Accessed, 1);

    if (access_type == MemAccessType::Write)
      pte.set(PteField::Dirty, 1);

    paddr =
        Mylib::set_bits(vaddr, Config::page_size_bits,
                        Config::page_frame_id_bits, pte[PteField::PhyFrameID]);
  } break;
  }

  return paddr;
}

void Cpu::dump() const {
  dprint("gprs:");
  for (uint32_t i = 0; i < this->gprs.size(); i++)
    dprint(" ", this->gprs[i]);
  dprintln();
}

const char *enum_class_to_str(const Cpu::CpuException::Type value) {
  static constexpr auto strs = std::to_array<const char *>({
      "VmemPageFault",
      "VmemGPFnotReadable",
      "VmemGPFnotWritable",
      "VmemGPFnotExecutable",
      "GPFinvalidInstruction",
  });

  mylib_assert_msg(std::to_underlying(value) < strs.size(), "invalid value ",
                   std::to_underlying(value))

      return strs[std::to_underlying(value)];
}

// ---------------------------------------

} // namespace Arch