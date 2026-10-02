#ifndef __ARQSIM_HEADER_ARCH_TIMER_H__
#define __ARQSIM_HEADER_ARCH_TIMER_H__

#include <cstdint>

#include <my-lib/std.h>
#include <my-lib/macros.h>

#include "../config.h"
#include "device.h"

namespace Arch {

// ---------------------------------------

class Timer : public IO_Device
{
private:
	uint16_t count = 0;
	uint16_t timer_interrupt_cycles = Config::timer_default_interrupt_cycles;

public:
	Timer (Computer& computer);

	void run_cycle () override final;
	uint16_t read (const uint16_t port) override final;
	void write (const uint16_t port, const uint16_t value) override final;
};

// ---------------------------------------

} // end namespace

#endif