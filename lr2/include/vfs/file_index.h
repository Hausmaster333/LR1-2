#ifndef FILE_INDEX_H
#define FILE_INDEX_H

#include "core/dynamic_array.h"
#include "ptr/shrd_ptr.h"
#include "tree/tree_set.h"
#include "tree/tree_dictionary.h"
#include "ptr/weak_ptr.h"
#include "vfs/file.h"
#include <cstdint>
#include <string>

enum class SearchField { Name, Extension };
enum class SearchMethod { FullScan, Index };

// Извлекает и нормализует расширение. У имени вида .gitignore оно пустое.
std::string extension_of(const std::string& name);
// Убирает начальную точку и переводит A–Z в нижний регистр.
std::string normalize_extension(std::string extension);

struct FileRef {
    std::uint64_t id = 0;
    WeakPtr<const RegularFile> file;

    // Различает одноимённые файлы в множестве по ID.
    bool operator<(const FileRef& other) const { return id < other.id; }
};

class FileIndex {
    private:
        using Index = TreeDictionary<std::string, ShrdPtr<TreeSet<FileRef>>>;

        Index by_name;
        Index by_extension;

        using FilePtr = ShrdPtr<const RegularFile>;

        // Добавляет файл в группу по ключу, при необходимости создаёт её.
        static void add_to(Index& index, const std::string& key, const FilePtr& file);
        // Убирает файл по ID и удаляет опустевшую группу.
        static void remove_from(Index& index, const std::string& key, const RegularFile& file);
        // Собирает файлы одной группы. Для отсутствующего ключа результат пуст.
        static DynamicArray<FilePtr> lookup(const Index& index, const std::string& key);
    public:
        // Индексирует файл по имени и расширению.
        void add(const FilePtr& file);
        // Убирает файл из обоих индексов.
        void remove(const RegularFile& file);
        // Заменяет старые записи индекса после смены имени.
        void reindex_after_rename(const FilePtr& file, const std::string& old_name);
        // Индексирует обычные файлы поддерева, пропуская ссылки.
        void add_subtree(const ShrdPtr<const File>& node);
        // Убирает из индекса обычные файлы поддерева, пропуская ссылки.
        void remove_subtree(const ShrdPtr<const File>& node);

        // Возвращает группу совпадений из индекса имён или расширений.
        DynamicArray<FilePtr> find(SearchField field, const std::string& value) const;
};

#endif
