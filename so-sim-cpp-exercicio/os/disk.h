#ifndef __OS_DISK_H__
#define __OS_DISK_H__

#include <string_view>

namespace OS {

void start_disk_load(std::string_view name);
void finish_disk_load();

}

#endif
