#pragma once
#include <memory>
#include <new>
#include <cstddef>
#include <iostream>
#include <array>
#include <cstdint>


struct ArenaBase {
    std::byte* buffer_ptr;
    std::size_t capacity;
    std::size_t offset;

    ArenaBase() = delete;
    ArenaBase(std::byte* b, std::size_t c) : buffer_ptr(b), capacity(c), offset(0) {}

    template<typename T>
    [[nodiscard]] T* getPointer(std::size_t requested /*bytes*/) {
        std::uintptr_t current_addr = reinterpret_cast<std::uintptr_t>(&buffer_ptr[offset]);
        auto align = alignof(T);
        
        std::size_t pad = (align - current_addr % align) % align;
        if (pad > capacity - offset || requested > capacity - offset - pad)
            throw std::bad_alloc();
        offset += pad;
        
        void* p = &buffer_ptr[offset];
        offset += requested;
        return static_cast<T*>(p);
    }

    void reset() noexcept {
        offset = 0;
    }

};

template <std::size_t initial_alloc>
struct Arena : ArenaBase {
    
    Arena() : ArenaBase(buffer, initial_alloc) {}

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    alignas(std::max_align_t) std::byte buffer[initial_alloc];
};



template <typename T, std::size_t initial>
struct ArenaAllocator {
    using value_type = T;

    ArenaAllocator() {
        arena = std::make_shared<Arena<initial>>();
    };

    template<typename U>
    ArenaAllocator(const ArenaAllocator<U, initial>& other) noexcept {
        arena = other.arena;
    }

    template<typename U>
    friend bool operator==(const ArenaAllocator<T, initial>& l, const ArenaAllocator<U, initial>& r) {
        return l.arena == r.arena;
    }

    T* allocate(std::size_t n) {
        if (n > std::size_t(-1) / sizeof(T)){
            throw std::bad_alloc();
        }

        std::size_t req = n * sizeof(T);
        return arena->template getPointer<T>(req); // can eventually throw      
    }

    void deallocate(T* p, std::size_t) noexcept {}

    template<typename U>
    struct rebind {using other = ArenaAllocator<U, initial>;};

    std::shared_ptr<Arena<initial>> arena;
};

template <typename T>
struct ArenaViewAllocator {
    using value_type = T;
    ArenaViewAllocator() = delete;

    explicit ArenaViewAllocator(ArenaBase* a) noexcept : arena(a) {};

    template<typename U>
    ArenaViewAllocator(const ArenaViewAllocator<U>& other) noexcept {
        arena = other.arena;
    }

    template<typename U>
    friend bool operator==(const ArenaViewAllocator<T>& l, const ArenaViewAllocator<U>& r) {
        return l.arena == r.arena;
    }

    T* allocate(std::size_t n) {
        if (n > std::size_t(-1) / sizeof(T)){
            throw std::bad_alloc();
        }

        std::size_t req = n * sizeof(T);
        return arena->template getPointer<T>(req);     
    }

    void deallocate(T* p, std::size_t) noexcept {}

    template<typename U>
    struct rebind {using other = ArenaViewAllocator<U>;};

    ArenaBase* arena;
};