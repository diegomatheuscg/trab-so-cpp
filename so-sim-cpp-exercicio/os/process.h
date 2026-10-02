#ifndef __OS_PROCESS_H__
#define __OS_PROCESS_H__

#include <list>
#include <vector>
#include <cstdint>
#include "../config.h"
#include "../arch/arch.h"
#include "os.h"

namespace OS {

enum class ProcessState : uint8_t {
    READY,
    RUNNING,
    SLEEPING,
    WAITING_DISK,
    DEAD
};

struct Process {
    uint16_t pid;
    uint16_t regs[Config::nregs];
    uint16_t pc;
    ProcessState state;
    char name[64];
    Arch::Cpu::PageTable page_table;
    std::vector<uint16_t> disk_image;
    std::vector<uint16_t> dynamic_pages;
    uint16_t next_dynamic_page;
    uint16_t wake_up_time;
    uint16_t start_time;
};

extern Arch::Cpu* global_cpu;
extern Process* current_process;
extern Process* idle_process;
extern std::list<Process*> process_list;
extern uint16_t next_pid;

constexpr int total_frames = Config::phys_mem_size_words / Config::page_size;

void save_context(Process* p);
void restore_context(Process* p);
void schedule();
void check_sleeping_processes();

Process* init_process(const char* name);
bool load_page(Process* p, uint16_t page_number);
void load_idle();
void load_program(const char* name);
void kill_process(Process* p);
void kill_process_by_pid(uint16_t pid);

}

#endif
