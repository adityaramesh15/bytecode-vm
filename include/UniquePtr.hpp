#pragma once

template <typename T>
class UniquePtr {
    public:
        constexpr UniquePtr() noexcept : m_ptr(nullptr) {}
        explicit UniquePtr(T* ptr) noexcept : m_ptr(ptr) {}     // explicit to avoid compiler implicit conversions of T* ptr on its own
        ~UniquePtr() noexcept {
            delete m_ptr;
        }

        // not allowing for copying of UniquePtrs, automatically delete
        UniquePtr(const UniquePtr&) = delete;
        UniquePtr& operator=(const UniquePtr&) = delete;

        // move constructor, take temp rvalue reference, stealing internal pointer, and zeroing out old pointer
        UniquePtr(UniquePtr&& other) noexcept : m_ptr(other.m_ptr) {
            other.m_ptr = nullptr;
        }
        
        // move assignment operator
        UniquePtr& operator=(UniquePtr&& other) noexcept {
            if (this != &other) {
                delete m_ptr;
                m_ptr = other.m_ptr;
                other.m_ptr = nullptr;  
            }
            return *this; 
        }

        // overloading operators
        T& operator*() const { return *m_ptr; }
        T* operator->() const { return m_ptr; }
        T* get() const { return m_ptr; }
        T* release() noexcept {
            T* temp = m_ptr;
            m_ptr = nullptr;
            return temp;
        }

    private:
        T* m_ptr = nullptr;
        
}; 