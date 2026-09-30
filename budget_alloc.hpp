#pragma once
#include <new>
#include <memory>
#include <cstddef> 
#include <iostream>

template <typename T, std::size_t budget>
struct BudgetAllocator {
    using value_type = T;

    BudgetAllocator() {
        if (budget >= std::size_t(-1))
            throw std::bad_alloc();
        free_space = std::make_shared<std::size_t>(budget);
    }

    template <typename U, std::size_t Ubudget>
    BudgetAllocator(const BudgetAllocator<U, Ubudget>& other) noexcept {
        std::cout << "other.free_space: " << *other.free_space << std::endl;
        free_space = other.free_space;
    }

    T* allocate (std::size_t n) {
        if (n > std::size_t(-1) / sizeof(T))
            throw std::bad_alloc();
        auto req = n * sizeof(T);
        std::cout << "allocating memory. requested: " << req <<", available: " << *free_space <<std::endl;
        if (req > *free_space)
            throw std::bad_alloc();
        T* p = static_cast<T*>( ::operator new(req));
        *free_space -= req;
        std::cout << "allocated. available now: " << *free_space << std::endl;
        return p;
    }

    void deallocate (T* p, std::size_t n) noexcept {
        auto req = n*sizeof(T);
        std::cout << "freeing the memory" <<std::endl;
        ::operator delete(p, req);
        *free_space += req;
    }

    template <typename U, std::size_t Ubudget>
    [[nodiscard]] friend bool operator==(const BudgetAllocator<T, budget>& t, const BudgetAllocator<U, Ubudget>& u) noexcept {
        return t.free_space == u.free_space;
    }

    template <typename U>
    struct rebind { using other = BudgetAllocator<U, budget>; };


    std::shared_ptr<std::size_t> free_space; // in bytes 
};