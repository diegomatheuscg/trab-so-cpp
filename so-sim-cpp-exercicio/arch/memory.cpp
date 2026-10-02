#include "memory.h"
#include "terminal.h"

// ---------------------------------------

namespace Arch {

// ---------------------------------------

Memory::Memory (Computer& computer)
	: Device(computer)
{
	for (auto& v: this->data)
		v = 0;
}

Memory::~Memory ()
{
	
}

void Memory::run_cycle ()
{	
}

void Memory::dump (const uint16_t init, const uint16_t end) const
{
	dprintln("memory dump from paddr ", init, " to ", end);
	for (uint16_t i = init; i < end; i++)
		dprint(this->data[i], " ");
	dprintln();
}

// ---------------------------------------

} // end namespace