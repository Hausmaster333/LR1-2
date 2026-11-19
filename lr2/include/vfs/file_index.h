#ifndef FILE_INDEX_H
#define FILE_INDEX_H

#include "core/dynamic_array.h"
#include "ptr/shrd_ptr.h"
#include "tree/bst.h"
#include "tree/tree_dictionary.h"
#include "ptr/weak_ptr.h"
#include "vfs/fs_node.h"
#include <cstdint>
#include <string>

enum class SearchField { Name, Extension };
enum class SearchMethod { FullScan, Index };

// Расширение для поиска: без точки, латиница в нижнем регистре. "Report.PDF" -> "pdf",
// ".pdf" -> "pdf"; у имени без расширения и у скрытого ".bashrc" - пустая строка.
std::string extension_of(const std::string& name);
std::string normalize_extension(std::string extension); // Запрос пользователя: ".PDF" -> "pdf"

// Невладеющая ссылка на файл в множестве: файлом владеет каталог. Порядок - по номеру файла.
struct FileRef {
    std::uint64_t id = 0;
    WeakPtr<const File> file;

    bool operator<(const FileRef& other) const { return id < other.id; }
};

// Индексы файлов: словари на AVL «имя -> множество файлов» и «расширение -> множество файлов».
// Поиск, добавление и удаление файла - O(log n).
class FileIndex {
    private:
        using FileSet = BinarySearchTree<FileRef>;
        using Index = TreeDictionary<std::string, ShrdPtr<FileSet>>;

        Index by_name;
        Index by_extension;

        using FilePtr = ShrdPtr<const File>;

        static void add_to(Index& index, const std::string& key, const FilePtr& file);
        static void remove_from(Index& index, const std::string& key, const File& file);
        static DynamicArray<FilePtr> lookup(const Index& index, const std::string& key);
    public:
        void add(const FilePtr& file);
        void remove(const File& file);
        void rename(const FilePtr& file, const std::string& old_name); // Файл уже переименован
        void add_subtree(const ShrdPtr<const FsNode>& node); // Все файлы внутри узла (или сам файл)
        void remove_subtree(const ShrdPtr<const FsNode>& node);

        // Файлы с точным именем или расширением; порядок не гарантирован
        DynamicArray<FilePtr> find(SearchField field, const std::string& value) const;
};

#endif
