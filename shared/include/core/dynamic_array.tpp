#include "core/dynamic_array.h"
#include <stdexcept>
#include <type_traits>
#include <utility>

template <class T>
DynamicArray<T>::DynamicArray() {}

template <class T>
DynamicArray<T>::DynamicArray(std::size_t size) {
    data = UnqPtr<T[]>::make_array(size);
}

template <class T>
DynamicArray<T>::DynamicArray(const T* items, std::size_t count) : DynamicArray(count) {
    for (std::size_t idx = 0; idx < count; idx++) {
        data[idx] = items[idx];
    }
}

template <class T>
DynamicArray<T>::DynamicArray(const DynamicArray<T>& other) : data(other.data.clone()) {}

template <class T>
DynamicArray<T>& DynamicArray<T>::operator=(const DynamicArray<T>& other) {
    data = other.data.clone();
    return *this;
}

template <class T>
const T& DynamicArray<T>::get(std::size_t index) const {
    if (index >= get_size()) throw std::out_of_range("Index out of range");

    return data[index];
}

template <class T>
std::size_t DynamicArray<T>::get_size() const {
    return data.size();
}

template <class T>
void DynamicArray<T>::set(std::size_t index, const T& value) {
    if (index >= get_size()) throw std::out_of_range("Index out of range");

    data[index] = value;
}

template <class T>
void DynamicArray<T>::set(std::size_t index, T&& value) {
    if (index >= get_size()) throw std::out_of_range("Index out of range");

    data[index] = std::move(value);
}

template <class T>
void DynamicArray<T>::resize(std::size_t new_size) {
    UnqPtr<T[]> new_data = UnqPtr<T[]>::make_array(new_size);

    std::size_t main_size = (new_size < get_size()) ? new_size : get_size();

    for (std::size_t idx = 0; idx < main_size; idx++) {
        if constexpr (std::is_nothrow_move_assignable<T>::value) {
            new_data[idx] = std::move(data[idx]);
        } else {
            new_data[idx] = data[idx];
        }
    }

    data = std::move(new_data);
}

template <class T>
bool DynamicArray<T>::Enumerator::move_next() {
    valid = next < array.get_size();
    if (valid) ++next;
    return valid;
}

template <class T>
const T& DynamicArray<T>::Enumerator::get_current() const {
    if (!valid) throw std::out_of_range("No current element");
    return array.get(next - 1);
}

template <class T>
void DynamicArray<T>::Enumerator::reset() {
    next = 0;
    valid = false;
}

template <class T>
UnqPtr<IEnumerator<T>> DynamicArray<T>::get_enumerator() const {
    return UnqPtr<Enumerator>::make(*this);
}
