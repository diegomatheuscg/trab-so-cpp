#include <stdexcept>
#include <string>
#include <string_view>

#include <cstdint>
#include <cstdlib>

#include "../config.h"
#include "../lib.h"
#include "../arch/arch.h"
#include "os.h"
#include "os-lib.h"
#include "process.h"

namespace OS {

static char cmd_buffer[256];
static uint16_t cmd_length = 0;

static void parse_command(const char* cmd);

static void print_prompt()
{
	terminal_print_str(global_cpu, Terminal::Command, "> ");
}

static void redraw_line()
{
	terminal_print_str(global_cpu, Terminal::Command, "\r> ");
	for (uint16_t i = 0; i < cmd_length; i++) {
		const char str[2] = { cmd_buffer[i], '\0' };
		terminal_print_str(global_cpu, Terminal::Command, str);
	}
}

void boot(Arch::Cpu *cpu)
{
	global_cpu = cpu;
	cmd_length = 0;

	global_cpu->set_vmem_mode(Arch::Cpu::VmemMode::Paging);
	global_cpu->write_io(IO_Port::TimerInterruptCycles, Config::timer_default_interrupt_cycles);

	terminal_println(global_cpu, Terminal::Kernel, "SO inicializado");
	load_idle();
	print_prompt();
}

void interrupt(const InterruptCode interrupt)
{
	switch (interrupt) {
		case InterruptCode::Keyboard: {
			const uint16_t input = global_cpu->read_io(IO_Port::TerminalReadTypedChar);
			const char c = static_cast<char>(input);

			if (terminal_is_return(c)) {
				terminal_print_str(global_cpu, Terminal::Command, "\n");
				cmd_buffer[cmd_length] = '\0';
				parse_command(cmd_buffer);
				cmd_length = 0;
				print_prompt();
			}
			else if (terminal_is_backspace(c)) {
				if (cmd_length > 0) {
					cmd_length--;
					redraw_line();
				}
			}
			else if (cmd_length < 255) {
				cmd_buffer[cmd_length++] = c;
				global_cpu->write_io(IO_Port::TerminalSet,
					static_cast<uint16_t>(Terminal::Command));
				global_cpu->write_io(IO_Port::TerminalUpload,
					static_cast<uint16_t>(c));
			}
		}
		break;

		case InterruptCode::Timer:
			schedule();
		break;

		case InterruptCode::CpuException: {
			const auto &cpu_exception = global_cpu->get_ref_cpu_exception();
			if (cpu_exception.type == CpuException::Type::VmemPageFault) {
				if (current_process != idle_process) {
					terminal_println(global_cpu, Terminal::Kernel,
						"Falha de pagina invalida. Processo sera finalizado.");
					kill_process(current_process);
				} else {
					terminal_println(global_cpu, Terminal::Kernel,
						"Falha de pagina no idle. SO sera desligado");
					global_cpu->turn_off();
				}
			}
			else switch (cpu_exception.type) {
				case CpuException::Type::VmemGPFnotReadable:
				case CpuException::Type::VmemGPFnotWritable:
				case CpuException::Type::VmemGPFnotExecutable: {
					terminal_print(global_cpu, Terminal::Kernel,
						"GPF de protecao (", cpu_exception.type,
						", vaddr=", cpu_exception.vaddr, ").\n");
					if (current_process != idle_process)
						kill_process(current_process);
					else
						global_cpu->turn_off();
					break;
				}
				case CpuException::Type::GPFinvalidInstruction: {
					terminal_print(global_cpu, Terminal::Kernel,
						"Instrucao invalida (vaddr=", cpu_exception.vaddr, ").\n");
					if (current_process != idle_process)
						kill_process(current_process);
					else
						global_cpu->turn_off();
					break;
				}
				default:
					break;
			}
		}
		break;

		default:
		break;
	}
}

static void parse_command(const char* cmd)
{
	std::string_view command(cmd);

	if (command.empty())
		return;

	if (command == "exit") {
		terminal_println(global_cpu, Terminal::Kernel,
			"Sistema encerrado pelo usuario.");
		global_cpu->turn_off();
	}

	else if (command.starts_with("load ")) {
		std::string_view program_name = command.substr(5);

		while (!program_name.empty() && program_name.front() == ' ')
			program_name.remove_prefix(1);

		if (program_name.empty()) {
			terminal_println(global_cpu, Terminal::Command,
				"Uso: load <programa>");
		}
		else {
			std::string name_str(program_name);
			load_program(name_str.c_str());
		}
	}

	else if (command.starts_with("kill ")) {
		std::string_view args = command.substr(5);
		while (!args.empty() && args.front() == ' ')
			args.remove_prefix(1);

		if (args.empty()) {
			terminal_println(global_cpu, Terminal::Command, "Uso: kill <pid>");
		} else {
			uint16_t pid = 0;
			for (char c : args) {
				if (c >= '0' && c <= '9')
					pid = static_cast<uint16_t>(pid * 10 + (c - '0'));
				else
					break;
			}
			kill_process_by_pid(pid);
		}
	}

	else {
		terminal_println(global_cpu, Terminal::Command,
			"Comando desconhecido: ", command);
	}
}

void syscall()
{
	const uint16_t syscall_num = global_cpu->get_gpr(0);

	Process* p = current_process;

	switch (syscall_num) {

		case 0:
			if (current_process != idle_process)
				kill_process(current_process);
		break;

		case 1:
		{
			uint16_t addr = global_cpu->get_gpr(1);
			std::string str;
			bool fault = false;
			while (true) {
				uint16_t pagina = addr >> Config::page_size_bits;
				uint16_t offset = addr & (Config::page_size - 1);
				auto &pte = p->page_table[pagina];

				if (pte[Arch::Cpu::PteField::Present] == 0 ||
					pte[Arch::Cpu::PteField::Readable] == 0) {
					terminal_println(global_cpu, Terminal::Kernel,
						"GPF: pagina sem permissao de leitura durante syscall 1.");
					fault = true;
					break;
				}

				pte.set(Arch::Cpu::PteField::Accessed, 1);

				uint16_t frame = pte.get(Arch::Cpu::PteField::PhyFrameID);
				uint16_t endereco_fisico = (frame << Config::page_size_bits) | offset;

				char c = global_cpu->pmem_read(endereco_fisico);
				if (c == '\0') break;
				str += c;
				addr++;
			}

			if (fault) {
				if (current_process != idle_process)
					kill_process(current_process);
				else
					global_cpu->turn_off();
			} else {
				terminal_print_str(global_cpu, Terminal::App, str.c_str());
			}
		}
		break;

		case 2:
			terminal_print_str(global_cpu, Terminal::App, "\n");
		break;

		case 3:
		{
			uint16_t value = global_cpu->get_gpr(1);
			terminal_print(global_cpu, Terminal::App, value);
		}
		break;

		default:
			terminal_print(global_cpu, Terminal::Kernel,
				"Syscall desconhecida: ", syscall_num, "\n");
		break;
	}
}


}
