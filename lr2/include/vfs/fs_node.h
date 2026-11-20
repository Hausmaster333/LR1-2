#ifndef FS_NODE_H
#define FS_NODE_H

#include "core/dynamic_array.h"
#include "ptr/shrd_ptr.h"
#include "ptr/unq_ptr.h"
#include "ptr/weak_ptr.h"
#include "tree/tree_dictionary.h"
#include "vfs/content_source.h"
#include <cstdint>
#include <string>

class Directory;

class FsNode {
    friend class Directory;
    private:
        std::string name;
        WeakPtr<Directory> parent;
    public:
        explicit FsNode(const std::string& name) : name(name) {}
        virtual ~FsNode() = default;

        const std::string& get_name() const { return name; }
        ShrdPtr<Directory> get_parent() const { return parent.lock(); }
        std::string get_path() const;
        virtual bool is_directory() const = 0;
};

class Directory : public FsNode {
    private:
        struct Key {
            explicit Key() = default;
        };

        TreeDictionary<std::string, ShrdPtr<FsNode>> children;
        int subdirectory_count = 0;
        WeakPtr<Directory> self;
    public:
        Directory(Key, const std::string& name) : FsNode(name) {}
        static ShrdPtr<Directory> create(const std::string& name);

        bool is_directory() const override { return true; }
        int get_child_count() const { return children.get_count(); }
        bool has_subdirectories() const { return subdirectory_count > 0; }

        ShrdPtr<FsNode> find_child(const std::string& name) const;
        DynamicArray<ShrdPtr<const FsNode>> list() const;

        void add_child(ShrdPtr<FsNode> child);
        ShrdPtr<FsNode> remove_child(const std::string& name);
        void rename_child(const std::string& old_name, const std::string& new_name);
};

class File : public FsNode {
    private:
        UnqPtr<IContentSource> content;
        std::uint64_t id;
    public:
        File(const std::string& name, UnqPtr<IContentSource> content);

        bool is_directory() const override { return false; }
        const IContentSource& get_content() const { return *content; }
        std::uint64_t get_id() const { return id; }
};

#endif
