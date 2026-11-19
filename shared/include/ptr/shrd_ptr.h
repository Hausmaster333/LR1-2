#ifndef SHRD_PTR_H
#define SHRD_PTR_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "unq_ptr.h"

// Общая часть блока: счётчики владельцев и слабых ссылок. Объект удаляет последний владелец
// через destroy_object() так, как объект был создан, поэтому ShrdPtr<Base> не зависит от
// деструктора Base. Сам блок живёт, пока жив объект или хоть один WeakPtr: только по блоку
// WeakPtr узнаёт, что объекта уже нет.
// Счётчики атомарные: копии одного объекта можно создавать и отпускать из разных потоков.
// 32-битные счётчики, как в libstdc++: оба помещаются рядом с vptr, и блок не больше, чем у std.
struct ControlBlock {
    std::atomic<std::uint32_t> references{1};
    std::atomic<std::uint32_t> weak_references{1}; // WeakPtr плюс одна общая доля всех владельцев

    ControlBlock() = default;
    ControlBlock(const ControlBlock&) = delete;
    ControlBlock& operator=(const ControlBlock&) = delete;
    virtual ~ControlBlock() = default;

    virtual void destroy_object() noexcept = 0;
};

// Объект лежит прямо в блоке: make() делает одно выделение памяти. Поле - сырая память,
// а не U: объект уничтожается раньше блока, если на блок ещё смотрят WeakPtr.
template <class U>
struct InplaceBlock final : ControlBlock {
    alignas(U) unsigned char storage[sizeof(U)];

    template <class... Args>
    explicit InplaceBlock(Args&&... args) {
        ::new (static_cast<void*>(storage)) U(std::forward<Args>(args)...);
    }

    U* object() noexcept { return std::launder(reinterpret_cast<U*>(storage)); }
    void destroy_object() noexcept override { object()->~U(); }
};

// Блок для объекта, созданного раньше (передача из UnqPtr)
template <class U>
struct PointerBlock final : ControlBlock {
    U* object;

    explicit PointerBlock(U* object) noexcept : object(object) {}
    void destroy_object() noexcept override { delete object; }
};

// Блок для массива, созданного через new[]
template <class U>
struct ArrayBlock final : ControlBlock {
    U* data;

    explicit ArrayBlock(U* data) noexcept : data(data) {}
    void destroy_object() noexcept override { delete[] data; }
};

template <class T>
class ShrdPtr {
    static_assert(!std::is_array<T>::value, "Use ShrdPtr<T[]> for arrays");

    template <class U> friend class ShrdPtr;
    template <class U> friend class WeakPtr;
    template <class To, class From>
    friend ShrdPtr<To> static_pointer_cast(const ShrdPtr<From>& source) noexcept;
    private:
        ControlBlock* block;
        T* ptr;

        // Принимает уже учтённую долю владения: новый блок или успешный WeakPtr::lock()
        ShrdPtr(ControlBlock* block, T* ptr) noexcept;
    public:
        // Создаёт пустой указатель
        ShrdPtr() noexcept;
        // Добавляет владельца того же объекта
        ShrdPtr(const ShrdPtr<T>& other) noexcept;
        // Передаёт долю владения и опустошает источник, счётчик не меняется
        ShrdPtr(ShrdPtr<T>&& other) noexcept;

        // Добавляет владельца с преобразованием Derived -> Base
        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr(const ShrdPtr<U>& other) noexcept;

        // Передаёт долю владения с преобразованием Derived -> Base
        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr(ShrdPtr<U>&& other) noexcept;

        // Забирает объект у UnqPtr; если блок не выделился, UnqPtr остаётся владельцем
        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr(UnqPtr<U>&& other);

        // Разделяет владение с источником, прежний объект отпускается
        ShrdPtr<T>& operator=(const ShrdPtr<T>& other) noexcept;
        // Забирает долю владения источника, прежний объект отпускается
        ShrdPtr<T>& operator=(ShrdPtr<T>&& other) noexcept;

        // Копирующее присваивание с преобразованием типа
        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr<T>& operator=(const ShrdPtr<U>& other) noexcept;

        // Перемещающее присваивание с преобразованием типа
        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr<T>& operator=(ShrdPtr<U>&& other) noexcept;

        // Создаёт объект вместе с блоком одним выделением памяти
        template <class... Args>
        static ShrdPtr<T> make(Args&&... args);

        // Возвращает адрес без передачи владения
        T* get() const noexcept;
        // Возвращает объект, для пустого указателя бросает исключение
        T& operator*() const;
        // Даёт доступ к членам, для пустого указателя бросает исключение
        T* operator->() const;
        // Проверяет наличие объекта
        explicit operator bool() const noexcept;
        // Возвращает число владельцев объекта; у пустого указателя 0
        size_t use_count() const noexcept;
        // Проверяет, что оба указывают на один объект; пустые равны между собой
        template <class U>
        bool operator==(const ShrdPtr<U>& other) const noexcept;
        template <class U>
        bool operator!=(const ShrdPtr<U>& other) const noexcept;

        // Отпускает долю владения, последний владелец удаляет объект
        void reset() noexcept;
        // Обменивает доли владения двух указателей
        void swap(ShrdPtr<T>& other) noexcept;
        // Отпускает долю владения
        ~ShrdPtr();
};

// Новый владелец того же объекта с типом To, как static_cast: ShrdPtr<Base> -> ShrdPtr<Derived>,
// когда известно, что внутри Derived. Проверки типа нет, как и у static_cast.
template <class To, class From>
ShrdPtr<To> static_pointer_cast(const ShrdPtr<From>& source) noexcept;

template <class T>
class ShrdPtr<T[]> {
    private:
        ControlBlock* block;
        T* data;
        size_t length;

        // Принимает новый блок со счётчиком 1
        ShrdPtr(ControlBlock* block, T* data, size_t length) noexcept;
    public:
        // Создаёт пустой указатель на массив
        ShrdPtr() noexcept;
        // Добавляет владельца того же массива
        ShrdPtr(const ShrdPtr<T[]>& other) noexcept;
        // Передаёт долю владения массивом и опустошает источник
        ShrdPtr(ShrdPtr<T[]>&& other) noexcept;
        // Забирает массив у UnqPtr; если блок не выделился, UnqPtr остаётся владельцем
        ShrdPtr(UnqPtr<T[]>&& other);
        // Разделяет владение массивом с источником
        ShrdPtr<T[]>& operator=(const ShrdPtr<T[]>& other) noexcept;
        // Забирает долю владения массивом у источника
        ShrdPtr<T[]>& operator=(ShrdPtr<T[]>&& other) noexcept;

        // Создаёт массив указанной длины; для 0 возвращает пустой указатель
        static ShrdPtr<T[]> make_array(size_t count);

        // Возвращает адрес массива без передачи владения
        T* get() const noexcept;
        // Возвращает элемент, при выходе за границы бросает исключение
        T& operator[](size_t index) const;
        // Проверяет наличие массива
        explicit operator bool() const noexcept;
        // Возвращает длину массива
        size_t size() const noexcept;
        // Возвращает число владельцев массива
        size_t use_count() const noexcept;

        // Отпускает долю владения, последний владелец удаляет массив
        void reset() noexcept;
        // Обменивает доли владения массивами
        void swap(ShrdPtr<T[]>& other) noexcept;
        // Отпускает долю владения
        ~ShrdPtr();
};

#include "shrd_ptr.tpp"

#endif
