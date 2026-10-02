#ifndef __ARQSIM_HEADER_ARCH_COMPUTER_H__
#define __ARQSIM_HEADER_ARCH_COMPUTER_H__

#include <array>
#include <list>

#include <cstdint>

#include <my-lib/std.h>
#include <my-lib/macros.h>

#include "../config.h"
#include "device.h"

namespace Arch {

// ---------------------------------------

class Terminal;
class Disk;
class Timer;
class Memory;
class Cpu;

class Computer
{
private:
	std::list<Device*> devices;
	std::array<IO_Device*, 1 << 16> io_ports;
	Terminal *terminal;
	Disk *disk;
	Timer *timer;
	Memory *memory;
	Cpu *cpu;

	bool alive = true;
	uint64_t cycle = 0;

	std::string turn_off_msg;

	inline static Computer *computer = nullptr;

private:
	Computer ();
	~Computer ();

public:
	static void init ()
	{
		mylib_assert(computer == nullptr)
		computer = new Computer;
	}

	static Computer& get ()
	{
		mylib_assert(computer != nullptr)
		return *computer;
	}

	static void destroy ()
	{
		mylib_assert(computer != nullptr)
		delete computer;
		computer = nullptr;
	}

	void run ();

	inline Terminal& get_terminal () const
	{
		return *this->terminal;
	}

	inline Disk& get_disk () const
	{
		return *this->disk;
	}

	inline Timer& get_timer () const
	{
		return *this->timer;
	}

	inline Memory& get_memory () const
	{
		return *this->memory;
	}

	inline Cpu& get_cpu () const
	{
		return *this->cpu;
	}

	inline void set_io_port (const uint16_t port, IO_Device *device)
	{
		mylib_assert(port < this->io_ports.size())
		this->io_ports[port] = device;
	}

	inline void set_io_port (const IO_Port port, IO_Device *device)
	{
		this->set_io_port(std::to_underlying(port), device);
	}

	inline IO_Device& get_io_port (const uint16_t port) const
	{
		mylib_assert(port < this->io_ports.size())
		mylib_assert(this->io_ports[port] != nullptr)
		return *this->io_ports[port];
	}

	inline IO_Device& get_io_port (const IO_Port port) const
	{
		return this->get_io_port(std::to_underlying(port));
	}

	inline void turn_off ()
	{
		this->alive = false;
	}
};

// ---------------------------------------

} // end namespace

#endif