#ifndef PERSON_H
#define PERSON_H

#include <string>
#include <utility>

// Строки хранит std::string, наружу они отдаются как char*, как в задании; nullptr даёт пустую строку
inline std::string string_or_empty(const char* source) {
    return source == nullptr ? std::string() : std::string(source);
}

struct Person_ID {
    int series;
    int number;

    bool operator==(const Person_ID& other) const {
        return series == other.series && number == other.number;
    }
    bool operator!=(const Person_ID& other) const { return !(*this == other); }
    bool operator<(const Person_ID& other) const {
        if (series != other.series) return series < other.series;
        return number < other.number;
    }
    bool operator>(const Person_ID& other) const { return other < *this; }
};

class Person {
    private:
        Person_ID id;
        std::string first_name;
        std::string middle_name;
        std::string last_name;
    public:
        Person() : id{0, 0} {}
        Person(Person_ID id, const char* first_name, const char* middle_name,
               const char* last_name)
            : id(id),
              first_name(string_or_empty(first_name)),
              middle_name(string_or_empty(middle_name)),
              last_name(string_or_empty(last_name)) {}
        Person(const Person& other) = default;
        Person(Person&& other) noexcept = default;
        Person& operator=(Person&& other) noexcept = default;

        // Сначала копия, потом перемещение: если копирование бросит, объект не изменится
        Person& operator=(const Person& other) {
            Person copy(other);
            return *this = std::move(copy);
        }

        virtual ~Person() = default;

        Person_ID get_id() const { return id; }

        const char* get_first_name() const { return first_name.c_str(); }
        const char* get_middle_name() const { return middle_name.c_str(); }
        const char* get_last_name() const { return last_name.c_str(); }
        std::string get_full_name() const { // Вычислимый атрибут
            std::string full_name;
            auto add = [&full_name](const std::string& part) {
                if (part.empty()) return;
                if (!full_name.empty()) full_name += ' ';
                full_name += part;
            };
            add(first_name);
            add(middle_name);
            add(last_name);
            return full_name;
        }

        bool operator==(const Person& other) const { return id == other.id; }
        bool operator!=(const Person& other) const { return !(*this == other); }
        bool operator<(const Person& other) const { return id < other.id; }
        bool operator>(const Person& other) const { return id > other.id; }
};

#endif
