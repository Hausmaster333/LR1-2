#ifndef PERSON_TYPES_H
#define PERSON_TYPES_H

#include "types/person.h"
#include <string>
#include <utility>

class Student : public Person {
    private:
        int grade_book_num;
        std::string group_id;
    public:
        Student() : Person(), grade_book_num(0) {}
        Student(Person_ID id, const char* first_name, const char* middle_name,
                const char* last_name, int grade_book_num, const char* group_id)
            : Person(id, first_name, middle_name, last_name),
              grade_book_num(grade_book_num),
              group_id(string_or_empty(group_id)) {}
        Student(const Student& other) = default;
        Student(Student&& other) noexcept = default;
        Student& operator=(Student&& other) noexcept = default;

        Student& operator=(const Student& other) {
            Student copy(other);
            return *this = std::move(copy);
        }

        int get_grade_book_num() const { return grade_book_num; }
        const char* get_group_id() const { return group_id.c_str(); }
};

class Teacher : public Person {
    private:
        int depart_num;
        std::string position;
    public:
        Teacher() : Person(), depart_num(0) {}
        Teacher(Person_ID id, const char* first_name, const char* middle_name,
                const char* last_name, int depart_num, const char* position)
            : Person(id, first_name, middle_name, last_name),
              depart_num(depart_num),
              position(string_or_empty(position)) {}
        Teacher(const Teacher& other) = default;
        Teacher(Teacher&& other) noexcept = default;
        Teacher& operator=(Teacher&& other) noexcept = default;

        Teacher& operator=(const Teacher& other) {
            Teacher copy(other);
            return *this = std::move(copy);
        }

        int get_depart_num() const { return depart_num; }
        const char* get_position() const { return position.c_str(); }
};

#endif
