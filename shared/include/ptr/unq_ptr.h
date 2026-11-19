#ifndef UNQ_PTR_H
#define UNQ_PTR_H

#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>

// Владеть From через To* можно, если delete To* вызовет деструктор настоящего типа From
template <class From, class To>
inline constexpr bool is_owner_convertible =
    std::is_convertible<From*, To*>::value &&
    (std::is_same<std::remove_cv_t<From>, std::remove_cv_t<To>>::value ||
     std::has_virtual_destructor<To>::value);

// Копия через конструктор T не срежет объект, только если за T* не может прятаться наследник
template <class T>
inline constexpr bool is_cloneable =
    std::is_copy_constructible<std::remove_cv_t<T>>::value &&
    (!std::is_polymorphic<T>::value || std::is_final<T>::value);

template <class T>
class UnqPtr {
    static_assert(!std::is_array<T>::value, "Use UnqPtr<T[]> for arrays");

    template <class U> friend class UnqPtr;
    template <class U> friend class ShrdPtr;
    private:
        T* ptr;

        // Принимает только что созданный объект; доступен лишь make()
        explicit UnqPtr(T* ptr) noexcept;
        // Отдаёт объект без удаления и опустошает указатель
        T* release() noexcept;
    public:
        // Создаёт пустой указатель
        UnqPtr() noexcept;
        // Запрещает копирование владельца: независимая копия только через clone()
        UnqPtr(const UnqPtr<T>& other) = delete;
        // Запрещает копирующее присваивание
        UnqPtr<T>& operator=(const UnqPtr<T>& other) = delete;
        // Передаёт владение и опустошает источник
        UnqPtr(UnqPtr<T>&& other) noexcept;
        // Забирает объект у источника, затем удаляет прежний свой
        UnqPtr<T>& operator=(UnqPtr<T>&& other) noexcept;

        // Передаёт владение Derived -> Base; требует виртуального деструктора у Base
        template <class U, class = std::enable_if_t<is_owner_convertible<U, T>>>
        UnqPtr(UnqPtr<U>&& other) noexcept;

        // Присваивает с преобразованием Derived -> Base
        template <class U, class = std::enable_if_t<is_owner_convertible<U, T>>>
        UnqPtr<T>& operator=(UnqPtr<U>&& other) noexcept;

        // Создаёт объект с переданными аргументами
        template <class... Args>
        static UnqPtr<T> make(Args&&... args);

        // Создаёт независимую копию объекта; для полиморфных типов недоступен (срезка)
        template <class U = T, class = std::enable_if_t<is_cloneable<U>>>
        UnqPtr<T> clone() const;

        // Возвращает адрес без передачи владения
        T* get() const noexcept;
        // Возвращает объект, для пустого указателя бросает исключение
        T& operator*() const;
        // Даёт доступ к членам, для пустого указателя бросает исключение
        T* operator->() const;
        // Проверяет наличие объекта
        explicit operator bool() const noexcept;

        // Удаляет объект и очищает указатель
        void reset() noexcept;
        // Обменивает владение двух указателей
        void swap(UnqPtr<T>& other) noexcept;
        // Освобождает принадлежащий объект
        ~UnqPtr();
};

template <class T>
class UnqPtr<T[]> {
    template <class U> friend class ShrdPtr;
    private:
        T* data;
        size_t length;

        // Принимает только что созданный массив и его длину
        UnqPtr(T* data, size_t length) noexcept;
        // Отдаёт массив без удаления и опустошает указатель
        T* release() noexcept;
    public:
        // Создаёт пустой указатель на массив
        UnqPtr() noexcept;
        // Запрещает копирование владельца массива
        UnqPtr(const UnqPtr<T[]>& other) = delete;
        // Запрещает копирующее присваивание
        UnqPtr<T[]>& operator=(const UnqPtr<T[]>& other) = delete;
        // Передаёт массив и опустошает источник
        UnqPtr(UnqPtr<T[]>&& other) noexcept;
        // Забирает массив у источника, затем удаляет прежний свой
        UnqPtr<T[]>& operator=(UnqPtr<T[]>&& other) noexcept;

        // Создаёт массив указанной длины; для 0 возвращает пустой указатель
        static UnqPtr<T[]> make_array(size_t count);
        // Копирует массив поэлементно
        template <class U = T, class = std::enable_if_t<std::is_copy_assignable<U>::value>>
        UnqPtr<T[]> clone() const;

        // Возвращает адрес массива без передачи владения
        T* get() const noexcept;
        // Возвращает элемент, при выходе за границы бросает исключение
        T& operator[](size_t index) const;
        // Проверяет наличие массива
        explicit operator bool() const noexcept;
        // Возвращает длину массива
        size_t size() const noexcept;

        // Удаляет массив и очищает указатель
        void reset() noexcept;
        // Обменивает массивы и их длины
        void swap(UnqPtr<T[]>& other) noexcept;
        // Освобождает принадлежащий массив
        ~UnqPtr();
};

#include "unq_ptr.tpp"

#endif
