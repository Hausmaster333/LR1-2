#ifndef VIRTUAL_FILE_SYSTEM_H
#define VIRTUAL_FILE_SYSTEM_H

#include "vfs/file_index.h"
#include "vfs/file_system_reader.h"
#include "vfs/file.h"
#include <string>
#include <set>

class VirtualFileSystem {
    private:
        ShrdPtr<Directory> root;
        ShrdPtr<Directory> current;
        FileIndex index;

        IFileSystemReader& reader;
        ShrdPtr<Directory> build_directory(const std::filesystem::path& path) const;

        ShrdPtr<File> find(const std::string& path, bool follow_last = true) const;
        ShrdPtr<File> resolve(const std::string& path, bool follow_last,
                              std::set<const File*>& resolving) const;
        ShrdPtr<File> node_at(const std::string& path, bool follow_last = true) const;
        ShrdPtr<Directory> directory_at(const std::string& path) const;
        ShrdPtr<File> movable_node_at(const std::string& path) const;
    public:
        VirtualFileSystem();
        explicit VirtualFileSystem(IFileSystemReader& reader);
        VirtualFileSystem(const VirtualFileSystem&) = delete;
        VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;

        bool exists(const std::string& path) const;
        const File& get(const std::string& path) const;
        const Directory& get_root() const { return *root; }
        DynamicArray<ShrdPtr<const File>> list(const std::string& directory_path) const;

        std::string current_path() const { return current->get_path(); }
        void change_directory(const std::string& path);
        void make_link(const std::string& target, const std::string& path);
        bool touch(const std::string& path);
        ShrdPtr<const RegularFile> regular_file(const std::string& path) const;
        std::string read_text(const std::string& path) const;
        void save_text(const std::string& path, std::uint64_t id, const std::string& text);

        void make_directory(const std::string& path);

        void add_file(const std::string& directory_path, const std::string& name, UnqPtr<IContentSource> content);

        void attach_file(const std::string& real_path, const std::string& directory_path, const std::string& name = "");

        void import_directory(const std::string& real_path, const std::string& directory_path);

        void remove(const std::string& path); 
        void move(const std::string& path, const std::string& target_directory);
        void rename(const std::string& path, const std::string& new_name);

        DynamicArray<ShrdPtr<const RegularFile>> find_files(SearchField field, const std::string& value,  SearchMethod method) const;
};

#endif
