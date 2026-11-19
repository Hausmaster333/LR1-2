#include "core/dynamic_array.h"
#include <stdexcept>
#include <type_traits>
#include <utility>

template <class T>
DynamicArray<T>::DynamicArray() {}

template <class T>
DynamicArray<T>::DynamicArray(int size) {
    if (size < 0) throw std::out_of_range("Size must be >= 0");

    data = UnqPtr<T[]>::make_array(size);
}

template <class T>
DynamicArray<T>::DynamicArray(const T* items, int count) : DynamicArray(count) {
    for (int idx = 0; idx < count; idx++) {
        data[idx] = items[idx];
    }
}

template <class T>
DynamicArray<T>::DynamicArray(const DynamicArray<T>& other) : data(other.data.clone()) {}

template <class T>
DynamicArray<T>& DynamicArray<T>::operator=(const DynamicArray<T>& other) {
    // Если копирование бросит, старые данные останутся нетронутыми
    data = other.data.clone();
    return *this;
}

template <class T>
const T& DynamicArray<T>::get(int index) const {
    if (index >= get_size() || index < 0) throw std::out_of_range("Index out of range");

    return data[index];
}

template <class T>
int DynamicArray<T>::get_size() const {
    return static_cast<int>(data.size());
}

template <class T>
void DynamicArray<T>::set(int index, const T& value) {
    if (index >= get_size() || index < 0) throw std::out_of_range("Index out of range");

    data[index] = value;
}

template <class T>
void DynamicArray<T>::set(int index, T&& value) {
    if (index >= get_size() || index < 0) throw std::out_of_range("Index out of range");

    data[index] = std::move(value);
}

template <class T>
void DynamicArray<T>::resize(int new_size) {
    if (new_size < 0) throw std::out_of_range("Size must be >= 0");

    UnqPtr<T[]> new_data = UnqPtr<T[]>::make_array(new_size);

    int main_size = (new_size < get_size()) ? new_size : get_size();

    for (int idx = 0; idx < main_size; idx++) {
        // Перемещаем, только если это не бросит: иначе исключение посередине оставило бы
        // старый буфер наполовину пустым, а при копировании он остаётся целым
        if constexpr (std::is_nothrow_move_assignable<T>::value) {
            new_data[idx] = std::move(data[idx]);
        } else {
            new_data[idx] = data[idx];
        }
    }

    data = std::move(new_data);
}

// ---------- Enumerator ----------

template <class T>
bool DynamicArray<T>::Enumerator::move_next() {
    if (index < array.get_size()) index++;
    return index < array.get_size();
}

template <class T>
const T& DynamicArray<T>::Enumerator::get_current() const {
    return array.get(index); // До move_next() и после конца - out_of_range
}

template <class T>
void DynamicArray<T>::Enumerator::reset() {
    index = -1;
}

template <class T>
UnqPtr<IEnumerator<T>> DynamicArray<T>::get_enumerator() const {
    return UnqPtr<Enumerator>::make(*this);
}
