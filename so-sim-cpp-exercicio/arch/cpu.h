#ifndef __ARQSIM_HEADER_ARCH_CPU_H__
#define __ARQSIM_HEADER_ARCH_CPU_H__

#include <array>

#include <my-lib/std.h>
#include <my-lib/macros.h>
#include <my-lib/bit.h>

#include "../config.h"
#include "device.h"
#include "memory.h"
#include "computer.h"

namespace Arch {

// ---------------------------------------

class Cpu : public Device
{
public:
	enum VmemMode : uint16_t {
		Disabled       = 0,
		BaseLimit      = 1,
		Paging         = 2
	};

	enum class MemAccessType : uint16_t {
		Execute        = 0,
		Read           = 1,
		Write          = 2,
	};

	struct CpuException {
		enum class Type : uint16_t {
			VmemPageFault             = 0,
			VmemGPFnotReadable        = 1,
			VmemGPFnotWritable        = 2,
			VmemGPFnotExecutable      = 3,
			GPFinvalidInstruction     = 4,
		};
		Type type;
		uint16_t vaddr;
	};

	struct PteField {
		constexpr static Mylib::BitField PhyFrameID = { 0, 12 };
		constexpr static Mylib::BitField Present = { 12, 1 };
		constexpr static Mylib::BitField Readable = { 13, 1 };
		constexpr static Mylib::BitField Writable = { 14, 1 };
		constexpr static Mylib::BitField Executable = { 15, 1 };
		constexpr static Mylib::BitField Dirty = { 16, 1 };
		constexpr static Mylib::BitField Accessed = { 17, 1 };
		constexpr static Mylib::BitField Foo = { 18, 14 };
	};

	using PageTableEntry = Mylib::BitSet<32>;
	using PageTable = std::array<PageTableEntry, Config::ptes_per_table>;

private:
	using Instruction = Mylib::BitSet<16>;

	std::array<uint16_t, Config::nregs> gprs;
	InterruptCode interrupt_code;
	bool has_interrupt = false;
	uint16_t backup_pc;

	MYLIB_OO_ENCAPSULATE_SCALAR(uint16_t, pc)
	MYLIB_OO_ENCAPSULATE_SCALAR_INIT(VmemMode, vmem_mode, VmemMode::Disabled)
	MYLIB_OO_ENCAPSULATE_SCALAR_INIT(uint16_t, vmem_paddr_base, 0)
	MYLIB_OO_ENCAPSULATE_SCALAR_INIT(uint16_t, vmem_size, Config::phys_mem_size_words)
	MYLIB_OO_ENCAPSULATE_PTR_INIT(PageTable*, page_table, nullptr)
	MYLIB_OO_ENCAPSULATE_OBJ_READONLY(CpuException, cpu_exception)

	MYLIB_OO_ENCAPSULATE_SCALAR_INIT_READONLY(uint16_t, pmem_size_words, Config::phys_mem_size_words)

public:
	Cpu (Computer& computer);
	~Cpu ();

	void run_cycle () override final;
	void dump () const;

	inline uint16_t get_gpr (const uint8_t code) const
	{
		mylib_assert(code < this->gprs.size())
		return this->gprs[code];
	}

	inline void set_gpr (const uint8_t code, const uint16_t v)
	{
		mylib_assert(code < this->gprs.size())
		this->gprs[code] = v;
	}

	inline uint16_t pmem_read (const uint16_t paddr) const
	{
		return this->computer.get_memory()[paddr];
	}

	inline void pmem_write (const uint16_t paddr, const uint16_t value)
	{
		this->computer.get_memory()[paddr] = value;
	}

	inline uint16_t read_io (const uint16_t port)
	{
		return this->computer.get_io_port(port).read(port);
	}

	inline uint16_t read_io (const IO_Port port)
	{
		return this->read_io(std::to_underlying(port));
	}

	inline void write_io (const uint16_t port, const uint16_t value)
	{
		this->computer.get_io_port(port).write(port, value);
	}

	inline void write_io (const IO_Port port, const uint16_t value)
	{
		this->write_io(std::to_underlying(port), value);
	}

	bool interrupt (const InterruptCode interrupt_code);
	void force_interrupt (const InterruptCode interrupt_code);
	void turn_off ();

private:
	void execute_r (const Instruction instruction);
	void execute_i (const Instruction instruction);

	uint16_t vmem_to_phys (const uint16_t vaddr, const MemAccessType access_type);

	inline uint16_t vmem_read_instruction (const uint16_t vaddr)
	{
		const uint16_t paddr = this->vmem_to_phys(vaddr, MemAccessType::Execute);
		return this->pmem_read(paddr);
	}

	inline uint16_t vmem_read (const uint16_t vaddr)
	{
		const uint16_t paddr = this->vmem_to_phys(vaddr, MemAccessType::Read);
		return this->pmem_read(paddr);
	}

	inline void vmem_write (const uint16_t vaddr, const uint16_t value)
	{
		const uint16_t paddr = this->vmem_to_phys(vaddr, MemAccessType::Write);
		this->pmem_write(paddr, value);
	}
};

// ---------------------------------------

const char* enum_class_to_str (const Cpu::CpuException::Type value);

inline std::ostream& operator << (std::ostream& out, const Cpu::CpuException::Type value)
{
	out << enum_class_to_str(value);
	return out;
}

// ---------------------------------------

} // end namespace

#endif