#include "core/array_sequence.h"
#include "vfs/virtual_file_system.h"
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;

static void check_name(const std::string& name) {
    if (name.empty()) throw std::invalid_argument("Имя не может быть пустым");
    if (name.find('/') != std::string::npos) throw std::invalid_argument("Имя не может содержать '/'");
    if (name == "." || name == "..") throw std::invalid_argument("Имя зарезервировано: " + name);
}

static void split_path(std::string path, std::string& parent, std::string& name) {
    while (path.size() > 1 && path.back() == '/') path.pop_back();

    std::size_t slash = path.rfind('/');
    if (slash == std::string::npos) { parent = "."; name = path; return; }
    parent = slash == 0 ? "/" : path.substr(0, slash);
    name = path.substr(slash + 1);
}

static std::string real_name(fs::path path) {
    if (!path.has_filename()) path = path.parent_path();
    return path.filename().u8string();
}

UnqPtr<IContentSource> DiskReader::open_file(const fs::path& path) const {
    if (!fs::is_regular_file(path)) {
        throw std::invalid_argument("Реальный файл не найден: " + path.u8string());
    }
    return UnqPtr<RealFileSource>::make(fs::absolute(path).u8string());
}

ArraySequence<DiskEntry> DiskReader::list_directory(const fs::path& path) const {
    if (!fs::is_directory(path)) {
        throw std::invalid_argument("Реальная папка не найдена: " + path.u8string());
    }
    ArraySequence<DiskEntry> entries;
    for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied)) {
        if (entry.is_symlink()) continue;
        if (entry.is_directory()) entries.push_back({entry.path(), true});
        else if (entry.is_regular_file()) entries.push_back({entry.path(), false});
    }
    return entries;
}

ShrdPtr<Directory> VirtualFileSystem::build_directory(const fs::path& path, const IFileSystemReader& reader) {
    auto directory = ShrdPtr<Directory>::make(real_name(path));
    for (const auto& entry : reader.list_directory(path)) {
        if (entry.is_directory) {
            Directory::add_child(directory, build_directory(entry.path, reader));
        } else {
            Directory::add_child(directory, ShrdPtr<RegularFile>::make(real_name(entry.path), reader.open_file(entry.path)));
        }
    }
    return directory;
}

VirtualFileSystem::VirtualFileSystem() : root(ShrdPtr<Directory>::make("")) {}

std::string FileSystemAccess::absolute(const std::string& path) const {
    if (path.empty()) throw std::invalid_argument("Пустой путь");
    if (path.front() == '/') return path;
    auto base = current_path();
    return (base == "/" ? base : base + "/") + path;
}

ShrdPtr<File> VirtualFileSystem::find(const std::string& path, bool follow_last) const {
    if (path.empty() || path.front() != '/') throw std::invalid_argument("Требуется абсолютный путь");
    ShrdPtr<File> node = root;
    std::size_t begin = 1;

    while (begin < path.size()) {
        std::size_t end = path.find('/', begin);
        if (end == std::string::npos) end = path.size();
        if (end > begin) {
            if (!node->is_directory()) return {};
            std::string part = path.substr(begin, end - begin);
            if (part == "..") {
                if (node != root) node = node->get_parent();
            } else if (part != ".") {
                node = static_cast<const Directory&>(*node).find_child(part);
            }
            if (!node) return {};
            if (node->kind() == FileKind::Link && (follow_last || path.find_first_not_of('/', end) != std::string::npos)) {
                node = static_cast<const Link&>(*node).destination(*root);
                if (!node) throw std::invalid_argument("Цель ссылки недоступна");
            }
        }
        begin = end + 1;
    }
    return node;
}

ShrdPtr<File> VirtualFileSystem::require_entry(const std::string& path, bool follow_last) const {
    ShrdPtr<File> node = find(path, follow_last);
    if (!node) throw std::invalid_argument("Путь не найден: " + path);
    return node;
}

