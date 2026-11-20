#ifndef ARRAY_H
#define ARRAY_H

#include "core/ienumerator.h"
#include "ptr/unq_ptr.h"

template <class T>
class DynamicArray {
    private:
        UnqPtr<T[]> data; // Сам хранит длину и освобождает память
    public:
        DynamicArray();
        DynamicArray(int size);
        DynamicArray(const T* items, int count);
        DynamicArray(const DynamicArray<T>& other);

        DynamicArray<T>& operator=(const DynamicArray<T>& other);

        const T& get(int index) const;

        int get_size() const;
        T* raw_data() { return data.get(); }
        const T* raw_data() const { return data.get(); }

        void set(int index, const T& value);
        void set(int index, T&& value); // Забирает значение без копии: для ShrdPtr без атомарных ++/--

        void resize(int newSize);

        // Читает через get(): адрес данных не хранит, поэтому переживает resize() и не выходит
        // за границы. Как и итераторы STL, не должен пережить сам массив.
        class Enumerator : public IEnumerator<T> {
            private:
                const DynamicArray<T>& array;
                int index;
            public:
                explicit Enumerator(const DynamicArray<T>& array) : array(array), index(-1) {}

                bool move_next() override;
                const T& get_current() const override;
                void reset() override;
        };

        UnqPtr<IEnumerator<T>> get_enumerator() const;
};

#include "dynamic_array.tpp"

#endif