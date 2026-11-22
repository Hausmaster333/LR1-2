#ifndef VFS_FILE_H
#define VFS_FILE_H

#include "core/dynamic_array.h"
#include "ptr/shrd_ptr.h"
#include "ptr/unq_ptr.h"
#include "ptr/weak_ptr.h"
#include "tree/tree_dictionary.h"
#include "vfs/content_source.h"
#include <cstdint>
#include <string>

class Directory;

enum class FileKind { Regular, Directory, Link };

class File {
    friend class Directory;
    private:
        std::string name;
        WeakPtr<Directory> parent;
    public:
        explicit File(const std::string& name) : name(name) {}
        File(const File&) = delete;
        File& operator=(const File&) = delete;
        virtual ~File() = default;

        const std::string& get_name() const { return name; }
        ShrdPtr<Directory> get_parent() const { return parent.lock(); }
        std::string get_path() const;
        virtual FileKind kind() const = 0;
        bool is_directory() const { return kind() == FileKind::Directory; }
};

class DirectoryLink : public File {
    private:
        std::string target;
    public:
        DirectoryLink(const std::string& name, const std::string& target) : File(name), target(target) {}
        FileKind kind() const override { return FileKind::Link; }
        const std::string& target_path() const { return target; }
};

class Directory : public File {
    friend class VirtualFileSystem;
    private:
        struct Key {
            explicit Key() = default;
        };

        TreeDictionary<std::string, ShrdPtr<File>> children;
        int subdirectory_count = 0;
        WeakPtr<Directory> self;

        void add_child(ShrdPtr<File> child);
        ShrdPtr<File> remove_child(const std::string& name);
        void rename_child(const std::string& old_name, const std::string& new_name);
    public:
        Directory(Key, const std::string& name) : File(name) {}
        static ShrdPtr<Directory> create(const std::string& name);

        FileKind kind() const override { return FileKind::Directory; }
        int get_child_count() const { return children.get_count(); }
        bool has_subdirectories() const { return subdirectory_count > 0; }

        ShrdPtr<File> find_child(const std::string& name) const;
        DynamicArray<ShrdPtr<const File>> list() const;
};

class RegularFile : public File {
    private:
        UnqPtr<IContentSource> content;
        std::uint64_t id;
    public:
        RegularFile(const std::string& name, UnqPtr<IContentSource> content);

        FileKind kind() const override { return FileKind::Regular; }
        IContentSource& get_content() { return *content; }
        const IContentSource& get_content() const { return *content; }
        std::uint64_t get_id() const { return id; }
};

#endif
