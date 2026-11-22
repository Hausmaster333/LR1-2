#ifndef CONTENT_SOURCE_H
#define CONTENT_SOURCE_H

#include <cstddef>
#include <string>
#include <stdexcept>

class IContentSource {
    public:
        virtual long long get_size() const = 0;
        virtual std::string read_beginning(std::size_t max_bytes) const = 0;
        virtual std::string read_all() const = 0;
        virtual bool editable() const { return false; }
        virtual void save(const std::string&) { throw std::invalid_argument("Файл доступен только для чтения"); }
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
        std::string read_all() const override;
        std::string get_location() const override;
};

class TextSource : public IContentSource {
    private:
        std::string text;
    public:
        long long get_size() const override { return text.size(); }
        std::string read_beginning(std::size_t limit) const override { return text.substr(0, limit); }
        std::string read_all() const override { return text; }
        std::string get_location() const override { return {}; }
        bool editable() const override { return true; }
        void save(const std::string& data) override { text = data; }
};

class GeneratedSource : public IContentSource {
    public:
        std::string read_all() const override { return {}; }
        long long get_size() const override { return 0; }
        std::string read_beginning(std::size_t) const override { return ""; }
        std::string get_location() const override { return "Generated without data"; }
};

#endif
