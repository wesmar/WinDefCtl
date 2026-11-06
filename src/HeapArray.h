#pragma once

#include <windows.h>
#include <cstddef>
#include "RuntimeSupport.h"

template <typename T>
class HeapArray {
    static_assert(__is_trivially_copyable(T), "HeapArray requires trivially copyable T");
    T*     data_     = nullptr;
    size_t size_     = 0;
    size_t capacity_ = 0;

public:
    HeapArray() = default;
    explicit HeapArray(size_t initialCapacity) {
        reserve(initialCapacity);
    }
    ~HeapArray() noexcept {
        if (data_) HeapFree(GetProcessHeap(), 0, data_);
    }

    HeapArray(const HeapArray&) = delete;
    HeapArray& operator=(const HeapArray&) = delete;

    HeapArray(HeapArray&& o) noexcept
        : data_(o.data_), size_(o.size_), capacity_(o.capacity_) {
        o.data_ = nullptr;
        o.size_ = 0;
        o.capacity_ = 0;
    }

    HeapArray& operator=(HeapArray&& o) noexcept {
        if (this != &o) {
            if (data_) HeapFree(GetProcessHeap(), 0, data_);
            data_ = o.data_;
            size_ = o.size_;
            capacity_ = o.capacity_;
            o.data_ = nullptr;
            o.size_ = 0;
            o.capacity_ = 0;
        }
        return *this;
    }

    bool reserve(size_t cap) noexcept {
        if (cap <= capacity_) return true;
        HANDLE h = GetProcessHeap();
        void* p = data_ ? HeapReAlloc(h, 0, data_, cap * sizeof(T))
                        : HeapAlloc(h, 0, cap * sizeof(T));
        if (!p) return false;
        data_ = static_cast<T*>(p);
        capacity_ = cap;
        return true;
    }

    bool resize(size_t sz) noexcept {
        if (sz > capacity_) {
            size_t newCap = (capacity_ * 3) / 2 + 8;
            if (newCap < sz) newCap = sz;
            if (!reserve(newCap)) return false;
        }
        if (sz > size_) {
            memset(data_ + size_, 0, (sz - size_) * sizeof(T));
        }
        size_ = sz;
        return true;
    }

    bool push_back(const T& item) noexcept {
        if (size_ >= capacity_) {
            size_t newCap = capacity_ ? (capacity_ * 2) : 8;
            if (!reserve(newCap)) return false;
        }
        data_[size_++] = item;
        return true;
    }

    void clear() noexcept {
        size_ = 0;
    }

    size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    T* data() noexcept { return data_; }
    const T* data() const noexcept { return data_; }

    T& operator[](size_t i) noexcept { return data_[i]; }
    const T& operator[](size_t i) const noexcept { return data_[i]; }

    T* begin() noexcept { return data_; }
    T* end() noexcept { return data_ + size_; }
    const T* begin() const noexcept { return data_; }
    const T* end() const noexcept { return data_ + size_; }
};
