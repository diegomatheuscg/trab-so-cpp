#include <algorithm>
#include <cstring>
#include <cstdint>
#include <vector>
#include <list>
#include <stdexcept>

#include "../config.h"
#include "../lib.h"
#include "../arch/arch.h"
#include "os.h"
#include "os-lib.h"
#include "process.h"

namespace OS
{

Arch::Cpu* global_cpu = nullptr;
Process* current_process = nullptr;
Process* idle_process = nullptr;
std::list<Process*> process_list;
uint16_t next_pid = 1;

static bool frames_livres[Config::phys_mem_size_words / Config::page_size] = {false};

static int alloc_frames()
{
    for (int i = 0; i < total_frames; i++) {
        if (!frames_livres[i]) {
            frames_livres[i] = true;
            return i;
        }
    }
    terminal_println(global_cpu, Terminal::Kernel, "Sem memoria disponivel");
    return -1;
}

static void free_frames(int frame_id)
{
    if (frame_id >= 0 && frame_id < total_frames)
        frames_livres[frame_id] = false;
}

static void free_process_frames(Process* p)
{
    for (int i = 0; i < Config::ptes_per_table; i++) {
        auto& pte = p->page_table[i];
        if (pte.get(Arch::Cpu::PteField::Present) == 1) {
            int frame = pte.get(Arch::Cpu::PteField::PhyFrameID);
            free_frames(frame);
            pte.set(Arch::Cpu::PteField::Present, 0);
        }
    }
}

void save_context(Process* p)
{
    for (uint8_t i = 0; i < Config::nregs; i++)
        p->regs[i] = global_cpu->get_gpr(i);
    p->pc = global_cpu->get_pc();
}

void restore_context(Process* p)
{
    for (uint8_t i = 0; i < Config::nregs; i++)
        global_cpu->set_gpr(i, p->regs[i]);
    global_cpu->set_pc(p->pc);
    global_cpu->set_vmem_mode(Arch::Cpu::VmemMode::Paging);
    global_cpu->set_page_table(&p->page_table);
}

void check_sleeping_processes()
{
    uint16_t now = global_cpu->read_io(IO_Port::TimerGetTimeSeconds);

    for (auto* p : process_list) {
        if (p->state == ProcessState::SLEEPING) {
            int16_t diff = static_cast<int16_t>(now - p->wake_up_time);
            if (diff >= 0)
                p->state = ProcessState::READY;
        }
    }
}

void schedule()
{
    check_sleeping_processes();

    if (current_process != nullptr) {
        save_context(current_process);
        if (current_process->state == ProcessState::RUNNING) {
            current_process->state = ProcessState::READY;
            process_list.remove(current_process);
            process_list.push_back(current_process);
        }
    } else if (idle_process != nullptr) {
        save_context(idle_process);
    }

    Process* next = nullptr;
    for (auto* p : process_list) {
        if (p->state == ProcessState::READY) {
            next = p;
            break;
        }
    }

    if (next != nullptr) {
        current_process = next;
        current_process->state = ProcessState::RUNNING;
        restore_context(current_process);
    } else {
        current_process = nullptr;
        if (idle_process != nullptr)
            restore_context(idle_process);
    }
}

Process* init_process(const char* name)
{
    Process* p = new Process;

    p->pid = next_pid++;
    p->state = ProcessState::READY;
    p->pc = 1;
    p->wake_up_time = 0;
    p->start_time = global_cpu->read_io(IO_Port::TimerGetTimeSeconds);

    uint32_t j = 0;
    while (name[j] && j < 63) {
        p->name[j] = name[j];
        j++;
    }
    p->name[j] = '\0';

    for (uint8_t i = 0; i < Config::nregs; i++)
        p->regs[i] = 0;

    for (int i = 0; i < Config::ptes_per_table; i++)
        p->page_table[i] = 0;

    try {
        p->disk_image = Lib::load_from_disk_to_16bit_buffer(name);
    } catch (...) {
        delete p;
        return nullptr;
    }

    int num_code_pages = (static_cast<int>(p->disk_image.size()) + Config::page_size - 1) / Config::page_size;
    p->next_dynamic_page = static_cast<uint16_t>(num_code_pages);

    return p;
}

bool load_page(Process* p, uint16_t page_number)
{
    const int program_size = static_cast<int>(p->disk_image.size());
    const int num_pages = (program_size + Config::page_size - 1) / Config::page_size;

    if (page_number >= num_pages)
        return false;

    int frame = alloc_frames();
    if (frame == -1)
        return false;

    p->page_table[page_number].set(Arch::Cpu::PteField::PhyFrameID, frame);

    uint16_t physical_address = static_cast<uint16_t>(frame * Config::page_size);
    for (uint16_t offset = 0; offset < Config::page_size; offset++) {
        const int disk_index = page_number * Config::page_size + offset;
        uint16_t value = 0;
        if (disk_index < program_size)
            value = p->disk_image[disk_index];
        global_cpu->pmem_write(physical_address + offset, value);
    }

    p->page_table[page_number].set(Arch::Cpu::PteField::Readable, 1);
    p->page_table[page_number].set(Arch::Cpu::PteField::Writable, 1);
    p->page_table[page_number].set(Arch::Cpu::PteField::Executable, 1);
    p->page_table[page_number].set(Arch::Cpu::PteField::Present, 1);

    return true;
}

void load_idle()
{
    if (idle_process != nullptr) {
        restore_context(idle_process);
        return;
    }

    idle_process = init_process("idle.bin");
    if (idle_process == nullptr) {
        terminal_println(global_cpu, Terminal::Kernel,
            "Erro ao carregar idle.bin. Sistema sera desligado.");
        global_cpu->turn_off();
        return;
    }

    idle_process->state = ProcessState::RUNNING;
    current_process = nullptr;
    restore_context(idle_process);
}

void load_program(const char* name)
{
    Process* p = init_process(name);
    if (p == nullptr) {
        terminal_println(global_cpu, Terminal::Kernel,
            "Erro ao carregar o programa: ", name);
        return;
    }

    process_list.push_back(p);
    terminal_print(global_cpu, Terminal::Kernel,
        "Programa '", name, "' carregado. PID: ", p->pid, "\n");

    if (current_process == nullptr)
        schedule();
}

void kill_process(Process* p)
{
    if (p == nullptr)
        return;

    terminal_print(global_cpu, Terminal::Kernel,
        "Processo '", p->name, "' (pid=", p->pid, ") finalizado\n");

    bool was_running = (p == current_process);

    free_process_frames(p);
    process_list.remove(p);

    if (was_running)
        current_process = nullptr;

    delete p;

    if (was_running)
        schedule();
}

void kill_process_by_pid(uint16_t pid)
{
    for (auto* p : process_list) {
        if (p->pid == pid) {
            kill_process(p);
            return;
        }
    }
    terminal_println(global_cpu, Terminal::Command,
        "Processo com PID ", pid, " nao encontrado.");
}

}
