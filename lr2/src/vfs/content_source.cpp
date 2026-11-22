#include "vfs/content_source.h"
#include <filesystem>
#include <fstream>
#include <system_error>

RealFileSource::RealFileSource(const std::string& real_path) : real_path(real_path) {}

long long RealFileSource::get_size() const {
    std::error_code error;
    std::uintmax_t size = std::filesystem::file_size(std::filesystem::u8path(real_path), error);
    return error ? -1 : static_cast<long long>(size);
}

std::string RealFileSource::read_beginning(std::size_t max_bytes) const {
    std::ifstream file(std::filesystem::u8path(real_path), std::ios::binary);
    std::string data(max_bytes, '\0');
    file.read(&data[0], static_cast<std::streamsize>(max_bytes));
    data.resize(static_cast<std::size_t>(file.gcount()));
    return data;
}

std::string RealFileSource::read_all() const {
    std::ifstream file(std::filesystem::u8path(real_path), std::ios::binary);
    if (!file) throw std::runtime_error("Не удалось открыть файл: " + real_path);
    std::string data;
    char block[8192];
    do {
        file.read(block, sizeof(block));
        data.append(block, static_cast<std::size_t>(file.gcount()));
    } while (file);
    if (file.bad() || !file.eof()) throw std::runtime_error("Ошибка чтения: " + real_path);
    return data;
}

std::string RealFileSource::get_location() const {
    return real_path;
}
