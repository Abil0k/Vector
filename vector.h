#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory
{
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity)), capacity_(capacity)
    {
    }

    RawMemory(const RawMemory &) = delete;

    RawMemory(RawMemory &&other) noexcept
        : buffer_(std::exchange(other.buffer_, nullptr)), capacity_(std::exchange(other.capacity_, 0))
    {
    }

    ~RawMemory()
    {
        Deallocate(buffer_);
    }

    RawMemory &operator=(const RawMemory &rhs) = delete;

    RawMemory &operator=(RawMemory &&rhs) noexcept
    {
        if (&rhs != this)
        {
            buffer_ = std::exchange(rhs.buffer_, nullptr);
            capacity_ = std::exchange(rhs.capacity_, 0);
        }
        return *this;
    }

    T *operator+(size_t offset) noexcept
    {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T *operator+(size_t offset) const noexcept
    {
        return const_cast<RawMemory &>(*this) + offset;
    }

    const T &operator[](size_t index) const noexcept
    {
        return const_cast<RawMemory &>(*this)[index];
    }

    T &operator[](size_t index) noexcept
    {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory &other) noexcept
    {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T *GetAddress() const noexcept
    {
        return buffer_;
    }

    T *GetAddress() noexcept
    {
        return buffer_;
    }

    size_t Capacity() const
    {
        return capacity_;
    }

private:
    static T *Allocate(size_t n)
    {
        return n != 0 ? static_cast<T *>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T *buf) noexcept
    {
        operator delete(buf);
    }

    T *buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector
{
public:
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size), size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector &other)
        : data_(other.size_), size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector &&other) noexcept
        : data_(std::move(other.data_)), size_(std::exchange(other.size_, 0))
    {
    }

    ~Vector()
    {
        std::destroy_n(data_.GetAddress(), size_);
    }

    Vector &operator=(const Vector &rhs)
    {
        if (this != &rhs)
        {
            if (rhs.size_ > data_.Capacity())
            {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else
            {
                if (rhs.size_ < size_)
                {
                    std::copy_n(rhs.data_.GetAddress(), rhs.size_, data_.GetAddress());
                    std::destroy_n(data_ + rhs.size_, size_ - rhs.size_);
                }
                else
                {
                    std::copy_n(rhs.data_.GetAddress(), size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_ + size_, rhs.size_ - size_, data_ + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector &operator=(Vector &&rhs) noexcept
    {
        if (this != &rhs)
        {
            data_ = std::move(rhs.data_);
            size_ = std::exchange(rhs.size_, 0);
        }
        return *this;
    }

    void Swap(Vector &other) noexcept
    {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Resize(size_t new_size)
    {
        if (size_ == new_size)
        {
            return;
        }
        if (size_ < new_size)
        {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }
        else
        {
            std::destroy_n(data_ + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    void Reserve(size_t new_capacity)
    {
        if (new_capacity <= data_.Capacity())
        {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        TransferContent(new_data);
    }

    void PushBack(const T &value)
    {
        EmplaceBack(value);
    }

    void PushBack(T &&value)
    {
        EmplaceBack(std::move(value));
    }

    void PopBack() noexcept
    {
        if (size_ > 0)
        {
            --size_;
            std::destroy_n(data_ + size_, 1);
        }
    }

    template <typename... Args>
    T &EmplaceBack(Args &&...args)
    {
        if (size_ == Capacity())
        {
            size_t new_capacity = (size_ == 0 ? 1 : size_ * 2);
            RawMemory<T> new_data(new_capacity);
            new (&new_data[size_]) T(std::forward<Args>(args)...);
            TransferContent(new_data);
        }
        else
        {
            new (end()) T(std::forward<Args>(args)...);
        }
        ++size_;
        return data_[size_ - 1];
    }

    using iterator = T *;
    using const_iterator = const T *;

    iterator begin() noexcept
    {
        return data_.GetAddress();
    }

    iterator end() noexcept
    {
        return data_ + size_;
    }

    const_iterator begin() const noexcept
    {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept
    {
        return data_ + size_;
    }

    const_iterator cbegin() const noexcept
    {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept
    {
        return data_ + size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args &&...args)
    {
        auto offset = pos - begin();
        if (pos == end())
        {
            EmplaceBack(std::move(args)...);
        }
        else
        {
            if (size_ == Capacity())
            {
                size_t new_capacity = (size_ == 0 ? 1 : size_ * 2);
                RawMemory<T> new_data(new_capacity);
                new (new_data + offset) T(std::forward<Args>(args)...);
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
                {
                    std::uninitialized_move_n(begin(), offset, new_data.GetAddress());
                    std::uninitialized_move(begin() + offset, end(), new_data.GetAddress() + offset + 1);
                }
                else
                {
                    std::uninitialized_copy_n(begin(), offset, new_data.GetAddress());
                    std::uninitialized_copy(begin() + offset, end(), new_data.GetAddress() + offset + 1);
                }
                std::destroy_n(begin(), size_);
                data_.Swap(new_data);
            }
            else
            {
                new (end()) T(std::move(*(end() - 1)));
                std::move_backward(begin() + offset, end() - 1, end());
                data_[offset] = T{std::forward<Args>(args)...};
            }
            ++size_;
        }
        return begin() + offset;
    }

    iterator Erase(const_iterator pos)
    {
        auto offset = pos - begin();
        std::move(begin() + offset + 1, end(), begin() + offset);
        std::destroy_n(end() - 1, 1);
        --size_;
        return begin() + offset;
    }

    iterator Insert(const_iterator pos, const T &value)
    {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T &&value)
    {
        return Emplace(pos, std::move(value));
    }

    size_t Size() const noexcept
    {
        return size_;
    }

    size_t Capacity() const noexcept
    {
        return data_.Capacity();
    }

    const T &operator[](size_t index) const noexcept
    {
        return const_cast<Vector &>(*this)[index];
    }

    T &operator[](size_t index) noexcept
    {
        assert(index < size_);
        return data_[index];
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    void TransferContent(RawMemory<T> &new_data)
    {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
        {
            std::uninitialized_move_n(begin(), size_, new_data.GetAddress());
        }
        else
        {
            std::uninitialized_copy_n(begin(), size_, new_data.GetAddress());
        }
        std::destroy_n(begin(), size_);
        data_.Swap(new_data);
    }
};