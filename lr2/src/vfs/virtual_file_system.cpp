#include "core/array_sequence.h"
#include "vfs/virtual_file_system.h"
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;

// Имя узла: непустое, без '/', не "." и не ".."
static void check_name(const std::string& name) {
    if (name.empty()) throw std::invalid_argument("Имя не может быть пустым");
    if (name.find('/') != std::string::npos) throw std::invalid_argument("Имя не может содержать '/'");
    if (name == "." || name == "..") throw std::invalid_argument("Имя зарезервировано: " + name);
}

// Делит путь на родителя и последнее имя: "/a/b" -> "/a" и "b", "/a" -> "/" и "a"
static void split_path(std::string path, std::string& parent, std::string& name) {
    while (path.size() > 1 && path.back() == '/') path.pop_back();

    std::size_t slash = path.rfind('/');
    if (slash == std::string::npos) throw std::invalid_argument("Путь должен начинаться с '/': " + path);
    parent = slash == 0 ? "/" : path.substr(0, slash);
    name = path.substr(slash + 1);
}

// Имя реального файла или папки в UTF-8; у "C:/data/" берётся "data"
static std::string real_name(fs::path path) {
    if (!path.has_filename()) path = path.parent_path();
    return path.filename().u8string();
}

namespace {
    class DiskReader : public IFileSystemReader {
        public:
            UnqPtr<IContentSource> open_file(const fs::path& path) const override {
                if (!fs::is_regular_file(path)) {
                    throw std::invalid_argument("Реальный файл не найден: " + path.u8string());
                }
                return UnqPtr<RealFileSource>::make(fs::absolute(path).u8string());
            }

            ArraySequence<DiskEntry> list_directory(const fs::path& path) const override {
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
    };

