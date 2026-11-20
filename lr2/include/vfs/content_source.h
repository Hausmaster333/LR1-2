#ifndef CONTENT_SOURCE_H
#define CONTENT_SOURCE_H

#include <cstddef>
#include <string>

class IContentSource {
    public:
        virtual long long get_size() const = 0;
        virtual std::string read_beginning(std::size_t max_bytes) const = 0;
        virtual std::string get_location() const = 0;
        virtual ~IContentSource() = default;
};

class RealFileSource : public IContentSource {
    private:
        std::string real_path;
    public:
        explicit RealFileSource(const std::string& real_path);

        long long get_size() const override;
        std::string read_beginning(std::size_t max_bytes) const override;
        std::string get_location() const override;
};

class GeneratedSource : public IContentSource {
    public:
        long long get_size() const override { return 0; }
        std::string read_beginning(std::size_t) const override { return ""; }
        std::string get_location() const override { return "Generated without data"; }
};

#endif
