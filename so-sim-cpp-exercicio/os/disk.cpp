#include <cstdint>
#include <string>
#include <vector>

#include "../arch/arch.h"
#include "disk.h"
#include "os-lib.h"
#include "os.h"
#include "process.h"

namespace OS {

static bool disk_load_pending = false;
static std::string disk_load_name;
static uint16_t disk_load_file_id = 0;
static std::vector<uint8_t> disk_load_buffer;

static void write_cmd(DiskCmd cmd)
{
    global_cpu->write_io(IO_Port::DiskCmd, static_cast<uint16_t>(cmd));
}

static void write_data(uint16_t data)
{
    global_cpu->write_io(IO_Port::DiskData, data);
}

static void select_file(uint16_t file_id)
{
    global_cpu->write_io(IO_Port::DiskFileID, file_id);
}

static void close_file(uint16_t file_id)
{
    select_file(file_id);
    write_cmd(DiskCmd::CloseFile);
}

static bool check_error(const char* action)
{
    const uint16_t err = global_cpu->read_io(IO_Port::DiskError);
    if (err != static_cast<uint16_t>(DiskError::NoError)) {
        terminal_println(global_cpu, Terminal::Command,
            "Falha ao ", action, ". Codigo do disco: ", err);
        return true;
    }
    return false;
}

//------------------------------------------------------------------------

void start_disk_load(const std::string_view name)
{
    if (disk_load_pending) {
        terminal_println(global_cpu, Terminal::Command,
            "Ja existe um carregamento de programa em andamento.");
        return;
    }

    disk_load_name = std::string(name);
    write_cmd(DiskCmd::SetFname);
    for (const char c : disk_load_name)
        write_data(static_cast<uint16_t>(c));
    write_data(0);
    write_cmd(DiskCmd::OpenFile);

    if (check_error("abrir o programa"))
        return;

    disk_load_file_id = global_cpu->read_io(IO_Port::DiskFileID);

    select_file(disk_load_file_id);
    if (check_error("selecionar o arquivo"))
        return;

    write_cmd(DiskCmd::GetFileSize);
    if (check_error("obter tamanho do arquivo"))
        return;

    const uint16_t size = global_cpu->read_io(IO_Port::DiskData);

    if (size == 0 || (size % sizeof(uint16_t)) != 0) {
        terminal_println(global_cpu, Terminal::Command,
            "Tamanho invalido do programa: ", size, " bytes.");
        close_file(disk_load_file_id);
        return;
    }

    disk_load_buffer.assign(size, 0);
    disk_load_pending = true;

    select_file(disk_load_file_id);
    if (check_error("selecionar arquivo para leitura")) {
        disk_load_pending = false;
        return;
    }

    write_data(size);
    write_cmd(DiskCmd::ReadFile);

    if (check_error("iniciar leitura do arquivo")) {
        disk_load_pending = false;
        return;
    }
}

void finish_disk_load()
{
    const uint16_t bytes_read = global_cpu->read_io(IO_Port::DiskData);
    if (bytes_read != disk_load_buffer.size()) {
        if (!check_error("ler o programa")) {
            terminal_println(global_cpu, Terminal::Command,
                "Quantidade de bytes lidos inesperada: ", bytes_read);
        }
        close_file(disk_load_file_id);
        disk_load_pending = false;
        return;
    }

    for (uint16_t i = 0; i < bytes_read; i++)
        disk_load_buffer[i] = static_cast<uint8_t>(
            global_cpu->read_io(IO_Port::DiskData));

    std::vector<uint16_t> image(bytes_read / sizeof(uint16_t));
    for (size_t i = 0; i < image.size(); i++)
        image[i] = static_cast<uint16_t>(disk_load_buffer[2 * i]) |
            static_cast<uint16_t>(disk_load_buffer[2 * i + 1]) << 8;

    close_file(disk_load_file_id);
    Process* process = init_process_from_image(
        disk_load_name.c_str(), image);
    disk_load_pending = false;
    if (process == nullptr) {
        terminal_println(global_cpu, Terminal::Command,
            "Falha ao criar o processo carregado.");
        return;
    }

    process_list.push_back(process);
    terminal_print(global_cpu, Terminal::Command,
        "Programa '", disk_load_name, "' carregado. PID: ",
        process->pid, "\n");
    if (current_process == idle_process)
        schedule();
}

}
