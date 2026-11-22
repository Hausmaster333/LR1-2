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
            directory->add_child(ShrdPtr<RegularFile>::make(real_name(entry.path), reader.open_file(entry.path)));
        }
    }
    return directory;
}

VirtualFileSystem::VirtualFileSystem() : VirtualFileSystem(disk_reader) {}

VirtualFileSystem::VirtualFileSystem(IFileSystemReader& reader)
    : root(Directory::create("")), current(root), reader(reader) {}

ShrdPtr<File> VirtualFileSystem::find(const std::string& path, bool follow_last) const {
    std::set<const File*> resolving;
    return resolve(path, follow_last, resolving);
}

ShrdPtr<File> VirtualFileSystem::resolve(const std::string& path, bool follow_last, std::set<const File*>& resolving) const {
    if (path.empty()) throw std::invalid_argument("Пустой путь");
    bool absolute = path.front() == '/';
    ShrdPtr<File> node = absolute ? root : current;
    std::size_t begin = absolute ? 1 : 0;

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
                const auto& link = static_cast<const DirectoryLink&>(*node);

                if (!resolving.insert(&link).second) throw std::invalid_argument("Цикл ссылок: " + link.get_path());

                auto target = resolve(link.target_path(), true, resolving);
                resolving.erase(&link);

                if (!target || !target->is_directory()) throw std::invalid_argument("Цель ссылки недоступна: " + link.target_path());
                node = target;
            }
        }
        begin = end + 1;
    }
    return node;
}

ShrdPtr<File> VirtualFileSystem::node_at(const std::string& path, bool follow_last) const {
    ShrdPtr<File> node = find(path, follow_last);
    if (!node) throw std::invalid_argument("Путь не найден: " + path);
    return node;
}

ShrdPtr<Directory> VirtualFileSystem::directory_at(const std::string& path) const {
    ShrdPtr<File> node = node_at(path);
    if (!node->is_directory()) throw std::invalid_argument("Это не каталог: " + path);
    return static_pointer_cast<Directory>(node);
}

ShrdPtr<File> VirtualFileSystem::movable_node_at(const std::string& path) const {
    ShrdPtr<File> node = node_at(path, false);
    if (node == root) throw std::invalid_argument("Корень нельзя перемещать, переименовывать и удалять");
    return node;
}

bool VirtualFileSystem::exists(const std::string& path) const {
    return static_cast<bool>(find(path, false));
}

const File& VirtualFileSystem::get(const std::string& path) const {
    return *node_at(path, false);
}

DynamicArray<ShrdPtr<const File>> VirtualFileSystem::list(const std::string& directory_path) const {
    return directory_at(directory_path)->list();
}

void VirtualFileSystem::change_directory(const std::string& path) {
    current = directory_at(path);
}

ShrdPtr<const RegularFile> VirtualFileSystem::regular_file(const std::string& path) const {
    auto file = node_at(path);
    if (file->is_directory()) throw std::invalid_argument("Это каталог: " + path);
    return static_pointer_cast<const RegularFile>(file);
}

void VirtualFileSystem::make_link(const std::string& target, const std::string& path) {
    if (target.empty() || target.front() != '/') throw std::invalid_argument("Укажите абсолютный путь к каталогу");
    directory_at(target);
    std::string parent, name;
    split_path(path, parent, name);
    check_name(name);
    directory_at(parent)->add_child(ShrdPtr<DirectoryLink>::make(name, target));
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
    file.get_content().save(text);
}

void VirtualFileSystem::make_directory(const std::string& path) {
    std::string parent_path, name;
    split_path(path, parent_path, name);
    check_name(name);
    directory_at(parent_path)->add_child(Directory::create(name));
}

void VirtualFileSystem::add_file(const std::string& directory_path, const std::string& name, UnqPtr<IContentSource> content) {
    ShrdPtr<Directory> directory = directory_at(directory_path);
    check_name(name);
    ShrdPtr<RegularFile> file = ShrdPtr<RegularFile>::make(name, std::move(content));
    directory->add_child(file);
    index.add(file);
}

void VirtualFileSystem::attach_file(const std::string& real_path, const std::string& directory_path, const std::string& name) {
    fs::path real = fs::u8path(real_path);
    add_file(directory_path, name.empty() ? real_name(real) : name, reader.open_file(real));
}

void VirtualFileSystem::import_directory(const std::string& real_path, const std::string& directory_path) {
    ShrdPtr<Directory> directory = directory_at(directory_path);
    fs::path real = fs::u8path(real_path);

    ShrdPtr<Directory> imported = build_directory(real);
    directory->add_child(imported);
    index.add_subtree(imported);
}

void VirtualFileSystem::remove(const std::string& path) {
    ShrdPtr<File> node = movable_node_at(path);
    for (auto ancestor = current; ancestor; ancestor = ancestor->get_parent()) {
        if (ancestor == node) throw std::invalid_argument("Сначала выйдите из удаляемого каталога");
    }
    index.remove_subtree(node);
    node->get_parent()->remove_child(node->get_name());
}

void VirtualFileSystem::move(const std::string& path, const std::string& target_directory) {
    ShrdPtr<File> node = movable_node_at(path);
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
    ShrdPtr<File> node = movable_node_at(path);
    std::string old_name = node->get_name();
    node->get_parent()->rename_child(old_name, new_name);

    if (node->kind() == FileKind::Regular && old_name != new_name) {
        index.reindex_after_rename(static_pointer_cast<const RegularFile>(node), old_name);
    }
}

static void scan(const Directory& directory, SearchField field, const std::string& value, 
                 DynamicArray<ShrdPtr<const RegularFile>>& found, int& count) {
    DynamicArray<ShrdPtr<const File>> children = directory.list();
    for (int position = 0; position < children.get_size(); position++) {
        const ShrdPtr<const File>& child = children.get(position);
        if (child->kind() == FileKind::Link) continue;
        if (child->is_directory()) {
            scan(static_cast<const Directory&>(*child), field, value, found, count);
            continue;
        }
        const std::string& name = child->get_name();
        bool matches = field == SearchField::Name ? name == value : extension_of(name) == value;
        if (!matches) continue;

        if (count == found.get_size()) found.resize(count == 0 ? 4 : count * 2);
        found.set(count++, static_pointer_cast<const RegularFile>(child));
    }
}

DynamicArray<ShrdPtr<const RegularFile>> VirtualFileSystem::find_files(SearchField field, const std::string& value,
                                                                SearchMethod method) const {
    if (method == SearchMethod::Index) return index.find(field, value);

    std::string query = field == SearchField::Extension ? normalize_extension(value) : value;
    DynamicArray<ShrdPtr<const RegularFile>> found;
    int count = 0;
    scan(*root, field, query, found, count);
    found.resize(count);
    return found;
}
