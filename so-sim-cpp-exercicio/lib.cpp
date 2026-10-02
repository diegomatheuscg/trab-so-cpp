#include <iostream>
#include <string_view>
#include <fstream>

#include <my-lib/std.h>
#include <my-lib/exception.h>
#include <my-lib/macros.h>

#include "lib.h"

namespace Lib {

// ---------------------------------------

static uint32_t get_file_size_bytes (const std::string_view fname)
{
	std::ifstream file;
	
	file.open(fname.data(), std::ios::binary | std::ios::in);

	if (!file.is_open())
		mylib_throw_assert_msg("cannot open file ", fname);

	file.seekg(0, std::ios::end);
	const auto bsize = file.tellg();

	file.close();

	return bsize;
}

// ---------------------------------------

uint32_t get_file_size_words (const std::string_view fname)
{
	const uint32_t bsize = get_file_size_bytes(fname);

	static_assert(sizeof(uint16_t) == 2);
	mylib_assert_msg((bsize & 0x01) == 0, "file size of ", fname, " is not even")

	return bsize / sizeof(uint16_t);
}

// ---------------------------------------

static bool load_from_disk_to_buffer (const std::string_view fname, void *ptr, const uint32_t mem_size_bytes)
{
	std::ifstream file;
	
	file.open(fname.data(), std::ios::binary | std::ios::in);
	
	if (!file.is_open())
		return false;
	
	file.seekg(0, std::ios::end);
	const auto bsize = file.tellg();

	if (bsize > mem_size_bytes) {
		file.close();
		return false;
	}

	file.seekg(0, std::ios::beg);

	file.read(static_cast<char *>(ptr), bsize);

	const auto amount_read = file.gcount();

	if (amount_read != bsize) {
		file.close();
		return false;
	}

	file.close();

	return true;
}

// ---------------------------------------

std::vector<uint16_t> load_from_disk_to_16bit_buffer (const std::string_view fname)
{
	const int32_t file_size_words = Lib::get_file_size_words(fname);

	std::vector<uint16_t> buffer(file_size_words);

	if (!load_from_disk_to_buffer(fname, buffer.data(), file_size_words * sizeof(uint16_t)))
		mylib_throw_assert_msg("cannot load file ", fname);

	return buffer;
}

// ---------------------------------------

} // end namespace