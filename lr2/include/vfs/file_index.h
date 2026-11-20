#ifndef FILE_INDEX_H
#define FILE_INDEX_H

#include "core/dynamic_array.h"
#include "ptr/shrd_ptr.h"
#include "tree/tree_set.h"
#include "tree/tree_dictionary.h"
#include "ptr/weak_ptr.h"
#include "vfs/fs_node.h"
#include <cstdint>
#include <string>

enum class SearchField { Name, Extension };
enum class SearchMethod { FullScan, Index };

std::string extension_of(const std::string& name);
std::string normalize_extension(std::string extension);

struct FileRef {
    std::uint64_t id = 0;
    WeakPtr<const File> file;

    bool operator<(const FileRef& other) const { return id < other.id; }
};

class FileIndex {
    private:
        using Index = TreeDictionary<std::string, ShrdPtr<TreeSet<FileRef>>>;

        Index by_name;
        Index by_extension;

        using FilePtr = ShrdPtr<const File>;

        static void add_to(Index& index, const std::string& key, const FilePtr& file);
        static void remove_from(Index& index, const std::string& key, const File& file);
        static DynamicArray<FilePtr> lookup(const Index& index, const std::string& key);
    public:
        void add(const FilePtr& file);
        void remove(const File& file);
        void reindex_after_rename(const FilePtr& file, const std::string& old_name);
        void add_subtree(const ShrdPtr<const FsNode>& node);
        void remove_subtree(const ShrdPtr<const FsNode>& node);

        DynamicArray<FilePtr> find(SearchField field, const std::string& value) const;
};

#endif
