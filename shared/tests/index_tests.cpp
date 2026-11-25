#include "core/index.h"
#include <gtest/gtest.h>
#include <string>
#include <type_traits>

namespace {
struct Parent {
    std::string name;
    int family_id;
};

std::optional<int> family_id(const Parent& parent) {
    if (parent.family_id < 0) return std::nullopt;
    return parent.family_id;
}

constexpr SearchCriterion<Parent, int> by_family{family_id};
}

TEST(IndexTest, ReturnsPositionsInInputOrderIncludingDuplicates) {
    ArraySequence<Parent> parents{{"Anna", 42}, {"Ivan", 42}, {"Anna", 42}, {"Olga", 17}, {"Unknown", -1}};
    auto index = build_index(parents, by_family);
    const auto positions = index.find(42);
    static_assert(std::is_same_v<decltype(positions), const DynamicArray<std::size_t>>);
    ASSERT_EQ(positions.get_size(), 3u);
    EXPECT_EQ(positions.get(0), 0u);
    EXPECT_EQ(positions.get(1), 1u);
    EXPECT_EQ(positions.get(2), 2u);
    EXPECT_EQ(index.find(17).get(0), 3u);
    EXPECT_EQ(index.find(-1).get_size(), 0u);
    EXPECT_EQ(index.find(999).get_size(), 0u);
}

TEST(IndexTest, BuildsFromDynamicArrayAndChecksEmptyCriterion) {
    DynamicArray<Parent> parents(1);
    parents.set(0, {"Anna", 42});
    EXPECT_EQ(build_index(parents, by_family).find(42).get_size(), 1u);
    DynamicArray<Parent> empty;
    EXPECT_EQ(build_index(empty, by_family).find(42).get_size(), 0u);
    using FamilyCriterion = SearchCriterion<Parent, int>;
    static_assert(!std::is_default_constructible_v<FamilyCriterion>);
    EXPECT_THROW((FamilyCriterion{nullptr}), std::invalid_argument);
}

TEST(IndexTest, DoesNotCopySourceElements) {
    DynamicArray<UnqPtr<int>> data(2);
    data.set(0, UnqPtr<int>::make(7));
    data.set(1, UnqPtr<int>::make(7));
    SearchCriterion<UnqPtr<int>, int> value{[](const UnqPtr<int>& item) -> std::optional<int> {
        return *item;
    }};
    const auto positions = build_index(data, value).find(7);
    ASSERT_EQ(positions.get_size(), 2u);
    EXPECT_EQ(positions.get(1), 1u);
    EXPECT_EQ(*data.get(1), 7);
}
