#ifndef SEARCH_CRITERIA_H
#define SEARCH_CRITERIA_H

#include "vfs/file_index.h"

inline std::optional<std::string> name_of(const File& file) {
    return file.get_name();
}

inline std::string normalize_extension(std::string extension) {
    if (!extension.empty() && extension[0] == '.') extension.erase(0, 1);
    for (char& symbol : extension) {
        if (symbol >= 'A' && symbol <= 'Z') symbol = static_cast<char>(symbol - 'A' + 'a');
    }
    return extension;
}

inline std::optional<std::string> file_extension(const File& file) {
    if (file.kind() != FileKind::Regular) return std::nullopt;
    const auto& name = file.get_name();
    auto dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0) return "";
    return normalize_extension(name.substr(dot + 1));
}

inline constexpr SearchCriterion by_name{name_of};
inline constexpr SearchCriterion by_extension{file_extension, normalize_extension};

#endif
