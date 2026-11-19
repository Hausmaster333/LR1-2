#include "tree/tree_dictionary.h"
#include <gtest/gtest.h>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace {
    std::vector<std::string> keys(UnqPtr<IEnumerator<KeyValue<std::string, int>>> iter) {
        std::vector<std::string> result;
        while (iter->move_next()) result.push_back(iter->get_current().key);
        return result;
    }
}

TEST(TreeDictionary, StartsEmpty) {
    TreeDictionary<std::string, int> dictionary;
    EXPECT_EQ(dictionary.get_count(), 0);
}

TEST(TreeDictionary, AddGetContains) {
    TreeDictionary<std::string, int> dictionary;
    dictionary.add("b", 2);
    dictionary.add("a", 1);
    EXPECT_EQ(dictionary.get_count(), 2);
    EXPECT_TRUE(dictionary.contains_key("a"));
    EXPECT_FALSE(dictionary.contains_key("c"));
    EXPECT_EQ(dictionary.get("a"), 1);
    EXPECT_EQ(dictionary.get("b"), 2);
}

TEST(TreeDictionary, DuplicateKeyIsRejected) {
    TreeDictionary<std::string, int> dictionary;
    dictionary.add("a", 1);
    EXPECT_THROW(dictionary.add("a", 5), std::invalid_argument);
    EXPECT_EQ(dictionary.get("a"), 1); // Старое значение не перезаписано
    EXPECT_EQ(dictionary.get_count(), 1);
}

TEST(TreeDictionary, MissingKeyThrows) {
    TreeDictionary<std::string, int> dictionary;
    const TreeDictionary<std::string, int>& read_only = dictionary;
    EXPECT_THROW(dictionary.get("x"), std::out_of_range);
    EXPECT_THROW(read_only.get("x"), std::out_of_range); // const-версия get
    EXPECT_THROW(dictionary.remove("x"), std::out_of_range);
}

TEST(TreeDictionary, Remove) {
    TreeDictionary<std::string, int> dictionary;
    dictionary.add("a", 1);
    dictionary.add("b", 2);
    dictionary.remove("a");
    EXPECT_FALSE(dictionary.contains_key("a"));
    EXPECT_EQ(dictionary.get("b"), 2);
    EXPECT_EQ(dictionary.get_count(), 1);
}

// Так индекс дописывает файл в список по ключу: get() отдаёт ссылку на хранимое значение
TEST(TreeDictionary, ValueChangesInPlace) {
    TreeDictionary<std::string, std::vector<int>> dictionary;
    dictionary.add("files", {});
    dictionary.get("files").push_back(7);
    dictionary.get("files").push_back(8);
    EXPECT_EQ(dictionary.get("files"), (std::vector<int>{7, 8}));
}

TEST(TreeDictionary, EnumeratesInKeyOrder) {
    TreeDictionary<std::string, int> dictionary;
    for (const char* key : {"pear", "apple", "fig", "banana"}) dictionary.add(key, 0);
    EXPECT_EQ(keys(dictionary.get_enumerator()),
              (std::vector<std::string>{"apple", "banana", "fig", "pear"}));
}

TEST(TreeDictionary, ThreadedEnumeratorInKeyOrder) {
    TreeDictionary<std::string, int> dictionary;
    dictionary.add("b", 0);
    dictionary.add("a", 0);
    EXPECT_EQ(keys(dictionary.get_threaded_enumerator()), (std::vector<std::string>{"a", "b"}));
}

// add сбрасывает прошивку, следующий вызов строит её заново и видит новый ключ
TEST(TreeDictionary, ThreadedEnumeratorSeesNewKeys) {
    TreeDictionary<std::string, int> dictionary;
    dictionary.add("b", 0);
    dictionary.add("a", 0);
    keys(dictionary.get_threaded_enumerator());

    dictionary.add("c", 0);
    EXPECT_EQ(keys(dictionary.get_threaded_enumerator()), (std::vector<std::string>{"a", "b", "c"}));
}

// Случайные add / get / remove повторяем на std::map: итог должен совпасть
TEST(TreeDictionary, RandomOperationsMatchStdMap) {
    std::mt19937 random(7);
    std::uniform_int_distribution<int> keys(0, 500);
    TreeDictionary<int, int> dictionary;
    std::map<int, int> reference;

    for (int step = 0; step < 5000; step++) {
        int key = keys(random);
        bool present = reference.count(key) == 1;
        if (random() % 3 == 0) {
            if (present) {
                dictionary.remove(key);
                reference.erase(key);
            } else {
                EXPECT_THROW(dictionary.remove(key), std::out_of_range);
            }
        } else if (present) {
            dictionary.get(key) += 1;
            reference[key] += 1;
        } else {
            dictionary.add(key, step);
            reference[key] = step;
        }
    }

    ASSERT_EQ(dictionary.get_count(), static_cast<int>(reference.size()));
    for (const auto& [key, value] : reference) EXPECT_EQ(dictionary.get(key), value);
}
