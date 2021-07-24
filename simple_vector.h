#pragma once

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

#include "array_ptr.h"

struct ReserveProxyObj {
    explicit ReserveProxyObj(size_t capacity_to_reserve)
        : capacity_to_reserve(capacity_to_reserve) {}

    size_t capacity_to_reserve = 0;
};

ReserveProxyObj Reserve(size_t capacity_to_reserve) {
    return ReserveProxyObj(capacity_to_reserve);
}

template<typename Type>
class SimpleVector {
public:
    using Iterator = Type*;
    using ConstIterator = const Type*;

    SimpleVector() noexcept = default;

    // Создаёт вектор из size элементов, инициализированных значением по умолчанию
    explicit SimpleVector(size_t size)
        : items_(ArrayPtr<Type>(size)), size_(size), capacity_(size) {
    }

    // Создаёт вектор из size элементов, инициализированных значением value
    SimpleVector(size_t size, const Type& value)
        : SimpleVector(size) {
        std::fill(begin(), end(), value);
    }

    // Создаёт вектор из std::initializer_list
    SimpleVector(std::initializer_list<Type> init)
        : SimpleVector(init.size()) {
        std::copy(init.begin(), init.end(), begin());
    }

    SimpleVector(const SimpleVector& other)
        : SimpleVector(other.GetSize()) {
        std::copy(other.begin(), other.end(), begin());
    }

    SimpleVector(SimpleVector&& other)
        : items_(std::move(other.items_)),
        size_(std::exchange(other.size_, 0)),
        capacity_(std::exchange(other.capacity_, 0)) {
    }

    SimpleVector(ReserveProxyObj reserveProxyObj) {
        Reserve(reserveProxyObj.capacity_to_reserve);
    }

    SimpleVector& operator=(const SimpleVector& rhs) {
        if (this != &rhs) {
            SimpleVector tmp(rhs);
            swap(tmp);
        }

        return *this;
    }

    SimpleVector& operator=(SimpleVector&& rhs) {
        if (this != &rhs) {
            items_ = std::move(rhs.items_);
            size_ = std::exchange(rhs.size_, 0);
            capacity_ = std::exchange(rhs.capacity_, 0);
        }

        return *this;
    }

    // Возвращает количество элементов в массиве
    [[nodiscard]] size_t GetSize() const noexcept {
        return size_;
    }

    // Возвращает вместимость массива
    [[nodiscard]] size_t GetCapacity() const noexcept {
        return capacity_;
    }

    // Сообщает, пустой ли массив
    [[nodiscard]] bool IsEmpty() const noexcept {
        return GetSize() == 0;
    }

    // Добавляет элемент в конец вектора
    // При нехватке места увеличивает вдвое вместимость вектора
    void PushBack(const Type& item) {
        DoubleIfFull();
        (*this)[size_++] = item;
    }

    void PushBack(Type&& item) {
        DoubleIfFull();
        (*this)[size_++] = std::move(item);
    }

    // "Удаляет" последний элемент вектора. Вектор не должен быть пустым
    void PopBack() noexcept {
        if (!IsEmpty()) {
            --size_;
        }
    }

    // Вставляет значение value в позицию pos.
    // Возвращает итератор на вставленное значение
    // Если перед вставкой значения вектор был заполнен полностью,
    // вместимость вектора должна увеличиться вдвое, а для вектора вместимостью 0 стать равной 1
    Iterator Insert(ConstIterator pos, const Type& value) {
        // pos перестанет быть валидным, если произойдет изменение размера,
        // поэтому запомним индекс элемента, на который указывает итератор
        const int index = pos - begin();

        DoubleIfFull();
        std::move_backward(begin() + index, end(), end() + 1);

        (*this)[index] = value;
        ++size_;

        return Iterator{ &items_[index] };
    }

    Iterator Insert(ConstIterator pos, Type&& value) {
        // pos перестанет быть валидным, если произойдет изменение размера,
        // поэтому запомним индекс элемента, на который указывает итератор
        const int index = pos - begin();

        DoubleIfFull();
        std::move_backward(begin() + index, (end()), end() + 1);
        
        (*this)[index] = std::move(value);
        ++size_;

        return Iterator{ &items_[index] };
    }

