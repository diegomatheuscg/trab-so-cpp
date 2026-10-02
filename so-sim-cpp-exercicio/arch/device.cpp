#include <array>

#include "device.h"

// ---------------------------------------

namespace Arch {

// ---------------------------------------

const char* enum_class_to_str (const InterruptCode code)
{
	static constexpr auto strs = std::to_array<const char*>({
		"Keyboard",
		"Disk",
		"Timer",
		"CpuException"
		});

	mylib_assert_msg(std::to_underlying(code) < strs.size(), "invalid interrupt code ", std::to_underlying(code))

	return strs[ std::to_underlying(code) ];
}

// ---------------------------------------

} // end namespace