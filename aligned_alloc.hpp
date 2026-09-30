#pragma once
#include <memory>
#include <new>
#include <iostream>
#include <cstddef>

template <typename T, std::size_t alignAt>
struct AlignedAllocator {
    using value_type = T;

    AlignedAllocator() noexcept = default;

    template <typename U>
    AlignedAllocator(const AlignedAllocator<U, alignAt>& other) {}

    T* allocate(std::size_t n) {
        if (n > std::size_t(-1) / sizeof(T)){
            throw std::bad_alloc();
        }
        return static_cast<T*> (::operator new(n * sizeof(T), std::align_val_t(alignAt)));
    }

    void deallocate(T* p, std::size_t) noexcept {
        ::operator delete (p, std::align_val_t(alignAt));
    }

    template <typename U, std::size_t uAlign>
    friend bool operator==(const AlignedAllocator<T, alignAt>&, const AlignedAllocator<U, uAlign>&) {
        return alignAt == uAlign;
    }

    template <typename U>
    struct rebind{
        using other = AlignedAllocator<U, alignAt>;
    };
};