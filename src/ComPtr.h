#pragma once

#include <windows.h>

template <typename T>
class ComPtr {
    T* ptr_ = nullptr;

public:
    constexpr ComPtr() noexcept = default;
    constexpr ComPtr(decltype(nullptr)) noexcept {}
    explicit ComPtr(T* p) noexcept : ptr_(p) {
        if (ptr_) ptr_->AddRef();
    }
    ComPtr(const ComPtr& o) noexcept : ptr_(o.ptr_) {
        if (ptr_) ptr_->AddRef();
    }
    ComPtr(ComPtr&& o) noexcept : ptr_(o.ptr_) {
        o.ptr_ = nullptr;
    }
    ~ComPtr() noexcept {
        Reset();
    }

    ComPtr& operator=(const ComPtr& o) noexcept {
        if (ptr_ != o.ptr_) {
            if (o.ptr_) o.ptr_->AddRef();
            Reset();
            ptr_ = o.ptr_;
        }
        return *this;
    }

    ComPtr& operator=(ComPtr&& o) noexcept {
        if (this != &o) {
            Reset();
            ptr_ = o.ptr_;
            o.ptr_ = nullptr;
        }
        return *this;
    }

    ComPtr& operator=(decltype(nullptr)) noexcept {
        Reset();
        return *this;
    }

    void Reset() noexcept {
        T* old = ptr_;
        if (old) {
            ptr_ = nullptr;
            old->Release();
        }
    }

    void Attach(T* p) noexcept {
        Reset();
        ptr_ = p;
    }

    T* Detach() noexcept {
        T* p = ptr_;
        ptr_ = nullptr;
        return p;
    }

    T* Get() const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    T** operator&() noexcept {
        Reset();
        return &ptr_;
    }
    void** PutVoid() noexcept {
        Reset();
        return reinterpret_cast<void**>(&ptr_);
    }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
};