    DiskReader disk_reader;
}

ShrdPtr<Directory> VirtualFileSystem::build_directory(const fs::path& path) const {
    auto directory = Directory::create(real_name(path));
    for (const auto& entry : reader.list_directory(path)) {
        if (entry.is_directory) {
            directory->add_child(build_directory(entry.path));
        } else {
            directory->add_child(ShrdPtr<File>::make(real_name(entry.path), reader.open_file(entry.path)));
        }
    }
    return directory;
}

VirtualFileSystem::VirtualFileSystem() : VirtualFileSystem(disk_reader) {}

VirtualFileSystem::VirtualFileSystem(IFileSystemReader& reader)
    : root(Directory::create("")), reader(reader) {}

ShrdPtr<FsNode> VirtualFileSystem::find(const std::string& path) const {
    if (path.empty() || path[0] != '/') throw std::invalid_argument("Путь должен начинаться с '/': " + path);

    ShrdPtr<FsNode> node = root;
    std::size_t begin = 1;
    while (begin < path.size()) {
        std::size_t end = path.find('/', begin);
        if (end == std::string::npos) end = path.size();
        if (end > begin) { // Пустые части ("//", '/' в конце) пропускаются
            if (!node->is_directory()) return {}; // Сквозь файл пройти нельзя
            node = static_cast<const Directory&>(*node).find_child(path.substr(begin, end - begin));
            if (!node) return {};
        }
        begin = end + 1;
    }
    return node;
}

ShrdPtr<FsNode> VirtualFileSystem::node_at(const std::string& path) const {
    ShrdPtr<FsNode> node = find(path);
    if (!node) throw std::invalid_argument("Путь не найден: " + path);
    return node;
}

ShrdPtr<Directory> VirtualFileSystem::directory_at(const std::string& path) const {
    ShrdPtr<FsNode> node = node_at(path);
    if (!node->is_directory()) throw std::invalid_argument("Это не каталог: " + path);
    return static_pointer_cast<Directory>(node);
}

ShrdPtr<FsNode> VirtualFileSystem::movable_node_at(const std::string& path) const {
    ShrdPtr<FsNode> node = node_at(path);
    if (node == root) throw std::invalid_argument("Корень нельзя перемещать, переименовывать и удалять");
    return node;
}

bool VirtualFileSystem::exists(const std::string& path) const {
    return static_cast<bool>(find(path));
}

// Узлом по-прежнему владеет каталог, поэтому ссылка остаётся живой после выхода
const FsNode& VirtualFileSystem::get(const std::string& path) const {
    return *node_at(path);
}

DynamicArray<ShrdPtr<const FsNode>> VirtualFileSystem::list(const std::string& directory_path) const {
    return directory_at(directory_path)->list();
}

void VirtualFileSystem::make_directory(const std::string& path) {
    std::string parent_path, name;
    split_path(path, parent_path, name);
    check_name(name);
    directory_at(parent_path)->add_child(Directory::create(name));
}

void VirtualFileSystem::add_file(const std::string& directory_path, const std::string& name,
                                 UnqPtr<IContentSource> content) {
    ShrdPtr<Directory> directory = directory_at(directory_path);
    check_name(name);
    ShrdPtr<File> file = ShrdPtr<File>::make(name, std::move(content));
    directory->add_child(file);
    index.add(file);
}

void VirtualFileSystem::attach_file(const std::string& real_path, const std::string& directory_path,
                                    const std::string& name) {
    fs::path real = fs::u8path(real_path);
    add_file(directory_path, name.empty() ? real_name(real) : name, reader.open_file(real));
}

void VirtualFileSystem::import_directory(const std::string& real_path,
                                         const std::string& directory_path) {
    ShrdPtr<Directory> directory = directory_at(directory_path);
    fs::path real = fs::u8path(real_path);

    // Каталог собирается целиком отдельно и прикрепляется в конце: при ошибке ФС не меняется
    ShrdPtr<Directory> imported = build_directory(real);
    directory->add_child(imported);
    index.add_subtree(imported);
}

void VirtualFileSystem::remove(const std::string& path) {
    ShrdPtr<FsNode> node = movable_node_at(path);
    index.remove_subtree(node);
    node->get_parent()->remove_child(node->get_name()); // Последний владелец - node: поддерево удалится с ним
}

void VirtualFileSystem::move(const std::string& path, const std::string& target_directory) {
    ShrdPtr<FsNode> node = movable_node_at(path);
    ShrdPtr<Directory> target = directory_at(target_directory);
    ShrdPtr<Directory> parent = node->get_parent();
    if (target == parent) return;

    for (ShrdPtr<Directory> ancestor = target; ancestor; ancestor = ancestor->get_parent()) {
        if (ancestor == node) throw std::invalid_argument("Каталог нельзя переместить внутрь самого себя");
    }
    if (target->find_child(node->get_name())) {
        throw std::invalid_argument("Имя уже занято: " + node->get_name());
    }

    target->add_child(parent->remove_child(node->get_name()));
}

void VirtualFileSystem::rename(const std::string& path, const std::string& new_name) {
    check_name(new_name);
    ShrdPtr<FsNode> node = movable_node_at(path);
    std::string old_name = node->get_name();
    node->get_parent()->rename_child(old_name, new_name);
    // Индекс хранит файлы, а не пути: перемещение его не трогает, переименование файла - да
    if (!node->is_directory() && old_name != new_name) {
        index.rename(static_pointer_cast<const File>(node), old_name);
    }
}

// Обход всех файлов поддерева с проверкой каждого; found растёт удвоением
static void scan(const Directory& directory, SearchField field, const std::string& value,
                 DynamicArray<ShrdPtr<const File>>& found, int& count) {
    DynamicArray<ShrdPtr<const FsNode>> children = directory.list();
    for (int position = 0; position < children.get_size(); position++) {
        const ShrdPtr<const FsNode>& child = children.get(position);
        if (child->is_directory()) {
            scan(static_cast<const Directory&>(*child), field, value, found, count);
            continue;
        }
        const std::string& name = child->get_name();
        bool matches = field == SearchField::Name ? name == value : extension_of(name) == value;
        if (!matches) continue;

        if (count == found.get_size()) found.resize(count == 0 ? 4 : count * 2);
        found.set(count++, static_pointer_cast<const File>(child));
    }
}

DynamicArray<ShrdPtr<const File>> VirtualFileSystem::find_files(SearchField field, const std::string& value,
                                                                SearchMethod method) const {
    if (method == SearchMethod::Index) return index.find(field, value);

    std::string query = field == SearchField::Extension ? normalize_extension(value) : value;
    DynamicArray<ShrdPtr<const File>> found;
    int count = 0;
    scan(*root, field, query, found, count);
    found.resize(count);
    return found;
}
