#ifndef VIRTUAL_FILE_SYSTEM_H
#define VIRTUAL_FILE_SYSTEM_H

#include "vfs/file_index.h"
#include "vfs/file_system_reader.h"
#include "vfs/fs_node.h"
#include <string>

class VirtualFileSystem {
    private:
        ShrdPtr<Directory> root;
        FileIndex index;

        IFileSystemReader& reader;
        ShrdPtr<Directory> build_directory(const std::filesystem::path& path) const;

        ShrdPtr<FsNode> find(const std::string& path) const;
        ShrdPtr<FsNode> node_at(const std::string& path) const;
        ShrdPtr<Directory> directory_at(const std::string& path) const;
        ShrdPtr<FsNode> movable_node_at(const std::string& path) const;
    public:
        VirtualFileSystem();
        explicit VirtualFileSystem(IFileSystemReader& reader);
        VirtualFileSystem(const VirtualFileSystem&) = delete;
        VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;

        bool exists(const std::string& path) const;
        const FsNode& get(const std::string& path) const;
        const Directory& get_root() const { return *root; }
        DynamicArray<ShrdPtr<const FsNode>> list(const std::string& directory_path) const;

        void make_directory(const std::string& path);

        void add_file(const std::string& directory_path, const std::string& name, UnqPtr<IContentSource> content);

        void attach_file(const std::string& real_path, const std::string& directory_path, const std::string& name = "");

        void import_directory(const std::string& real_path, const std::string& directory_path);

        void remove(const std::string& path); 
        void move(const std::string& path, const std::string& target_directory);
        void rename(const std::string& path, const std::string& new_name);

        DynamicArray<ShrdPtr<const File>> find_files(SearchField field, const std::string& value,  SearchMethod method) const;
};

#endif
