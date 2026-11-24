#include "vfs/command.h"
#include <sstream>
#include <string_view>

static std::string read_path(std::istream& input, const std::string& command, bool last = true) {
    std::string path;
    if (input.peek() == '"') {
        input.get();
        if (!std::getline(input, path, '"') || input.eof())
            throw std::invalid_argument("Закройте двойную кавычку");
    } else {
        input >> path;
    }
    input >> std::ws;
    if (last && !input.eof()) throw std::invalid_argument("Лишние аргументы");
    if (path.empty()) throw std::invalid_argument("Использование: " + command + " <путь>");
    return path;
}

static void change_directory(FileSystemAccess& access, std::istream& input, CommandResult& result) {
    access.change_directory(read_path(input, "cd"));
    result.navigated = true;
}

static void list_files(FileSystemAccess& access, std::istream& input, CommandResult& result) {
    auto files = input.eof() ? access.current_directory().list() : access.list(read_path(input, "ls"));
    for (int i = 0; i < files.get_size(); ++i) {
        const auto& file = files.get(i);
        if (file->kind() == FileKind::Link) {
            result.output += file->get_name() + " → " + static_cast<const Link&>(*file).target_path(access.filesystem().get_root()) + "\n";
            continue;
        }
        result.output += file->get_name() + (file->is_directory() ? "/\n" : "\n");
    }
}

static void make_directory(FileSystemAccess& access, std::istream& input, CommandResult& result) {
    access.make_directory(read_path(input, "mkdir"));
    result.changed = true;
}

static void touch_file(FileSystemAccess& access, std::istream& input, CommandResult& result) {
    result.changed = access.touch(read_path(input, "touch"));
}

static void print_file(FileSystemAccess& access, std::istream& input, CommandResult& result) {
    result.output = access.read_text(read_path(input, "cat"));
}

static void remove_file(FileSystemAccess& access, std::istream& input, CommandResult& result) {
    access.remove(read_path(input, "rm"));
    result.changed = true;
}

static void make_link(FileSystemAccess& access, std::istream& input, CommandResult& result) {
    std::string flag;
    if (!(input >> flag) || flag != "-s") throw std::invalid_argument("Использование: ln -s цель ссылка");
    input >> std::ws;
    auto target = read_path(input, "ln -s", false);
    access.make_link(target, read_path(input, "ln -s"));
    result.changed = true;
}

static void exit_shell(FileSystemAccess&, std::istream& input, CommandResult& result) {
    if (!input.eof()) throw std::invalid_argument("Использование: exit");
    result.exit = true;
}

struct Command {
    std::string_view name;
    void (*run)(FileSystemAccess&, std::istream&, CommandResult&);
};

static constexpr Command commands[] = {
    {"cd", change_directory},
    {"ls", list_files},
    {"mkdir", make_directory},
    {"touch", touch_file},
    {"cat", print_file},
    {"ln", make_link},
    {"rm", remove_file},
    {"exit", exit_shell},
};

CommandResult execute_command(FileSystemAccess& access, const std::string& line) {
    CommandResult result;
    try {
        std::istringstream input(line);
        std::string name;
        if (!(input >> name)) return result;
        input >> std::ws;

        for (const auto& command : commands) {
            if (command.name == name) {
                command.run(access, input, result);
                return result;
            }
        }
        throw std::invalid_argument("Неизвестная команда: " + name);
    } catch (const std::exception& e) {
        result.failed = true;
        result.output = e.what();
    }
    return result;
}
