#ifndef VALUE_CODEC_H
#define VALUE_CODEC_H

#include "core/value_codec.h"
#include "types/complex.h"
#include "types/person_types.h"
#include <charconv>
#include <stdexcept>
#include <string>

namespace codec {
    inline std::invalid_argument malformed(const std::string& text) {
        return std::invalid_argument("Malformed record: '" + text + "'");
    }

    inline std::string next_field(const std::string& text, std::size_t& pos) {
        std::size_t begin = text.find_first_not_of(' ', pos);
        if (begin == std::string::npos) throw malformed(text);
        pos = text.find(' ', begin);
        return text.substr(begin, pos == std::string::npos ? std::string::npos : pos - begin);
    }

    inline void expect_end(const std::string& text, std::size_t pos) {
        if (pos != std::string::npos && text.find_first_not_of(' ', pos) != std::string::npos) {
            throw malformed(text);
        }
    }

    inline Person_ID read_id(const std::string& text, std::size_t& pos) {
        int series = parse_number<int>(next_field(text, pos));
        return Person_ID{series, parse_number<int>(next_field(text, pos))};
    }

    inline std::string person_fields(const Person& person) {
        return format_number(person.get_id().series) + " " + format_number(person.get_id().number)
            + " " + person.get_first_name() + " " + person.get_middle_name()
            + " " + person.get_last_name();
    }
}

template <>
struct ValueCodec<Complex> {
    static std::string to_string(const Complex& value) {
        std::string imag = codec::format_number(value.get_imag());
        if (imag[0] != '-') imag = "+" + imag;
        return codec::format_number(value.get_real()) + imag + "i";
    }

    static Complex from_string(const std::string& text) {
        const char* begin = text.data();
        const char* end = begin + text.size();
        double real = 0.0;
        double imag = 0.0;

        auto [real_end, real_error] = std::from_chars(begin, end, real);
        if (real_error != std::errc() || real_end == end) throw bad_complex(text);

        const char* imag_begin = real_end;
        if (*imag_begin == '+') {
            imag_begin++;
        } else if (*imag_begin != '-') {
            throw bad_complex(text);
        }

        auto [imag_end, imag_error] = std::from_chars(imag_begin, end, imag);
        if (imag_error != std::errc() || imag_end + 1 != end || *imag_end != 'i') {
            throw bad_complex(text);
        }
        return Complex(real, imag);
    }

    private:
        static std::invalid_argument bad_complex(const std::string& text) {
            return std::invalid_argument("Not a complex number (a+bi): '" + text + "'");
        }
};

template <>
struct ValueCodec<Student> {
    static std::string to_string(const Student& value) {
        return codec::person_fields(value) + " "
            + codec::format_number(value.get_grade_book_num()) + " "
            + value.get_group_id();
    }

    static Student from_string(const std::string& text) {
        using namespace codec;
        std::size_t pos = 0;
        Person_ID id = read_id(text, pos);
        std::string first = next_field(text, pos);
        std::string middle = next_field(text, pos);
        std::string last = next_field(text, pos);
        int grade_book = parse_number<int>(next_field(text, pos));
        std::string group = next_field(text, pos);
        expect_end(text, pos);
        return Student(id, first.c_str(), middle.c_str(), last.c_str(), grade_book,
                       group.c_str());
    }
};

template <>
struct ValueCodec<Teacher> {
    static std::string to_string(const Teacher& value) {
        return codec::person_fields(value) + " "
            + codec::format_number(value.get_depart_num()) + " "
            + value.get_position();
    }

    static Teacher from_string(const std::string& text) {
        using namespace codec;
        std::size_t pos = 0;
        Person_ID id = read_id(text, pos);
        std::string first = next_field(text, pos);
        std::string middle = next_field(text, pos);
        std::string last = next_field(text, pos);
        int department = parse_number<int>(next_field(text, pos));
        std::string position = next_field(text, pos);
        expect_end(text, pos);
        return Teacher(id, first.c_str(), middle.c_str(), last.c_str(), department, position.c_str());
    }
};

#endif
