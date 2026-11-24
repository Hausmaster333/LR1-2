#include "vfs/file.h"
#include <stdexcept>
#include <utility>

std::string File::get_path() const {
    ShrdPtr<Directory> owner = get_parent();
    if (!owner) return "/";

    std::string parent_path = owner->get_path();
    return (parent_path == "/" ? "" : parent_path) + "/" + name;
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

void Directory::add_child(const ShrdPtr<Directory>& parent, const ShrdPtr<File>& child) {
    if (parent->children.contains_key(child->name)) {
        throw std::invalid_argument("Имя уже занято: " + child->name);
    }
    parent->children.add(child->name, child);
    child->parent = parent;
    if (child->is_directory()) parent->subdirectory_count++;
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

    ShrdPtr<File> child = children.get(old_name);
    std::string name = new_name;
    children.add(name, child);
    children.remove(old_name);
    child->name = std::move(name);
}

static std::uint64_t next_file_id = 0;

File::File(const std::string& name) : id(next_file_id++), name(name) {}

ShrdPtr<File> Link::destination(const File& root) const {
    auto file = target.lock();
    for (auto ancestor = file; ancestor; ancestor = ancestor->get_parent()) {
        if (&*ancestor == &root) return file;
    }
    return {};
}

std::string Link::target_path(const File& root) const {
    auto file = destination(root);
    return file ? file->get_path() : "цель недоступна";
}

RegularFile::RegularFile(const std::string& name, UnqPtr<IContentSource> content)
    : File(name), content(std::move(content)) {}
