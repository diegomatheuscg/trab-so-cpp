#include "computer.h"
#include "terminal.h"
#include "disk.h"
#include "timer.h"
#include "memory.h"
#include "cpu.h"

// ---------------------------------------

namespace Arch {

// ---------------------------------------

Computer::Computer ()
{
	for (auto& port: this->io_ports)
		port = nullptr;
	
	this->terminal = new Terminal(*this);
	this->disk = new Disk(*this);
	this->timer = new Timer(*this);
	this->memory = new Memory(*this);
	this->cpu = new Cpu(*this);

	this->devices.push_back(this->terminal);
	this->devices.push_back(this->disk);
	this->devices.push_back(this->timer);
	this->devices.push_back(this->memory);
	this->devices.push_back(this->cpu);
}

Computer::~Computer ()
{
	for (auto *device: this->devices)
		delete device;
}

void Computer::run ()
{
	while (this->alive) {
		for (auto *device: this->devices)
			device->run_cycle();
		this->cycle++;
	}
}

// ---------------------------------------

} // end namespace