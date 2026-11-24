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
#include <optional>

struct SearchCriterion {
    std::optional<std::string> (*extract)(const File&);
    std::string (*normalize)(std::string) = nullptr;

    bool operator==(const SearchCriterion& other) const {
        return extract == other.extract && normalize == other.normalize;
    }
};

struct FileRef {
    std::uint64_t id = 0;
    WeakPtr<const File> file;

    bool operator<(const FileRef& other) const { return id < other.id; }
};

class FileIndex {
    private:
        using Index = TreeDictionary<std::string, ShrdPtr<TreeSet<FileRef>>>;

        Index groups;
        SearchCriterion field;
    public:
        explicit FileIndex(SearchCriterion criterion) : field(criterion) {
            if (!field.extract) throw std::invalid_argument("Не задан критерий индекса");
        }
        SearchCriterion criterion() const { return field; }
        void add(const ShrdPtr<const File>& file);
        void remove(const File& file);
        void build(const ShrdPtr<const File>& root);
        DynamicArray<ShrdPtr<const File>> find(const std::string& key) const;
};

#endif