ShrdPtr<Directory> VirtualFileSystem::require_directory(const std::string& path) const {
    ShrdPtr<File> node = require_entry(path);
    if (!node->is_directory()) throw std::invalid_argument("Это не каталог: " + path);
    return static_pointer_cast<Directory>(node);
}

ShrdPtr<File> VirtualFileSystem::require_non_root_entry(const std::string& path) const {
    ShrdPtr<File> node = require_entry(path, false);
    if (node == root) throw std::invalid_argument("Корень нельзя перемещать, переименовывать и удалять");
    return node;
}

bool VirtualFileSystem::exists(const std::string& path) const {
    return static_cast<bool>(find(path, false));
}

const File& VirtualFileSystem::get(const std::string& path) const {
    return *require_entry(path, false);
}

DynamicArray<ShrdPtr<const File>> VirtualFileSystem::list(const std::string& directory_path) const {
    return require_directory(directory_path)->list();
}

ShrdPtr<const RegularFile> VirtualFileSystem::regular_file(const std::string& path) const {
    auto file = require_entry(path);
    if (file->is_directory()) throw std::invalid_argument("Это каталог: " + path);
    return static_pointer_cast<const RegularFile>(file);
}

void VirtualFileSystem::make_link(const std::string& target, const std::string& path) {
    if (target.empty() || target.front() != '/') throw std::invalid_argument("Укажите абсолютный путь к цели");
    auto destination = require_entry(target);
    std::string parent, name;
    split_path(path, parent, name);
    check_name(name);
    auto link = ShrdPtr<Link>::make(name, destination);
    auto directory = require_directory(parent);
    attach_entry(directory, link);
}

bool VirtualFileSystem::touch(const std::string& path) {
    if (exists(path)) {
        regular_file(path);
        return false;
    }
    std::string parent, name;
    split_path(path, parent, name);
    add_file(parent, name, UnqPtr<TextSource>::make());
    return true;
}

std::string VirtualFileSystem::read_text(const std::string& path) const {
    return regular_file(path)->get_content().read_all();
}

void VirtualFileSystem::save_text(const std::string& path, std::uint64_t id, const std::string& text) {
    auto node = find(path);
    if (!node || node->is_directory() || static_cast<RegularFile&>(*node).get_id() != id)
        throw std::invalid_argument("Открытый файл был удалён или заменён");
    auto& file = static_cast<RegularFile&>(*node);
    unindex_entry(node);
    try { file.get_content().save(text); }
    catch (...) { index_entry(node); throw; }
    index_entry(node);
}

void VirtualFileSystem::make_directory(const std::string& path) {
    std::string parent_path, name;
    split_path(path, parent_path, name);
    check_name(name);
    attach_entry(require_directory(parent_path), ShrdPtr<Directory>::make(name));
}

void VirtualFileSystem::add_file(const std::string& directory_path, const std::string& name, UnqPtr<IContentSource> content) {
    ShrdPtr<Directory> directory = require_directory(directory_path);
    check_name(name);
    ShrdPtr<RegularFile> file = ShrdPtr<RegularFile>::make(name, std::move(content));
    attach_entry(directory, file);
}

void VirtualFileSystem::attach_file(const std::string& real_path, const std::string& directory_path,
                                    const std::string& name, const IFileSystemReader& reader) {
    fs::path real = fs::u8path(real_path);
    add_file(directory_path, name.empty() ? real_name(real) : name, reader.open_file(real));
}

void VirtualFileSystem::import_directory(const std::string& real_path, const std::string& directory_path,
                                         const IFileSystemReader& reader) {
    ShrdPtr<Directory> directory = require_directory(directory_path);
    fs::path real = fs::u8path(real_path);

    ShrdPtr<Directory> imported = build_directory(real, reader);
    attach_entry(directory, imported);
}