    // Удаляет элемент вектора в указанной позиции
    Iterator Erase(ConstIterator pos) {
        size_t index = pos - begin();

        Iterator it = begin() + index;

        std::copy(std::make_move_iterator(it + 1), std::make_move_iterator(end()), it);

        --size_;

        if (index == static_cast<size_t>(GetSize())) {
            return end();
        }

        return Iterator{ &items_[index] };
    }

    // Обменивает значение с другим вектором
    void swap(SimpleVector& other) noexcept {
        items_.swap(other.items_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

    // Возвращает ссылку на элемент с индексом index
    Type& operator[](size_t index) noexcept {
        return items_[index];
    }

    // Возвращает константную ссылку на элемент с индексом index
    const Type& operator[](size_t index) const noexcept {
        return (*this)[index];
    }

    // Возвращает ссылку на элемент с индексом index
    // Выбрасывает исключение std::out_of_range, если index >= size
    Type& At(size_t index) {
        using namespace std::string_literals;

        if (index >= GetSize()) {
            throw std::out_of_range("Index out of range"s);
        }

        return (*this)[index];
    }

    // Возвращает константную ссылку на элемент с индексом index
    // Выбрасывает исключение std::out_of_range, если index >= size
    const Type& At(size_t index) const {
        return const_cast<Type&>((*this).At(index));
    }

    // Обнуляет размер массива, не изменяя его вместимость
    void Clear() noexcept {
        size_ = 0;
    }

    // Изменяет размер массива.
    // При увеличении размера новые элементы получают значение по умолчанию для типа Type
    void Resize(size_t new_size) {
        // если новый размер меньше текущего - просто уменьшаем размер
        if (new_size < GetSize()) {
            size_ = new_size;
        }
        else if (new_size > GetSize()) {
            // если новый размер больше вместимости
            if (new_size > GetCapacity()) {
                Reserve(new_size);
            }
            else {
                std::fill(begin() + size_, begin() + new_size, Type{});

                /*for (auto it = end(); it != begin() + new_size; ++it) {			/// замените на контейнерный алгоритм из <algorithm>
                     *it = std::move(Type{});
                }*/
            }

            size_ = new_size;
        }
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity > GetCapacity()) {
            SimpleVector tmp(new_capacity);
            tmp.Resize(GetCapacity());
            std::move(begin(), end(), tmp.begin());
            swap(tmp);
        }
    }

    // Возвращает итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    Iterator begin() noexcept {
        if (IsEmpty()) {
            return nullptr;
        }

        return &items_[0];
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    Iterator end() noexcept {
        if (IsEmpty()) {
            return nullptr;
        }

        return &items_[GetSize()];
    }

    // Возвращает константный итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator begin() const noexcept {
        return cbegin();
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator end() const noexcept {
        return cend();
    }

    // Возвращает константный итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator cbegin() const noexcept {
        return ConstIterator{ &items_[0] };
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator cend() const noexcept {
        return ConstIterator{ &items_[GetSize()] };
    }

private:
    ArrayPtr<Type> items_{};
    size_t size_ = 0;
    size_t capacity_ = 0;

    void DoubleIfFull() {
        if (GetSize() == GetCapacity()) {
            Reserve(GetCapacity() != 0 ? GetCapacity() * 2 : 1);
        }
    }
};

template<typename Type>
inline bool operator==(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return (&lhs == &rhs)
        || (lhs.GetSize() == rhs.GetSize()
            && std::equal(lhs.begin(), lhs.end(), rhs.begin()));
}

template<typename Type>
inline bool operator!=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(lhs == rhs);
}

template<typename Type>
inline bool operator<(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template<typename Type>
inline bool operator<=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(lhs > rhs);
}

template<typename Type>
inline bool operator>(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return rhs < lhs;
}

template<typename Type>
inline bool operator>=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(lhs < rhs);
}