#include <algorithm>
#include <cstring>
#include <cstdint>
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
std::list<Process*>::iterator current_it = process_list.end();
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

void schedule()
{
    if (current_process == idle_process) {
        save_context(idle_process);
    } else if (current_process != nullptr) {
        save_context(current_process);
        if (current_process->state == ProcessState::RUNNING) {
            current_process->state = ProcessState::READY;
            if (current_it != process_list.end())
                process_list.splice(process_list.end(), process_list, current_it);
        }
    } else if (idle_process != nullptr) {
        save_context(idle_process);
    }

    if (!process_list.empty()) {
        current_it = process_list.begin();
        for (auto it = process_list.begin(); it != process_list.end(); ++it) {
            if ((*it)->state == ProcessState::READY) {
                current_it = it;
                break;
            }
        }

        current_process = *current_it;
        current_process->state = ProcessState::RUNNING;
        restore_context(current_process);
    } else {
        current_it = process_list.end();
        current_process = idle_process;
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

    std::vector<uint16_t> disk_image;
    try {
        disk_image = Lib::load_from_disk_to_16bit_buffer(name);
    } catch (...) {
        delete p;
        return nullptr;
    }

    const int num_code_pages =
        (static_cast<int>(disk_image.size()) + Config::page_size - 1) / Config::page_size;
    p->next_dynamic_page = static_cast<uint16_t>(num_code_pages);

    for (int page = 0; page < num_code_pages; page++) {
        const int frame = alloc_frames();
        if (frame == -1) {
            free_process_frames(p);
            delete p;
            return nullptr;
        }

        auto& pte = p->page_table[page];
        pte.set(Arch::Cpu::PteField::PhyFrameID, frame);
        const uint16_t physical_address =
            static_cast<uint16_t>(frame * Config::page_size);
        for (uint16_t offset = 0; offset < Config::page_size; offset++) {
            const int disk_index = page * Config::page_size + offset;
            const uint16_t value = disk_index < static_cast<int>(disk_image.size())
                ? disk_image[disk_index]
                : 0;
            global_cpu->pmem_write(physical_address + offset, value);
        }
        pte.set(Arch::Cpu::PteField::Readable, 1);
        pte.set(Arch::Cpu::PteField::Writable, 1);
        pte.set(Arch::Cpu::PteField::Executable, 1);
        pte.set(Arch::Cpu::PteField::Present, 1);
    }

    return p;
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
    current_process = idle_process;
    current_it = process_list.end();
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

    if (current_process == idle_process)
        schedule();
}

void kill_process(Process* p)
{
    if (p == nullptr || p == idle_process)
        return;

    terminal_print(global_cpu, Terminal::Kernel,
        "Processo '", p->name, "' (pid=", p->pid, ") finalizado\n");

    bool was_running = (p == current_process);

    free_process_frames(p);
    if (was_running)
        current_it = process_list.erase(current_it);
    else {
        auto it = std::find(process_list.begin(), process_list.end(), p);
        if (it != process_list.end())
            process_list.erase(it);
    }

    if (was_running)
        current_process = idle_process;

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
