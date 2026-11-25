#include "tree/bst.h"
#include "types/value_codec.h"
#include <gtest/gtest.h>
#include <string>

TEST(Codec, Int) {
    EXPECT_EQ(ValueCodec<int>::to_string(-42), "-42");
    EXPECT_EQ(ValueCodec<int>::from_string("-42"), -42);
    EXPECT_THROW(ValueCodec<int>::from_string("4x"), std::invalid_argument);
    EXPECT_THROW(ValueCodec<int>::from_string(""), std::invalid_argument);
    EXPECT_THROW(ValueCodec<int>::from_string("99999999999"), std::invalid_argument); // Не влезает в int
}

TEST(Codec, Double) {
    EXPECT_EQ(ValueCodec<double>::to_string(0.1), "0.1");
    EXPECT_EQ(ValueCodec<double>::from_string("0.1"), 0.1);
    EXPECT_EQ(ValueCodec<double>::from_string("-1e-3"), -0.001);
}

TEST(Codec, ComplexRoundTrip) {
    for (Complex value : {Complex(1.5, -2), Complex(0, 0), Complex(-3, 4), Complex(1e5, 2)}) {
        std::string text = ValueCodec<Complex>::to_string(value);
        EXPECT_EQ(ValueCodec<Complex>::from_string(text), value) << text;
    }
    EXPECT_EQ(ValueCodec<Complex>::to_string(Complex(1.5, -2)), "1.5-2i");
    EXPECT_EQ(ValueCodec<Complex>::from_string("1e+5+2i"), Complex(1e5, 2)); // '+' в экспоненте - не знак мнимой части
}

TEST(Codec, ComplexMalformed) {
    for (const char* text : {"", "1", "1+2", "1+2j", "1+2i ", "+i", "1++2i", "i"}) {
        EXPECT_THROW(ValueCodec<Complex>::from_string(text), std::invalid_argument) << text;
    }
}

TEST(Codec, StudentRoundTrip) {
    Student student({4510, 123456}, "Ivan", "Ivanovich", "Ivanov", 777, "B24-504");
    Student restored = ValueCodec<Student>::from_string(ValueCodec<Student>::to_string(student));
    EXPECT_EQ(restored.get_id(), student.get_id());
    EXPECT_STREQ(restored.get_last_name(), "Ivanov");
    EXPECT_EQ(restored.get_grade_book_num(), 777);
    EXPECT_STREQ(restored.get_group_id(), "B24-504");
}

TEST(Codec, TeacherRoundTrip) {
    Teacher teacher({1, 2}, "Anna", "Petrovna", "Smirnova", 22, "docent");
    Teacher restored = ValueCodec<Teacher>::from_string(ValueCodec<Teacher>::to_string(teacher));
    EXPECT_EQ(restored.get_depart_num(), 22);
    EXPECT_STREQ(restored.get_position(), "docent");
}

TEST(Codec, PersonWrongFieldCount) {
    EXPECT_THROW(ValueCodec<Student>::from_string("1 2 A B C"), std::invalid_argument);           // Мало полей
    EXPECT_THROW(ValueCodec<Teacher>::from_string("1 2 A B C 3 x extra"), std::invalid_argument); // Лишнее поле
}

TEST(Types, ComplexTotalOrder) {
    Complex one(1, 0);
    Complex unit_imag(0, 1);
    EXPECT_TRUE(one != unit_imag);
    EXPECT_TRUE(unit_imag < one || one < unit_imag);
    EXPECT_TRUE(Complex(1, 1) < Complex(0, 2));
}

TEST(Types, PersonCopyAndFullName) {
    Person person({1, 2}, "Ivan", "Ivanovich", "Ivanov");
    Person copy = person;
    EXPECT_EQ(copy.get_full_name(), "Ivan Ivanovich Ivanov");
}

TEST(Types, EmptyPersonCopy) {
    Person empty;
    Person empty_copy = empty;
    EXPECT_EQ(empty_copy.get_full_name(), "");

    Person person({1, 2}, "Ivan", "Ivanovich", "Ivanov");
    empty_copy = person;
    EXPECT_STREQ(empty_copy.get_first_name(), "Ivan");
}

TEST(TypedTrees, DoubleRoundTrip) {
    BinarySearchTree<double> doubles;
    for (double value : {0.5, -1.25, 3.0}) doubles.insert(value);
    EXPECT_EQ(BinarySearchTree<double>::from_string(doubles.to_string()), doubles);
}

TEST(TypedTrees, ComplexOrderedByModulus) {
    BinarySearchTree<Complex> complexes;
    for (Complex value : {Complex(1, 0), Complex(0, 1), Complex(3, 4), Complex(-1, -1)}) {
        complexes.insert(value);
    }
    EXPECT_EQ(complexes.get_count(), 4);
    EXPECT_EQ(complexes.get(3), Complex(3, 4)); // Модуль 5 - самый большой
    EXPECT_EQ(BinarySearchTree<Complex>::from_string(complexes.to_string()), complexes);
}

TEST(TypedTrees, StringsInAlphabeticalOrder) {
    BinarySearchTree<std::string> words;
    for (const char* word : {"pear", "apple", "fig"}) words.insert(word);
    EXPECT_EQ(words.get(0), "apple");
}

TEST(TypedTrees, StudentsKeyedById) {
    BinarySearchTree<Student> students;
    students.insert(Student({2, 1}, "B", "B", "B", 2, "G2"));
    students.insert(Student({1, 5}, "A", "A", "A", 1, "G1"));
    EXPECT_FALSE(students.insert(Student({1, 5}, "Dup", "D", "D", 3, "G3")));

    std::optional<Student> found = students.try_find(Student({1, 5}, "", "", "", 0, ""));
    ASSERT_TRUE(found.has_value());
    EXPECT_STREQ(found.value().get_first_name(), "A");
}

TEST(TypedTrees, StudentsRoundTripAndMap) {
    BinarySearchTree<Student> students;
    students.insert(Student({2, 1}, "B", "B", "B", 2, "G2"));
    students.insert(Student({1, 5}, "A", "A", "A", 1, "G1"));

    BinarySearchTree<Student> restored =
        BinarySearchTree<Student>::from_string(students.to_string());
    EXPECT_STREQ(restored.get(1).get_group_id(), "G2");

    BinarySearchTree<int> grades =
        students.map([](const Student& student) { return student.get_grade_book_num(); });
    EXPECT_EQ(grades.get_count(), 2);
}
