#ifndef FILE_SYSTEM_READER_H
#define FILE_SYSTEM_READER_H

#include "ptr/unq_ptr.h"
#include "vfs/content_source.h"
#include <filesystem>
#include "core/array_sequence.h"

struct DiskEntry {
    std::filesystem::path path;
    bool is_directory;
};

class IFileSystemReader {
    public:
        virtual UnqPtr<IContentSource> open_file(const std::filesystem::path& path) const = 0;
        virtual ArraySequence<DiskEntry> list_directory(const std::filesystem::path& path) const = 0;
        virtual ~IFileSystemReader() = default;
};

class DiskReader final : public IFileSystemReader {
    public:
        UnqPtr<IContentSource> open_file(const std::filesystem::path& path) const override;
        ArraySequence<DiskEntry> list_directory(const std::filesystem::path& path) const override;
};

#endif
