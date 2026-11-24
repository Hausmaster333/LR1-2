#include "vfs/file_index.h"

void FileIndex::add(const ShrdPtr<const File>& file) {
    auto key = field.extract(*file);
    if (!key) return;
    if (!groups.contains_key(*key)) groups.add(*key, ShrdPtr<TreeSet<FileRef>>::make());
    groups.get(*key)->add({file->get_id(), file});
}

void FileIndex::remove(const File& file) {
    auto key = field.extract(file);
    if (!key || !groups.contains_key(*key)) return;
    auto& group = *groups.get(*key);
    group.remove({file.get_id(), {}});
    if (!group.get_count()) groups.remove(*key);
}

DynamicArray<ShrdPtr<const File>> FileIndex::find(const std::string& value) const {
    auto key = field.normalize ? field.normalize(value) : value;
    if (!groups.contains_key(key)) return {};
    auto& files = *groups.get(key);
    DynamicArray<ShrdPtr<const File>> result(files.get_count());
    auto iter = files.get_enumerator();
    int i = 0;
    while (iter->move_next()) result.set(i++, iter->get_current().file.lock());
    return result;
}

void FileIndex::build(const ShrdPtr<const File>& root) {
    FileIndex built(field);
    visit_files(root, [&](const auto& file) {
        if (file != root) built.add(file);
    });
    groups = std::move(built.groups);
}
