#include "vfs/file.h"
#include <atomic>
#include <stdexcept>
#include <utility>

std::string File::get_path() const {
    ShrdPtr<Directory> owner = get_parent();
    if (!owner) return "/";

    std::string parent_path = owner->get_path();
    return (parent_path == "/" ? "" : parent_path) + "/" + name;
}

ShrdPtr<Directory> Directory::create(const std::string& name) {
    ShrdPtr<Directory> directory = ShrdPtr<Directory>::make(Key(), name);
    directory->self = directory;
    return directory;
}

ShrdPtr<File> Directory::find_child(const std::string& name) const {
    return children.contains_key(name) ? children.get(name) : ShrdPtr<File>();
}

DynamicArray<ShrdPtr<const File>> Directory::list() const {
    DynamicArray<ShrdPtr<const File>> result(children.get_count());
    int index = 0;
    for (bool directories : {true, false}) {
        auto iter = children.get_enumerator();
        while (iter->move_next()) {
            const ShrdPtr<File>& child = iter->get_current().value;
            if (child->is_directory() == directories) result.set(index++, child);
        }
    }
    return result;
}

void Directory::add_child(ShrdPtr<File> child) {
    if (children.contains_key(child->name)) {
        throw std::invalid_argument("Имя уже занято: " + child->name);
    }
    children.add(child->name, child);
    child->parent = self;
    if (child->is_directory()) subdirectory_count++;
}

ShrdPtr<File> Directory::remove_child(const std::string& name) {
    ShrdPtr<File> child = children.get(name);
    children.remove(name);
    child->parent.reset();
    if (child->is_directory()) subdirectory_count--;
    return child;
}

void Directory::rename_child(const std::string& old_name, const std::string& new_name) {
    if (old_name == new_name) return;
    if (children.contains_key(new_name)) throw std::invalid_argument("Имя уже занято: " + new_name);

    ShrdPtr<File> child = remove_child(old_name);
    child->name = new_name;
    add_child(std::move(child));
}

static std::atomic<std::uint64_t> next_file_id{0};

RegularFile::RegularFile(const std::string& name, UnqPtr<IContentSource> content)
    : File(name), content(std::move(content)), id(next_file_id.fetch_add(1)) {}
