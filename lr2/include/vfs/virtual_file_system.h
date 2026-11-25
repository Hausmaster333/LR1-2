#ifndef VIRTUAL_FILE_SYSTEM_H
#define VIRTUAL_FILE_SYSTEM_H

#include "core/list_sequence.h"
#include "core/index.h"
#include "vfs/file_system_reader.h"
#include "vfs/file.h"
#include <string>
#include <any>

enum class SearchMethod { FullScan, Index };

class FileSystemAccess;

class VirtualFileSystem {
    friend class FileSystemAccess;
    private:
        ShrdPtr<Directory> root;
        ListSequence<FileSystemAccess*> clients;
        ListSequence<std::any> indexes;
        std::optional<ArraySequence<ShrdPtr<const File>>> indexed_files;

        void invalidate_indexes();

        template <class Key>
        struct CachedIndex {
            SearchCriterion<ShrdPtr<const File>, Key> criterion;
            Index<Key> index;
        };

        template <class Key>
        const Index<Key>& get_or_build_index(SearchCriterion<ShrdPtr<const File>, Key> criterion, bool rebuild = false);

        const ArraySequence<ShrdPtr<const File>>& get_or_collect_files();

        void attach_entry(const ShrdPtr<Directory>& parent, const ShrdPtr<File>& child);
        static ShrdPtr<Directory> build_directory(const std::filesystem::path& path, const IFileSystemReader& reader);

        ShrdPtr<File> find(const std::string& path, bool follow_last = true) const;
        ShrdPtr<File> require_entry(const std::string& path, bool follow_last = true) const;

        ShrdPtr<Directory> require_directory(const std::string& path) const;
        ShrdPtr<File> require_non_root_entry(const std::string& path) const;
    public:
        VirtualFileSystem();
        VirtualFileSystem(const VirtualFileSystem&) = delete;
        VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;

        bool exists(const std::string& path) const;
        const File& get(const std::string& path) const;
        const Directory& get_root() const { return *root; }

        DynamicArray<ShrdPtr<const File>> list(const std::string& directory_path) const;

        void make_link(const std::string& target, const std::string& path);
        bool touch(const std::string& path);

        ShrdPtr<const RegularFile> regular_file(const std::string& path) const;

        std::string read_text(const std::string& path) const;
        void save_text(const std::string& path, std::uint64_t id, const std::string& text);

        void make_directory(const std::string& path);

        void add_file(const std::string& directory_path, const std::string& name, UnqPtr<IContentSource> content);

        void attach_file(const std::string& real_path, const std::string& directory_path, const std::string& name = "",
                         const IFileSystemReader& reader = DiskReader{});

        void import_directory(const std::string& real_path, const std::string& directory_path,
                              const IFileSystemReader& reader = DiskReader{});

        void remove(const std::string& path);
        void move(const std::string& path, const std::string& target_directory);
        void rename(const std::string& path, const std::string& new_name);

        template <class Key>
        void build_index(SearchCriterion<ShrdPtr<const File>, Key> criterion) { get_or_build_index(criterion, true); }

        template <class Key>
        DynamicArray<ShrdPtr<const File>> find_files(SearchCriterion<ShrdPtr<const File>, Key> criterion, const Key& key, SearchMethod method);
};

class FileSystemAccess {
    ShrdPtr<VirtualFileSystem> vfs;
    ShrdPtr<Directory> current;
    std::string absolute(const std::string& path) const;
public:
    explicit FileSystemAccess(const ShrdPtr<VirtualFileSystem>& fs) : vfs(fs) {
        if (!vfs) throw std::invalid_argument("Не задана файловая система");
        current = vfs->root;
        vfs->clients.push_back(this);
    }
    FileSystemAccess(const FileSystemAccess&) = delete;
    FileSystemAccess& operator=(const FileSystemAccess&) = delete;

    VirtualFileSystem& filesystem() const { return *vfs; }
    const Directory& current_directory() const { return *current; }
    std::string current_path() const { return current->get_path(); }
    void change_directory(const std::string& path) { current = vfs->require_directory(absolute(path)); }
    DynamicArray<ShrdPtr<const File>> list(const std::string& path) const { return vfs->list(absolute(path)); }
    void make_directory(const std::string& path) { vfs->make_directory(absolute(path)); }
    bool touch(const std::string& path) { return vfs->touch(absolute(path)); }
    std::string read_text(const std::string& path) const { return vfs->read_text(absolute(path)); }
    void remove(const std::string& path) { vfs->remove(absolute(path)); }
    void make_link(const std::string& target, const std::string& path) { vfs->make_link(absolute(target), absolute(path)); }

    ~FileSystemAccess() { vfs->clients.remove(this); }
};

#include "vfs/virtual_file_system.tpp"

#endif
