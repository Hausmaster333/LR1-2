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

static void change_directory(VirtualFileSystem& vfs, std::istream& input, CommandResult& result) {
    vfs.change_directory(read_path(input, "cd"));
    result.navigated = true;
}

static void list_files(VirtualFileSystem& vfs, std::istream& input, CommandResult& result) {
    auto files = vfs.list(input.eof() ? "." : read_path(input, "ls"));
    for (int i = 0; i < files.get_size(); ++i) {
        const auto& file = files.get(i);
        if (file->kind() == FileKind::Link) {
            result.output += file->get_name() + " → " + static_cast<const DirectoryLink&>(*file).target_path() + "\n";
            continue;
        }
        result.output += file->get_name() + (file->is_directory() ? "/\n" : "\n");
    }
}

static void make_directory(VirtualFileSystem& vfs, std::istream& input, CommandResult& result) {
    vfs.make_directory(read_path(input, "mkdir"));
    result.changed = true;
}

static void touch_file(VirtualFileSystem& vfs, std::istream& input, CommandResult& result) {
    result.changed = vfs.touch(read_path(input, "touch"));
}

static void print_file(VirtualFileSystem& vfs, std::istream& input, CommandResult& result) {
    result.output = vfs.read_text(read_path(input, "cat"));
}

static void remove_file(VirtualFileSystem& vfs, std::istream& input, CommandResult& result) {
    vfs.remove(read_path(input, "rm"));
    result.changed = true;
}

static void make_link(VirtualFileSystem& vfs, std::istream& input, CommandResult& result) {
    std::string flag;
    if (!(input >> flag) || flag != "-s") throw std::invalid_argument("Использование: ln -s /каталог ссылка");
    input >> std::ws;
    auto target = read_path(input, "ln -s", false);
    vfs.make_link(target, read_path(input, "ln -s"));
    result.changed = true;
}

static void exit_shell(VirtualFileSystem&, std::istream& input, CommandResult& result) {
    if (!input.eof()) throw std::invalid_argument("Использование: exit");
    result.exit = true;
}

struct Command {
    std::string_view name;
    void (*run)(VirtualFileSystem&, std::istream&, CommandResult&);
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

CommandResult execute_command(VirtualFileSystem& vfs, const std::string& line) {
    CommandResult result;
    try {
        std::istringstream input(line);
        std::string name;
        if (!(input >> name)) return result;
        input >> std::ws;

        for (const auto& command : commands) {
            if (command.name == name) {
                command.run(vfs, input, result);
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
