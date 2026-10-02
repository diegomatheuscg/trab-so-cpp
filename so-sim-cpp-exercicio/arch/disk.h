#ifndef __ARQSIM_HEADER_ARCH_DISK_H__
#define __ARQSIM_HEADER_ARCH_DISK_H__

#include <fstream>
#include <unordered_map>
#include <vector>

#include <my-lib/std.h>
#include <my-lib/macros.h>

#include "../config.h"
#include "device.h"

namespace Arch {

// ---------------------------------------

class Disk : public IO_Device
{
public:
	enum class Cmd : uint16_t {
		SetFname           = 0,
		OpenFile           = 1,
		CloseFile          = 2,
		ReadFile           = 3,
		WriteFile          = 4,
		GetFileSize        = 5,
		SeekFilePos        = 6,
	};

	enum class State : uint16_t {
		Idle                    = 0,
		SettingFname            = 1,
		ReadingFile             = 2,
		UploadingFileSize       = 3,
		UploadingFile           = 4,
	};

	enum class Error : uint16_t {
		NoError				    = 0,
		CannotOpenFile          = 1,
		FileAlreadyOpen         = 2,
		InvalidFileDescriptor   = 3,
	};

private:
	struct FileDescriptor {
		uint16_t id;
		std::string fname;
		std::fstream file;
	};

private:
	std::unordered_map<uint16_t, FileDescriptor> file_descriptors;
	uint32_t count = 0; // used for read/write operations, to count the amount of cycles AND to know how many bytes were uploaded
	uint16_t next_id = 100;
	State state = State::Idle;
	std::string fname;
	uint16_t data_written;
	uint16_t data_result;
	std::vector<uint8_t> buffer;
	FileDescriptor *current_file_descriptor = nullptr;
	Error error = Error::NoError;

public:
	Disk (Computer& computer);
	~Disk ();

	void run_cycle () override final;
	uint16_t read (const uint16_t port) override final;
	void write (const uint16_t port, const uint16_t value) override final;

private:
	void process_cmd (const uint16_t cmd_);
	uint16_t process_data_read ();
	void process_data_write (const uint16_t value);

	static std::fstream::pos_type get_file_size (std::fstream& file);
};

// ---------------------------------------

} // end namespace

#endif