void VirtualFileSystem::remove(const std::string& path) {
    ShrdPtr<File> node = require_non_root_entry(path);
    for (auto client : clients) {
        for (auto directory = &client->current_directory(); directory; directory = directory->get_parent().get()) {
            if (directory == node.get()) throw std::invalid_argument("Сначала выйдите из удаляемого каталога");
        }
    }
    auto parent = node->get_parent();
    visit_files(node, [&](const auto& file) { unindex_entry(file); });
    unindex_entry(parent);
    parent->remove_child(node->get_name());
    index_entry(parent);
}

void VirtualFileSystem::move(const std::string& path, const std::string& target_directory) {
    ShrdPtr<File> node = require_non_root_entry(path);
    ShrdPtr<Directory> target = require_directory(target_directory);
    ShrdPtr<Directory> parent = node->get_parent();
    if (target == parent) return;

    for (ShrdPtr<Directory> ancestor = target; ancestor; ancestor = ancestor->get_parent()) {
        if (ancestor == node) throw std::invalid_argument("Каталог нельзя переместить внутрь самого себя");
    }
    if (target->find_child(node->get_name())) {
        throw std::invalid_argument("Имя уже занято: " + node->get_name());
    }

    visit_files(node, [&](const auto& file) { unindex_entry(file); });
    unindex_entry(parent);
    auto moved = parent->remove_child(node->get_name());
    index_entry(parent);
    attach_entry(target, moved);
}

void VirtualFileSystem::rename(const std::string& path, const std::string& new_name) {
    check_name(new_name);
    ShrdPtr<File> node = require_non_root_entry(path);
    auto parent = node->get_parent();
    if (new_name == node->get_name()) return;
    if (parent->find_child(new_name)) throw std::invalid_argument("Имя уже занято: " + new_name);
    visit_files(node, [&](const auto& file) { unindex_entry(file); });
    parent->rename_child(node->get_name(), new_name);
    visit_files(node, [&](const auto& file) { index_entry(file); });
}

void VirtualFileSystem::index_entry(const ShrdPtr<const File>& file) {
    if (file == root) return;
    for (auto& index : indexes)
        index.add(file);
}

void VirtualFileSystem::unindex_entry(const ShrdPtr<const File>& file) {
    if (file == root) return;
    for (auto& index : indexes)
        index.remove(*file);
}

void VirtualFileSystem::attach_entry(const ShrdPtr<Directory>& parent, const ShrdPtr<File>& child) {
    if (parent->find_child(child->get_name()))
        throw std::invalid_argument("Имя уже занято: " + child->get_name());
    unindex_entry(parent);
    Directory::add_child(parent, child);
    index_entry(parent);
    visit_files(child, [&](const auto& file) { index_entry(file); });
}

FileIndex& VirtualFileSystem::create_index(SearchCriterion criterion) {
    FileIndex index(criterion);
    index.build(root);
    return indexes.push_back(std::move(index));
}

void VirtualFileSystem::build_index(SearchCriterion field) {
    for (auto& index : indexes) {
        if (index.criterion() == field) { index.build(root); return; }
    }
    create_index(field);
}

DynamicArray<ShrdPtr<const File>> VirtualFileSystem::find_files(SearchCriterion field, const std::string& value,
                                                                SearchMethod method) {
    if (!field.extract) throw std::invalid_argument("Не задан критерий поиска");
    if (method == SearchMethod::Index) {
        for (const auto& index : indexes)
            if (index.criterion() == field) return index.find(value);
        return create_index(field).find(value);
    }
    auto query = field.normalize ? field.normalize(value) : value;
    ArraySequence<ShrdPtr<const File>> matches;
    visit_files(root, [&](const auto& file) {
        if (file != root && field.extract(*file) == query)
            matches.push_back(file);
    });
    DynamicArray<ShrdPtr<const File>> result(matches.size());
    for (int i = 0; i < result.get_size(); ++i)
        result.set(i, matches[i]);
    return result;
}
