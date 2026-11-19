#include "vfs/file_index.h"

std::string extension_of(const std::string& name) {
    std::size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0) return "";
    return normalize_extension(name.substr(dot + 1));
}

std::string normalize_extension(std::string extension) {
    if (!extension.empty() && extension[0] == '.') extension.erase(0, 1);
    for (char& symbol : extension) {
        if (symbol >= 'A' && symbol <= 'Z') symbol = static_cast<char>(symbol - 'A' + 'a');
    }
    return extension;
}

void FileIndex::add_to(Index& index, const std::string& key, const FilePtr& file) {
    if (!index.contains_key(key)) index.add(key, ShrdPtr<FileSet>::make());
    index.get(key)->insert(FileRef{file->get_id(), file});
}

void FileIndex::remove_from(Index& index, const std::string& key, const File& file) {
    FileSet& files = *index.get(key);
    files.remove(FileRef{file.get_id(), WeakPtr<const File>()});
    if (files.get_count() == 0) index.remove(key); // Пустые ключи не копятся
}

// Файлы из индекса удаляются раньше, чем из каталогов, поэтому lock() здесь не пуст
DynamicArray<FileIndex::FilePtr> FileIndex::lookup(const Index& index, const std::string& key) {
    if (!index.contains_key(key)) return DynamicArray<FilePtr>();

    const FileSet& files = *index.get(key);
    DynamicArray<FilePtr> result(files.get_count());
    int position = 0;
    auto iter = files.get_enumerator();
    while (iter->move_next()) result.set(position++, iter->get_current().file.lock());
    return result;
}

void FileIndex::add(const FilePtr& file) {
    add_to(by_name, file->get_name(), file);
    add_to(by_extension, extension_of(file->get_name()), file);
}

void FileIndex::remove(const File& file) {
    remove_from(by_name, file.get_name(), file);
    remove_from(by_extension, extension_of(file.get_name()), file);
}

void FileIndex::rename(const FilePtr& file, const std::string& old_name) {
    remove_from(by_name, old_name, *file);
    remove_from(by_extension, extension_of(old_name), *file);
    add(file);
}

void FileIndex::add_subtree(const ShrdPtr<const FsNode>& node) {
    if (!node->is_directory()) {
        add(static_pointer_cast<const File>(node));
        return;
    }
    DynamicArray<ShrdPtr<const FsNode>> children = static_cast<const Directory&>(*node).list();
    for (int index = 0; index < children.get_size(); index++) add_subtree(children.get(index));
}

void FileIndex::remove_subtree(const ShrdPtr<const FsNode>& node) {
    if (!node->is_directory()) {
        remove(static_cast<const File&>(*node));
        return;
    }
    DynamicArray<ShrdPtr<const FsNode>> children = static_cast<const Directory&>(*node).list();
    for (int index = 0; index < children.get_size(); index++) remove_subtree(children.get(index));
}

DynamicArray<FileIndex::FilePtr> FileIndex::find(SearchField field, const std::string& value) const {
    return field == SearchField::Name ? lookup(by_name, value)
                                      : lookup(by_extension, normalize_extension(value));
}
