#include <chrono>

#include "timer.h"
#include "computer.h"
#include "cpu.h"


// ---------------------------------------

namespace Arch {

// ---------------------------------------

using Clock = std::chrono::steady_clock;

static const auto start_time = Clock::now();

// ---------------------------------------

Timer::Timer (Computer& computer)
	: IO_Device(computer)
{
	this->computer.set_io_port(IO_Port::TimerInterruptCycles, this);
	this->computer.set_io_port(IO_Port::TimerGetTimeSeconds, this);
}

void Timer::run_cycle ()
{
	if (this->count >= this->timer_interrupt_cycles) {
		if (this->computer.get_cpu().interrupt(InterruptCode::Timer))
			this->count = 0;
	}
	else
		this->count++;
}

uint16_t Timer::read (const uint16_t port)
{
	const IO_Port port_enum = static_cast<IO_Port>(port);
	uint16_t r;

	switch (port_enum) {
		using enum IO_Port;

		case TimerInterruptCycles:
			r = this->timer_interrupt_cycles;
		break;

		case TimerGetTimeSeconds:
			r = std::chrono::duration_cast<std::chrono::seconds>(
				Clock::now() - start_time
			).count();
		break;

		default:
			mylib_throw_assert_msg("Timer read invalid port ", port);
	}

	return r;
}

void Timer::write (const uint16_t port, const uint16_t value)
{
	const IO_Port port_enum = static_cast<IO_Port>(port);

	switch (port_enum) {
		using enum IO_Port;

		case TimerInterruptCycles:
			this->timer_interrupt_cycles = value;
		break;

		default:
			mylib_throw_assert_msg("Timer write invalid port ", port);
	}
}

// ---------------------------------------

} // end namespace