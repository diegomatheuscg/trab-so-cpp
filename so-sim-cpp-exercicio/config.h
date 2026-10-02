#ifndef __ARQSIM_HEADER_CONFIG_H__
#define __ARQSIM_HEADER_CONFIG_H__

#include <cstdint>

namespace Config {

	inline constexpr uint16_t phys_mem_size_bits = 15;

	inline constexpr uint16_t phys_mem_size_words = 1 << phys_mem_size_bits;

	inline constexpr uint16_t timer_default_interrupt_cycles = 1024;

	inline constexpr uint32_t disk_interrupt_cycles = 1024 * 10;

	// ---------------------------------------

	// Don't change this

	inline constexpr uint32_t nregs = 8;

	inline constexpr uint16_t virtual_mem_size_bits = 16;

	inline constexpr uint32_t virtual_mem_size = 1 << virtual_mem_size_bits;

	inline constexpr uint32_t page_size_bits = 4;

	inline constexpr uint16_t page_size = 1 << page_size_bits;

	inline constexpr uint32_t page_frame_id_bits = virtual_mem_size_bits - page_size_bits;

	inline constexpr uint16_t ptes_per_table = 1 << page_frame_id_bits;

	inline constexpr uint16_t ptes_flag_bits = page_size_bits;

}
#endif