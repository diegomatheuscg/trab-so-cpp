#ifndef __ARQSIM_HEADER_ARCH_DEVICE_H__
#define __ARQSIM_HEADER_ARCH_DEVICE_H__

#include <my-lib/std.h>
#include <my-lib/exception.h>
#include <my-lib/macros.h>

#include "../config.h"

namespace Arch {

// ---------------------------------------

enum class InterruptCode : uint16_t {
	Keyboard         = 0,
	Disk             = 1,
	Timer            = 2,
	CpuException     = 3,
};

const char* enum_class_to_str (const InterruptCode code);

inline std::ostream& operator << (std::ostream& out, const InterruptCode value)
{
	out << enum_class_to_str(value);
	return out;
}

enum class IO_Port : uint16_t {
	TerminalSet               = 0,   // write
	TerminalUpload            = 1,   // write
	TerminalReadTypedChar     = 2,   // read
	TimerInterruptCycles      = 10,  // read/write
	TimerGetTimeSeconds       = 11,  // read
	DiskCmd                   = 20,  // write
	DiskData		          = 21,  // read/write
	DiskFileID		          = 22,  // read/write
	DiskState                 = 23,  // read
	DiskError                 = 24,  // read
};

// ---------------------------------------

class Computer;

class Device
{
protected:
	Computer& computer;

public:
	Device (Computer& computer)
		: computer(computer)
	{
	}

	virtual ~Device () = default;
	virtual void run_cycle () = 0;
};

// ---------------------------------------

class IO_Device : public Device
{
public:
	IO_Device (Computer& computer)
		: Device(computer)
	{
	}

	virtual ~IO_Device () = default;
	virtual uint16_t read (const uint16_t port) = 0;
	virtual void write (const uint16_t port, const uint16_t value) = 0;
};

// ---------------------------------------

} // end namespace

#endif