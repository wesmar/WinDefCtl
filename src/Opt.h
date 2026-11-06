#pragma once

template <typename T>
struct Opt {
    bool has_val = false;
    T    val{};

    constexpr Opt() noexcept = default;
    constexpr Opt(const T& v) noexcept : has_val(true), val(v) {}

    constexpr explicit operator bool() const noexcept { return has_val; }
    constexpr bool has_value() const noexcept { return has_val; }
    constexpr const T& value() const noexcept { return val; }
    constexpr T& value() noexcept { return val; }
    constexpr const T& operator*() const noexcept { return val; }
    constexpr T& operator*() noexcept { return val; }
    constexpr const T* operator->() const noexcept { return &val; }
    constexpr T* operator->() noexcept { return &val; }
};
