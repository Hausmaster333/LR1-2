#ifndef WEAK_PTR_H
#define WEAK_PTR_H

#include <cstddef>
#include <type_traits>
#include "shrd_ptr.h"

// Невладеющая ссылка на объект ShrdPtr: не продлевает жизнь объекта, но знает, жив ли он.
// Доступ к объекту только через lock(), пока полученный ShrdPtr держит объект живым.
// Нужен там, где ShrdPtr образовал бы цикл или ссылка не должна владеть (обратные связи).
template <class T>
class WeakPtr {
    static_assert(!std::is_array<T>::value, "WeakPtr<T[]> is not supported");

    private:
        ControlBlock* block;
        T* ptr;
    public:
        // Создаёт пустую ссылку
        WeakPtr() noexcept;
        // Ссылается на тот же объект
        WeakPtr(const WeakPtr<T>& other) noexcept;
        // Забирает ссылку и опустошает источник
        WeakPtr(WeakPtr<T>&& other) noexcept;

        // Ссылается на объект владельца, с преобразованием Derived -> Base
        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        WeakPtr(const ShrdPtr<U>& owner) noexcept;

        // Ссылается на объект источника, прежняя ссылка отпускается
        WeakPtr<T>& operator=(const WeakPtr<T>& other) noexcept;
        // Забирает ссылку источника, прежняя ссылка отпускается
        WeakPtr<T>& operator=(WeakPtr<T>&& other) noexcept;

        // Ссылается на объект владельца
        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        WeakPtr<T>& operator=(const ShrdPtr<U>& owner) noexcept;

        // Новый владелец объекта; если объект уже удалён - пустой ShrdPtr
        ShrdPtr<T> lock() const noexcept;
        // Проверяет, что объект удалён (или ссылка пуста)
        bool expired() const noexcept;
        // Возвращает число владельцев объекта; у удалённого объекта 0
        size_t use_count() const noexcept;

        // Отпускает ссылку; последняя ссылка на блок удаляет блок
        void reset() noexcept;
        // Обменивает ссылки
        void swap(WeakPtr<T>& other) noexcept;
        // Отпускает ссылку
        ~WeakPtr();
};

#include "weak_ptr.tpp"

#endif
