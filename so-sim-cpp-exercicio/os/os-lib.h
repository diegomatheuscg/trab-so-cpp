#ifndef __ARQSIM_HEADER_OSLIB_H__
#define __ARQSIM_HEADER_OSLIB_H__

#include <cstdint>

#include <my-lib/std.h>
#include <my-lib/macros.h>

#include "../config.h"
#include "../arch/arch.h"
#include "os.h"

namespace OS {

// ---------------------------------------s

inline bool terminal_is_backspace (const char c) noexcept
{
	return (c == 8);
}

inline bool terminal_is_alpha (const char c) noexcept
{
	return (c >= 'a') && (c <= 'z');
}

inline bool terminal_is_num (const char c) noexcept
{
	return (c >= '0') && (c <= '9');
}

inline bool terminal_is_return (const char c) noexcept
{
	return (c == '\n');
}

inline void terminal_print_str (Arch::Cpu *cpu, const Terminal video, const std::string_view str)
{
	cpu->write_io(IO_Port::TerminalSet, static_cast<uint16_t>(video));

	for (const char c : str)
		cpu->write_io(IO_Port::TerminalUpload, static_cast<uint16_t>(c));
}

template <typename... Types>
void terminal_print (Arch::Cpu *cpu, const Terminal video, Types&&... vars)
{
	const std::string str = Mylib::build_str_from_stream(vars...);
	terminal_print_str(cpu, video, str);
}

template <typename... Types>
void terminal_println (Arch::Cpu *cpu, const Terminal video, Types&&... vars)
{
	terminal_print(cpu, video, vars..., '\n');
}

// ---------------------------------------

} // end namespace

#endif