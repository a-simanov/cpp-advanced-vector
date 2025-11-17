#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory;

template <typename T>
class Vector {
public:

    using iterator = T*;    
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());

    }

    Vector(Vector&& other) noexcept
        :data_(std::move(other.data_))
        , size_(std::exchange(other.size_, 0)) {
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector tmp(rhs);
                Swap(tmp);
            } else {
                if (rhs.Size() < size_) {
                    std::copy_n(rhs.data_.GetAddress(), rhs.Size(), data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + rhs.Size(), size_ - rhs.Size());
                } else {
                    std::copy_n(rhs.data_.GetAddress(), size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress(), rhs.Size() - size_, data_.GetAddress());
                }         
                size_ = rhs.Size();
            }            
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_ = std::move(rhs.data_);
            size_ = std::exchange(rhs.size_, 0);
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    static void Destroy(T* buf) noexcept {
        buf->~T();
    }    

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(begin(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(begin(), size_, new_data.GetAddress());
        }

        std::destroy_n(begin(), size_);

        new_data.Swap(data_);
    }

    void Resize(size_t new_size) {
        if (size_ > new_size) {
            std::destroy_n(begin() + new_size, size_ - new_size);
        } else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(end(), new_size - size_);
        }
        size_ = new_size;
    }


    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack() noexcept {
        std::destroy_at(end() - 1);
        --size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        T* ptr = nullptr;
        if (size_ == Capacity()) {
            size_t new_size = (size_ != 0) ? size_ * 2 : 1;
            RawMemory<T> new_data(new_size);
            ptr = new (new_data + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(begin(), size_, new_data.GetAddress());
            }
            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        } else {
            ptr = new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *ptr;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert (pos >= begin() && pos <= end());
        size_t idx = pos - begin();        
        if (size_ < Capacity()) {
            void* buf = operator new (sizeof(T));
            T* tmp = new (buf) T(std::forward<Args>(args)...);
            new (end()) T(std::forward<T>(*(end() - 1)));
            std::move_backward(begin() + idx, end() - 1, end());
            data_[idx] = std::forward<T>(*tmp);
            tmp->~T();
            operator delete (buf);
        } else if (size_ == Capacity()) {
            size_t new_size = (size_ != 0) ? size_ * 2 : 1;
            RawMemory<T> new_data(new_size);
            new(new_data + idx) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), idx, new_data.GetAddress());
                std::uninitialized_move_n(begin() + idx, size_ - idx, new_data.GetAddress() + idx + 1);
            } else {
                try {
                    std::uninitialized_copy_n(begin(), idx, new_data.GetAddress());
                    std::uninitialized_copy_n(begin() + idx, size_ - idx, new_data.GetAddress() + idx + 1);
                } catch (...) {
                    std::destroy_n(new_data.GetAddress() + idx, 1);
                    throw;
                }                
            }
            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        }
        ++size_;
        return begin() + idx;
    }

    iterator Erase(const_iterator pos) {
        assert (pos >= begin() && pos < end());
        size_t idx = pos - begin();
        std::move(begin() + idx + 1, end(), begin() + idx);
        PopBack();
        return begin() + idx;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (&rhs != this) {
            buffer_ = std::exchange(rhs.buffer_, nullptr);
            capacity_ = std::exchange(rhs.capacity_, 0);
        }
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};