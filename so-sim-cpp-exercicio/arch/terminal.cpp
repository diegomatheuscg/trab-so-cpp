#include "terminal.h"
#include "computer.h"
#include "cpu.h"
 
// ---------------------------------------

namespace Arch {

// ---------------------------------------

VideoOutput::VideoOutput (const uint32_t xinit, const uint32_t xend, const uint32_t yinit, const uint32_t yend)
{
	const uint32_t w = xend - xinit;
	const uint32_t h = yend - yinit;

	this->buffer = MatrixBuffer(h - 2, w - 2);
	this->buffer.set_all(' ');

	this->x = 0;
	this->y = 0;

	this->win = newwin(h, w, yinit, xinit);
	refresh();
	box(this->win, 0, 0);
	wrefresh(this->win);

	this->update();
}

VideoOutput::~VideoOutput ()
{

}

void VideoOutput::print (const std::string_view str)
{
	const auto len = str.size();
	const auto nrows = this->buffer.get_nrows();
	const auto ncols = this->buffer.get_ncols();

	for (uint32_t i = 0; i < len; i++) {
		if (this->x >= ncols) {
			this->x = 0;
			this->y++;

			if (this->y >= nrows) {
				this->roll();
				this->y--;
			}
		}

		if (str[i] == '\n') {
			// fill the rest of the line with spaces
			for (uint32_t i = this->x; i < ncols; i++)
				this->buffer[this->y, i] = ' ';
			
			this->x = 0;
			this->y++;

			if (this->y >= nrows) {
				this->roll();
				this->y--;
			}
		}
		else if (str[i] == '\r') {
			this->x = 0;

			for (uint32_t i = 0; i < ncols; i++)
				this->buffer[this->y, i] = ' ';
		}
		else {
			this->buffer[this->y, this->x] = str[i];
			this->x++;
		}
	}

	this->update();
}

void VideoOutput::roll ()
{
	const auto nrows = this->buffer.get_nrows();
	const auto ncols = this->buffer.get_ncols();

	for (uint32_t row = 1; row < nrows; row++) {
		for (uint32_t col = 0; col < ncols; col++)
			this->buffer[row-1, col] = this->buffer[row, col];
	}

	// clear last line
	for (uint32_t col = 0; col < ncols; col++)
		this->buffer[nrows-1, col] = ' ';
}

void VideoOutput::update ()
{
	const auto nrows = this->buffer.get_nrows();
	const auto ncols = this->buffer.get_ncols();

	for (uint32_t row = 0; row < nrows; row++) {
		for (uint32_t col = 0; col < ncols; col++)
			mvwprintw(this->win, row+1, col+1, "%c", this->buffer[row, col]);
	}

	refresh();
	wrefresh(this->win);
}

void VideoOutput::dump () const
{
	const auto nrows = this->buffer.get_nrows();
	const auto ncols = this->buffer.get_ncols();

	for (uint32_t row = 0; row < nrows; row++) {
		for (uint32_t col = 0; col < ncols; col++)
			std::cout << this->buffer[row, col];
		std::cout << std::endl;
	}
}

// ---------------------------------------

Terminal::Terminal (Computer& computer)
	: IO_Device(computer)
{
	const uint32_t total_w = COLS;
	const uint32_t total_h = LINES;

	this->videos.reserve( std::to_underlying(Type::Count) );

	// arch video
	this->videos.emplace_back(1, total_w/3, 1, total_h);

	// kernel video
	this->videos.emplace_back(total_w/3 + 1, 2*(total_w/3), 1, total_h/2);

	// command video
	this->videos.emplace_back(total_w/3 + 1, 2*(total_w/3), total_h/2 + 1, total_h);

	// app video
	this->videos.emplace_back(2*(total_w/3) + 1, total_w, 1, total_h);

	this->computer.set_io_port(IO_Port::TerminalSet, this);
	this->computer.set_io_port(IO_Port::TerminalUpload, this);
	this->computer.set_io_port(IO_Port::TerminalReadTypedChar, this);
}

Terminal::~Terminal ()
{
}

void Terminal::run_cycle ()
{
	const int typed = getch();

	if (typed != ERR) {
		this->has_char = true;

		if (typed == KEY_BACKSPACE || typed == 127) // || '\b'
			this->typed_char = 8;
		else
			this->typed_char = typed;
	}

	if (this->has_char)
		this->computer.get_cpu().interrupt(InterruptCode::Keyboard);
}

uint16_t Terminal::read (const uint16_t port)
{
	const IO_Port port_enum = static_cast<IO_Port>(port);
	uint16_t r;

	switch (port_enum) {
		using enum IO_Port;

		case TerminalSet:
			r = std::to_underlying(this->current_video);
		break;
		
		case TerminalReadTypedChar:
			this->has_char = false;
			r = this->typed_char;
		break;

		default:
			mylib_throw_assert_msg("Terminal read invalid port ", port);
	}

	return r;
}

void Terminal::write (const uint16_t port, const uint16_t value)
{
	const IO_Port port_enum = static_cast<IO_Port>(port);

	switch (port_enum) {
		using enum IO_Port;

		case TerminalSet:
			this->current_video = static_cast<Type>(value);
		break;

		case TerminalUpload: {
			const char str[2] = { static_cast<char>(value), 0 };
			this->videos[ std::to_underlying(this->current_video) ].print(str);
		}
		break;

		default:
			mylib_throw_assert_msg("Terminal write invalid port ", port);
	}
}

// ---------------------------------------

} // end namespace