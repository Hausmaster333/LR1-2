#pragma once

#include "vfs/virtual_file_system.h"

struct CommandResult {
    std::string output;
    bool changed = false;
    bool navigated = false;
    bool exit = false;
    bool failed = false;
};

CommandResult execute_command(FileSystemAccess& access, const std::string& line);
