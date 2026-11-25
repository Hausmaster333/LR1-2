#ifndef ARRAY_H
#define ARRAY_H

#include <cstddef>
#include "core/ienumerator.h"
#include "ptr/unq_ptr.h"

template <class T>
class DynamicArray {
    private:
        UnqPtr<T[]> data;
    public:
        DynamicArray();
        DynamicArray(std::size_t size);
        DynamicArray(const T* items, std::size_t count);
        DynamicArray(const DynamicArray<T>& other);
        DynamicArray(DynamicArray&&) noexcept = default;

        DynamicArray<T>& operator=(const DynamicArray<T>& other);
        DynamicArray& operator=(DynamicArray&&) noexcept = default;

        const T& get(std::size_t index) const;

        std::size_t get_size() const;
        T* raw_data() { return data.get(); }
        const T* raw_data() const { return data.get(); }

        void set(std::size_t index, const T& value);
        void set(std::size_t index, T&& value);

        void resize(std::size_t new_size);

        class Enumerator : public IEnumerator<T> {
            private:
                const DynamicArray<T>& array;
                std::size_t next = 0;
                bool valid = false;
            public:
                explicit Enumerator(const DynamicArray<T>& array) : array(array) {}

                bool move_next() override;
                const T& get_current() const override;
                void reset() override;
        };

        UnqPtr<IEnumerator<T>> get_enumerator() const;
};

#include "dynamic_array.tpp"

#endif
