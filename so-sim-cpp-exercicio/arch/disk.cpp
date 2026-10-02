#include <limits>

#include "disk.h"
#include "computer.h"
#include "cpu.h"

// ---------------------------------------

namespace Arch {

// ---------------------------------------

Disk::Disk (Computer& computer)
	: IO_Device(computer)
{
	this->computer.set_io_port(IO_Port::DiskCmd, this);
	this->computer.set_io_port(IO_Port::DiskData, this);
	this->computer.set_io_port(IO_Port::DiskFileID, this);
	this->computer.set_io_port(IO_Port::DiskState, this);
	this->computer.set_io_port(IO_Port::DiskError, this);
}

Disk::~Disk ()
{
	for (auto& it: this->file_descriptors) {
		auto& d = it.second;
		if (d.file.is_open())
			d.file.close();
	}
}

void Disk::run_cycle ()
{
	switch (state) {
		using enum State;

		case ReadingFile:
			if (this->count >= Config::disk_interrupt_cycles) {
				if (this->computer.get_cpu().interrupt(InterruptCode::Disk)) {
					this->count = 0;
					this->state = State::UploadingFileSize;
				}
			}
			else
				this->count++;
		break;

		default: ;
	}
}

uint16_t Disk::read (const uint16_t port)
{
	const IO_Port port_enum = static_cast<IO_Port>(port);
	uint16_t r;

	switch (port_enum) {
		using enum IO_Port;

		case DiskData:
			r = this->process_data_read();
		break;

		case DiskFileID:
			if (this->current_file_descriptor == nullptr) {
				r = 0;
				this->error = Error::InvalidFileDescriptor;
			}
			else {
				r = this->current_file_descriptor->id;
				this->error = Error::NoError;
			}
		break;

		case DiskState:
			r = static_cast<uint16_t>(this->state);
		break;

		case DiskError:
			r = std::to_underlying(this->error);
		break;

		default:
			mylib_throw_assert_msg("Disk read invalid port ", port);
	}

	return r;
}

void Disk::write (const uint16_t port, const uint16_t value)
{
	const IO_Port port_enum = static_cast<IO_Port>(port);

	switch (port_enum) {
		using enum IO_Port;

		case DiskCmd:
			this->process_cmd(value);
		break;
		
		case DiskData:
			this->process_data_write(value);
		break;

		case DiskFileID: {
			const auto it = this->file_descriptors.find(value);

			if (it == this->file_descriptors.end()) {
				this->current_file_descriptor = nullptr;
				this->error = Error::InvalidFileDescriptor;
			}
			else {
				this->current_file_descriptor = &it->second;
				this->error = Error::NoError;
			}
		}
		break;

		default:
			mylib_throw_assert_msg("Disk read invalid port ", port);
	}
}

void Disk::process_cmd (const uint16_t cmd_)
{
	const Cmd cmd = static_cast<Cmd>(cmd_);

	if (this->state != State::Idle)
		return;

	switch (cmd)
	{
		using enum Cmd;

		case SetFname:
			this->fname = "";
			this->state = State::SettingFname;
		break;

		case OpenFile: {
			// check if file is already open

			for (const auto& it: this->file_descriptors) {
				if (it.second.fname == this->fname) {
					this->current_file_descriptor = nullptr;
					this->error = Error::FileAlreadyOpen;
					return;
				}
			}

			FileDescriptor desc;
			desc.id = this->next_id++;
			mylib_assert(desc.id < std::numeric_limits<uint16_t>::max())
			desc.fname = std::move(this->fname);
			desc.file.open(desc.fname.data(), std::ios::binary | std::ios_base::in);

			if (!desc.file.is_open()) {
				this->current_file_descriptor = nullptr;
				this->error = Error::CannotOpenFile;
				return;
			}
			
			auto pair = this->file_descriptors.insert(std::make_pair(desc.id, std::move(desc)));
			
			if (!pair.second)
				mylib_throw_assert_msg("file descriptor already exists");

			this->current_file_descriptor = &pair.first->second;

			this->error = Error::NoError;
		};
		break;

		case CloseFile: {
			if (this->current_file_descriptor == nullptr) {
				this->error = Error::InvalidFileDescriptor;
				return;
			}

			const auto it = this->file_descriptors.find(this->current_file_descriptor->id);

			mylib_assert(it != this->file_descriptors.end())

			FileDescriptor& desc = *this->current_file_descriptor;
			desc.file.close();

			this->file_descriptors.erase(it);
			
			this->current_file_descriptor = nullptr;
			this->error = Error::NoError;
		}
		break;

		case ReadFile:
			if (this->current_file_descriptor == nullptr) {
				this->error = Error::InvalidFileDescriptor;
				return;
			}

			this->state = State::ReadingFile;
			this->count = 0;
			this->error = Error::NoError;
		break;

		case GetFileSize: {
			if (this->current_file_descriptor == nullptr) {
				this->error = Error::InvalidFileDescriptor;
				return;
			}

			const auto size = get_file_size(this->current_file_descriptor->file);

			this->data_result = size;
			this->error = Error::NoError;
		}
		break;

		default:
			mylib_throw_assert_msg("Disk invalid command ", cmd_);
	}
}

uint16_t Disk::process_data_read ()
{
	uint16_t r;

	switch (this->state) {
		using enum State;

		case Idle:
			r = this->data_result;
		break;

		case UploadingFileSize: {
			if (this->current_file_descriptor == nullptr) {
				this->error = Error::InvalidFileDescriptor;
				r = 0;
				break;
			}

			// here, we actually read the file

			const auto size_to_read = this->data_written;

			this->buffer.resize(size_to_read);

			// now, read the file

			auto& file = this->current_file_descriptor->file;

			file.read(reinterpret_cast<char*>(this->buffer.data()), size_to_read);

			const auto amount_read = file.gcount();
			this->buffer.resize(amount_read);
			r = amount_read;

			this->error = Error::NoError;

			if (amount_read > 0) {
				this->state = State::UploadingFile;
				this->count = 0;
			}
			else
				this->state = State::Idle;
		}
		break;

		case UploadingFile:
			mylib_assert(this->count < this->buffer.size())
			
			r = this->buffer[this->count++];

			if (this->count == this->buffer.size())
				this->state = State::Idle;
		break;

		default:
			mylib_throw_assert_msg("Disk invalid state ", static_cast<uint16_t>(this->state));
	}

	return r;
}

void Disk::process_data_write (const uint16_t value)
{
	switch (this->state) {
		using enum State;

		case Idle:
			this->data_written = value;
		break;

		case SettingFname: {
			char c = static_cast<char>(value);

			if (c == 0)
				this->state = State::Idle;
			else
				this->fname += c;
		}
		break;

		default:
			mylib_throw_assert_msg("Disk invalid state ", static_cast<uint16_t>(this->state));
	}
}

std::fstream::pos_type Disk::get_file_size (std::fstream& file)
{
	const auto pos = file.tellg();
	file.seekg(0, std::ios::end);
	const auto size = file.tellg();
	file.seekg(pos);

	return size;
}

// ---------------------------------------

} // end namespace