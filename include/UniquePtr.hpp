#pragma once
#include <utility>

// default custom deleter so that UniquePtr can support custom deleters
template <typename T>
struct DefaultDelete {
    constexpr void operator()(T* ptr) const noexcept {
        delete ptr;
    }
}; 

template <typename T, typename Deleter = DefaultDelete<T>>
class UniquePtr {
    public:
        constexpr UniquePtr() noexcept : m_ptr(nullptr), m_deleter(Deleter()) {}
        explicit UniquePtr(T* ptr) noexcept : m_ptr(ptr), m_deleter(Deleter()) {}     // explicit to avoid compiler implicit conversions of T* ptr on its own

        UniquePtr(T* ptr, Deleter deleter) noexcept : m_ptr(ptr), m_deleter(std::move(deleter)) {}

        ~UniquePtr() noexcept {
        if (m_ptr) {
            m_deleter(m_ptr);
        }
    }

        // not allowing for copying of UniquePtrs, automatically delete
        UniquePtr(const UniquePtr&) = delete;
        UniquePtr& operator=(const UniquePtr&) = delete;

        // move constructor, take temp rvalue reference, stealing internal pointer, and zeroing out old pointer
        UniquePtr(UniquePtr&& other) noexcept : m_ptr(other.m_ptr), m_deleter(std::move(other.m_deleter)) {
            other.m_ptr = nullptr;
        }
        
        // move assignment operator
        UniquePtr& operator=(UniquePtr&& other) noexcept {
            if (this != &other) {
                if(m_ptr) {
                    m_deleter(m_ptr);
                }
                m_ptr = other.m_ptr;
                m_deleter = std::move(other.m_deleter); 
                other.m_ptr = nullptr;  
            }
            return *this; 
        }

        // overloading operators
        T& operator*() const { return *m_ptr; }
        T* operator->() const { return m_ptr; }
        T* get() const { return m_ptr; }
        
        Deleter& get_deleter() noexcept { return m_deleter; }
        const Deleter& get_deleter() const noexcept { return m_deleter; }
        
        T* release() noexcept {
            T* temp = m_ptr;
            m_ptr = nullptr;
            return temp;
        }

    private:
        T* m_ptr = nullptr;
        [[no_unique_address]] Deleter m_deleter;
}; 