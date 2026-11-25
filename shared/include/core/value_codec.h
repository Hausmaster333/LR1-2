#ifndef CORE_VALUE_CODEC_H
#define CORE_VALUE_CODEC_H

#include <charconv>
#include <stdexcept>
#include <string>

template <class T>
struct ValueCodec;

namespace codec {
    template <class N>
    N parse_number(const std::string& text) {
        N value{};
        const char* end = text.data() + text.size();
        auto [ptr, error] = std::from_chars(text.data(), end, value);
        if (text.empty() || error != std::errc() || ptr != end) {
            throw std::invalid_argument("Not a number: '" + text + "'");
        }
        return value;
    }

    template <class N>
    std::string format_number(N value) {
        char buffer[64];
        auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        return std::string(buffer, result.ptr);
    }
}

template <>
struct ValueCodec<int> {
    static std::string to_string(int value) { return codec::format_number(value); }
    static int from_string(const std::string& text) {
        return codec::parse_number<int>(text);
    }
};

template <>
struct ValueCodec<double> {
    static std::string to_string(double value) { return codec::format_number(value); }
    static double from_string(const std::string& text) {
        return codec::parse_number<double>(text);
    }
};

template <>
struct ValueCodec<std::string> {
    static std::string to_string(const std::string& value) { return value; }
    static std::string from_string(const std::string& text) { return text; }
};

#endif
