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
        std::uint64_t id;
        std::string name;
        WeakPtr<Directory> parent;
    public:
        explicit File(const std::string& name);
        std::uint64_t get_id() const { return id; }
        File(const File&) = delete;
        File& operator=(const File&) = delete;
        virtual ~File() = default;

        const std::string& get_name() const { return name; }
        ShrdPtr<Directory> get_parent() const { return parent.lock(); }
        std::string get_path() const;
        virtual FileKind kind() const = 0;
        bool is_directory() const { return kind() == FileKind::Directory; }
};

class Link : public File {
    private:
        WeakPtr<File> target;
    public:
        Link(const std::string& name, const ShrdPtr<File>& target) : File(name), target(target) {}
        FileKind kind() const override { return FileKind::Link; }
        ShrdPtr<File> destination(const File& root) const;
        std::string target_path(const File& root) const;
};

class Directory : public File {
    friend class VirtualFileSystem;
    private:
        TreeDictionary<std::string, ShrdPtr<File>> children;
        int subdirectory_count = 0;

        static void add_child(const ShrdPtr<Directory>& parent, const ShrdPtr<File>& child);
        ShrdPtr<File> remove_child(const std::string& name);
        void rename_child(const std::string& old_name, const std::string& new_name);
    public:
        explicit Directory(const std::string& name) : File(name) {}

        FileKind kind() const override { return FileKind::Directory; }
        int get_child_count() const { return children.get_count(); }
        bool has_subdirectories() const { return subdirectory_count > 0; }

        ShrdPtr<File> find_child(const std::string& name) const;
        DynamicArray<ShrdPtr<const File>> list() const;
};

class RegularFile : public File {
    private:
        UnqPtr<IContentSource> content;
    public:
        RegularFile(const std::string& name, UnqPtr<IContentSource> content);

        FileKind kind() const override { return FileKind::Regular; }
        IContentSource& get_content() { return *content; }
        const IContentSource& get_content() const { return *content; }
};

template<class F>
void visit_files(const ShrdPtr<const File>& file, const F& visit) {
    visit(file);
    if (file->is_directory()) {
        auto children = static_cast<const Directory&>(*file).list();
        for (int i = 0; i < children.get_size(); ++i)
            visit_files(children.get(i), visit);
    }
}

#endif
