#include <new>  // for ::operator new and std::bad_alloc
#include <memory>
#include <cstddef> // for std::size_t
#include <iostream>

template<typename T>
struct OutputAllocator {
    using value_type = T;   // required by stl

    // required by stl, default constructor 
    OutputAllocator() noexcept {
        std::cout<< "default constructor of OutputAllocator called" << std::endl;
    }; 

    // required by stl, conversion
    template <typename U>
    OutputAllocator(const OutputAllocator<U>&) noexcept {
        std::cout<< "conversion constructor of OutputAllocator called" << std::endl;
    } 

    // required, allocator
    T* allocate(std::size_t n /*how many elements*/) { 
        std::cout << "custom allocator for " << n << " elements has been called, allocating " << n * sizeof(T) << " bytes" <<std::endl ; 
        if (n > (std::size_t(-1) / sizeof(T))){
            throw std::bad_alloc();
        }

        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    // required, free
    void deallocate(T* p, std::size_t n) {
        std::cout << "custom delete for " << p << " pointer called. n parameter is ignored" << std::endl;
        ::operator delete(p);
    }

    // required, boolean operator: can an allocator free another allocator?
    template <typename U>
    friend bool operator==(const OutputAllocator<U>&, const OutputAllocator<T>&) noexcept {
        std::cout << "operator== called on OutputAllocator" << std::endl; 
        return true;
    }
};