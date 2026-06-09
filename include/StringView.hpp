#pragma once
#include <cstddef>
#include <cstring>
#include <cassert>

class StringView {
    public: 
        constexpr StringView() noexcept : m_data(nullptr), m_size(0) {}
        constexpr StringView(const char* str) noexcept {
            if (str) {
                m_data = str;
                size_t len = 0;
                while (str[len] != '\0') {
                    ++len;
                }
                m_size = len; 
            } else {
                m_data = nullptr;
                m_size = 0;
            }
        }
        constexpr StringView(const char* str, size_t len) noexcept : m_data(str), m_size(len) {}

        // size getter
        constexpr size_t size() const noexcept { return m_size; }

        // data ptr getter
        constexpr const char* data() const noexcept { return m_data; }               

        // whether or not the size is 0 
        constexpr bool empty() const noexcept { return m_size == 0; }
        
        // overloading char* access by returning char at pos with custom assert cheks (index < m_size)
        constexpr char operator[](size_t index) const noexcept {
            assert(index < m_size && "StringView index out of bounds!");
            return m_data[index]; 
        }
        
        

    private: 
        const char* m_data = nullptr;       // pointer to the beginning of the string, and const since string is read only 
        size_t m_size = 0;                  // cannot rely one \n so every bboundary check & char iteration must reflect window size
        
        
}; 