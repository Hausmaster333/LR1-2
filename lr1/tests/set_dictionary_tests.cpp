#include "tree/tree_dictionary.h"
#include "tree/tree_set.h"
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "core/array_sequence.h"

struct NoDefault {
    int value;

    explicit NoDefault(int value) : value(value) {}
};

ArraySequence<int> keys(UnqPtr<IEnumerator<int>> iter) {
    ArraySequence<int> result;
    while (iter->move_next())
        result.push_back(iter->get_current());
    return result;
}

ArraySequence<std::string> keys(UnqPtr<IEnumerator<KeyValue<std::string, int>>> iter) {
    ArraySequence<std::string> result;
    while (iter->move_next())
        result.push_back(iter->get_current().key);
    return result;
}

TEST(TreeSet, AddContainsRemove) {
    TreeSet<int> tree_set;
    ISet<int>& set = tree_set;
    EXPECT_TRUE(set.add(3));
    EXPECT_TRUE(set.add(1));
    EXPECT_FALSE(set.add(3));
    EXPECT_EQ(set.get_count(), 2);
    EXPECT_TRUE(set.contains(1));
    EXPECT_FALSE(set.contains(2));

    EXPECT_TRUE(set.remove(1));
    EXPECT_FALSE(set.remove(1));
    EXPECT_EQ(set.get_count(), 1);
}

TEST(TreeSet, EnumeratesInOrder) {
    TreeSet<int> set;
    for (int key : {5, 1, 3}) set.add(key);
    EXPECT_EQ(keys(set.get_enumerator()), (ArraySequence<int>{1, 3, 5}));
}

TEST(TreeDictionary, AddAndGet) {
    TreeDictionary<std::string, int> tree_dictionary;
    IDictionary<std::string, int>& dictionary = tree_dictionary;
    dictionary.add("b", 2);
    dictionary.add("a", 1);
    EXPECT_EQ(dictionary.get_count(), 2);
    EXPECT_TRUE(dictionary.contains_key("a"));
    EXPECT_FALSE(dictionary.contains_key("c"));
    EXPECT_EQ(dictionary.get("a"), 1);
    EXPECT_EQ(dictionary.get("b"), 2);
}

TEST(TreeDictionary, MissingAndDuplicateKeys) {
    TreeDictionary<std::string, int> dictionary;
    dictionary.add("a", 1);
    EXPECT_THROW(dictionary.add("a", 5), std::invalid_argument);
    EXPECT_EQ(dictionary.get("a"), 1);
    EXPECT_THROW(dictionary.get("c"), std::out_of_range);
    EXPECT_THROW(dictionary.remove("c"), std::out_of_range);
}

TEST(TreeDictionary, ValueChangesInPlace) {
    TreeDictionary<std::string, int> dictionary;
    dictionary.add("a", 1);
    dictionary.add("b", 2);
    dictionary.get("a") = 10;
    const auto& read_only = dictionary;
    auto iter = read_only.get_enumerator();
    static_assert(std::is_same_v<decltype(read_only.get("a")), const int&>);
    static_assert(std::is_same_v<decltype((iter->get_current().value)), const int&>);
    EXPECT_EQ(read_only.get("a"), 10);
    EXPECT_EQ(keys(dictionary.get_enumerator()), (ArraySequence<std::string>{"a", "b"}));
}

TEST(TreeDictionary, RemoveByKey) {
    TreeDictionary<std::string, int> dictionary;
    dictionary.add("a", 1);
    dictionary.add("b", 2);
    dictionary.remove("a");
    EXPECT_FALSE(dictionary.contains_key("a"));
    EXPECT_EQ(dictionary.get("b"), 2);
}

TEST(TreeDictionary, EnumeratesPairsByKey) {
    TreeDictionary<std::string, int> dictionary;
    dictionary.add("c", 3);
    dictionary.add("a", 1);
    dictionary.add("b", 2);
    EXPECT_EQ(keys(dictionary.get_enumerator()), (ArraySequence<std::string>{"a", "b", "c"}));

    auto iter = dictionary.get_enumerator();
    while (iter->move_next()) {
        EXPECT_EQ(iter->get_current().value, iter->get_current().key[0] - 'a' + 1);
    }
}

TEST(TreeDictionary, ValueWithoutDefaultConstructor) {
    TreeDictionary<int, NoDefault> dictionary;
    dictionary.add(2, NoDefault(20));
    dictionary.add(1, NoDefault(10));
    EXPECT_EQ(dictionary.get(1).value, 10);
    dictionary.remove(1);
    EXPECT_FALSE(dictionary.contains_key(1));
    EXPECT_EQ(dictionary.get(2).value, 20);
}

TEST(BstKeyValue, OrdersAndFindsByKey) {
    BinarySearchTree<std::string, int> tree;
    EXPECT_TRUE(tree.insert({"b", 2}));
    EXPECT_TRUE(tree.insert({"a", 1}));
    EXPECT_FALSE(tree.insert({"a", 99}));

    EXPECT_EQ(tree.find("a").value, 1);
    EXPECT_EQ(tree.index_of("b"), 1);
    EXPECT_EQ(tree.get(0).key, "a");
    EXPECT_TRUE(tree.remove("a"));
    EXPECT_EQ(tree.get_count(), 1);
}

TEST(TreeDictionary, MoveOnlyValue) {
    TreeDictionary<std::string, UnqPtr<int>> tree_dictionary;
    IDictionary<std::string, UnqPtr<int>>& dictionary = tree_dictionary;
    dictionary.add("a", UnqPtr<int>::make(1));
    UnqPtr<int> two = UnqPtr<int>::make(2);
    dictionary.add("b", std::move(two));
    *dictionary.get("a") += 10;
    EXPECT_EQ(*dictionary.get("a"), 11);
    EXPECT_EQ(*dictionary.get("b"), 2);
    dictionary.remove("a");
    EXPECT_FALSE(dictionary.contains_key("a"));
}
