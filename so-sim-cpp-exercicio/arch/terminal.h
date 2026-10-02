#ifndef __ARQSIM_HEADER_ARCH_TERMINAL_H__
#define __ARQSIM_HEADER_ARCH_TERMINAL_H__

#if defined(CONFIG_TARGET_LINUX)
#include <ncurses.h>
#elif defined(CONFIG_TARGET_WINDOWS)
#include <ncurses/ncurses.h>
#else
#error Untested platform
#endif

#include <string>
#include <vector>

#include <my-lib/macros.h>
#include <my-lib/matrix.h>
#include <my-lib/std.h>


#include "../config.h"
#include "computer.h"
#include "device.h"


namespace Arch {

// ---------------------------------------

class VideoOutput {
private:
  using MatrixBuffer = Mylib::Matrix<char, true>;

  WINDOW *win;

  MatrixBuffer buffer;

  // cursor position in the buffer
  uint32_t x;
  uint32_t y;

public:
  VideoOutput(const uint32_t xinit, const uint32_t xend, const uint32_t yinit,
              const uint32_t yend);
  ~VideoOutput();

  void print(const std::string_view str);
  void dump() const;

private:
  void roll();
  void update();
};

// ---------------------------------------

class Terminal : public IO_Device {
public:
  enum class Type : uint16_t {
    Kernel = 0,
    Arch = 1,
    Command = 2,
    App = 3,

    Count = 4 // amount of sub-terminals
  };

private:
  std::vector<VideoOutput> videos;
  uint16_t typed_char;
  bool has_char = false;
  Type current_video = Type::Arch;

public:
  Terminal(Computer &computer);
  ~Terminal();

  void run_cycle() override final;
  uint16_t read(const uint16_t port) override final;
  void write(const uint16_t port, const uint16_t value) override final;

  void dump(const Type video) const {
    this->videos[std::to_underlying(video)].dump();
  }

  void print_str(const Type video, const std::string_view str) {
    this->videos[std::to_underlying(video)].print(str);
  }
};

// ---------------------------------------

template <typename... Types> void dprint(Types &&...vars) {
  const std::string str = Mylib::build_str_from_stream(vars...);
  Computer::get().get_terminal().print_str(Terminal::Type::Arch, str);
}

template <typename... Types> void dprintln(Types &&...vars) {
  dprint(vars..., '\n');
}

// ---------------------------------------

} // namespace Arch

#endif