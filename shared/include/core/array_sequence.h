#ifndef ARRAY_SEQUENCE_H
#define ARRAY_SEQUENCE_H

#include "core/dynamic_array.h"
#include <algorithm>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <utility>

template<class T>
class ArraySequence {
private:
    DynamicArray<T> array;
    int count = 0;

public:
    ArraySequence() = default;
    explicit ArraySequence(int size) : array(size), count(size) {}
    ArraySequence(std::initializer_list<T> values) : array(values.begin(), static_cast<int>(values.size())), count(array.get_size()) {}

    std::size_t size() const { return count; }
    bool empty() const { return count == 0; }
    T* data() { return array.raw_data(); }
    const T* data() const { return array.raw_data(); }
    T* begin() { return data(); }
    const T* begin() const { return data(); }
    T* end() { return data() + count; }
    const T* end() const { return data() + count; }

    T& operator[](std::size_t index) { return data()[index]; }
    const T& operator[](std::size_t index) const { return data()[index]; }
    const T& at(std::size_t index) const {
        if (index >= size()) throw std::out_of_range("Sequence index out of range");
        return (*this)[index];
    }
    T& back() { return (*this)[count - 1]; }
    const T& back() const { return (*this)[count - 1]; }

    void reserve(int capacity) {
        if (capacity > array.get_size()) array.resize(capacity);
    }

    void push_back(T value) {
        if (count == array.get_size()) {
            constexpr int limit = std::numeric_limits<int>::max();
            if (count == limit) throw std::length_error("Sequence too large");
            reserve(count == 0 ? 4 : (count > limit / 2 ? limit : count * 2));
        }
        array.set(count, std::move(value));
        ++count;
    }

    void clear() {
        array.resize(0);
        count = 0;
    }

    void remove_first() {
        if (empty()) throw std::out_of_range("Empty sequence");
        std::move(begin() + 1, end(), begin());
        array.set(--count, T{});
    }

    bool operator==(const ArraySequence& other) const {
        return size() == other.size() && std::equal(begin(), end(), other.begin());
    }
};

#endif
