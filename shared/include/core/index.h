#ifndef DATA_INDEX_H
#define DATA_INDEX_H

#include "core/array_sequence.h"
#include "tree/tree_dictionary.h"
#include <optional>
#include <stdexcept>

template <class T, class Key>
struct SearchCriterion {
    std::optional<Key> (*const extract)(const T&);

    constexpr explicit SearchCriterion(std::optional<Key> (*function)(const T&)) : extract(function) {
        if (!function) throw std::invalid_argument("Missing index key extractor");
    }
};

template <class Key>
class Index {
    TreeDictionary<Key, ArraySequence<std::size_t>> groups;
public:
    template <class T>
    Index(const T* items, std::size_t size, SearchCriterion<T, Key> criterion) {
        if (size && !items) throw std::invalid_argument("Invalid index input");
        for (std::size_t i = 0; i < size; ++i) {
            auto key = criterion.extract(items[i]);
            if (!key) continue;
            if (!groups.contains_key(*key)) groups.add(*key, {});
            groups.get(*key).push_back(i);
        }
    }

    DynamicArray<std::size_t> find(const Key& key) const {
        if (!groups.contains_key(key)) return {};
        const auto& positions = groups.get(key);
        return DynamicArray<std::size_t>(positions.data(), positions.size());
    }
};

template <class T, class Key>
Index<Key> build_index(const ArraySequence<T>& data, SearchCriterion<T, Key> criterion) {
    return {data.data(), data.size(), criterion};
}

template <class T, class Key>
Index<Key> build_index(const DynamicArray<T>& data, SearchCriterion<T, Key> criterion) {
    return {data.raw_data(), data.get_size(), criterion};
}

#endif
