#ifndef __ARQSIM_HEADER_ARCH_MEMORY_H__
#define __ARQSIM_HEADER_ARCH_MEMORY_H__

#include <array>

#include <my-lib/std.h>
#include <my-lib/macros.h>

#include "../config.h"
#include "device.h"

namespace Arch {

// ---------------------------------------

class Memory : public Device
{
private:
	std::array<uint16_t, Config::phys_mem_size_words> data;

public:
	Memory (Computer& computer);
	~Memory ();

	void run_cycle () override final;

	inline uint16_t* get_raw ()
	{
		return this->data.data();
	}

	inline uint16_t operator[] (const uint32_t paddr) const
	{
		mylib_assert(paddr < this->data.size())
		return this->data[paddr];
	}

	inline uint16_t& operator[] (const uint32_t paddr)
	{
		mylib_assert(paddr < this->data.size())
		return this->data[paddr];
	}

	void dump (const uint16_t init = 0, const uint16_t end = Config::phys_mem_size_words-1) const;
};

// ---------------------------------------

} // end namespace

#endif