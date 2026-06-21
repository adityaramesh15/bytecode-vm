#pragma once
#include <cstddef>
#include <memory>
#include <new> 
#include <span>
#include <utility>

namespace MemoryEngine {
    class LinearArena {
        public:
            explicit LinearArena(size_t total_capacity_bytes) : m_capacity(total_capacity_bytes), m_offset(0) {
                if (m_capacity > 0) {
                    m_buffer = std::make_unique<std::byte[]>(m_capacity);
                }
            }

            ~LinearArena() = default;
            LinearArena(const LinearArena&) = delete;
            LinearArena& operator=(const LinearArena&) = delete;

            LinearArena(LinearArena&& other) noexcept : 
                m_buffer(std::move(other.m_buffer)), 
                m_capacity(other.m_capacity), 
                m_offset(other.m_offset) 
            {
                other.m_capacity = 0;
                other.m_offset = 0;    
            }

            LinearArena& operator=(LinearArena&& other) noexcept {
                if (this != &other) {
                    m_buffer = std::move(other.m_buffer);
                    m_capacity = other.m_capacity;
                    m_offset = other.m_offset;
                    
                    other.m_capacity = 0;
                    other.m_offset = 0;
                }
                return *this; 
            }

            // for when i want to allocate but not instantiate (think resizing vector space but not emplacing objects just yet)
            template <typename T>
            [[nodiscard]] void* allocate_raw(size_t count = 1) {
                size_t bytes_needed = sizeof(T) * count;
                size_t alignment = alignof(T);

                void* current_ptr = static_cast<void*>(m_buffer.get() + m_offset);
                size_t space_left = m_capacity - m_offset; 
                
                void* aligned_ptr = std::align(alignment, bytes_needed, current_ptr, space_left);
                if (!aligned_ptr) {
                    throw std::bad_alloc();
                }

                m_offset = (static_cast<std::byte*>(aligned_ptr) + bytes_needed) - m_buffer.get();
                return aligned_ptr;
            }

            template <typename T, typename... Args>
            [[nodiscard]] T* allocate(Args&&... args) {
                void* aligned_ptr = allocate_raw<T>(1);
                return ::new (aligned_ptr) T(std::forward<Args>(args)...); 
            }

            void reset() noexcept {
                m_offset = 0; 
            }

            [[nodiscard]] size_t capacity() const noexcept { return m_capacity; }
            [[nodiscard]] size_t bytes_used() const noexcept { return m_offset; }
            [[nodiscard]] std::span<std::byte> current_allocations() noexcept {
                return std::span<std::byte>(m_buffer.get(), m_offset);
            }

            [[nodiscard]] std::span<const std::byte> current_allocations() const noexcept {
                return std::span<const std::byte>(m_buffer.get(), m_offset);
            }

        private:
            std::unique_ptr<std::byte[]> m_buffer{nullptr};
            size_t m_capacity{0};
            size_t m_offset{0};
    };
}