#pragma once
#include <cstddef>
#include <memory>
#include <new> 
#include <span>
#include <utility>
#include <cassert>
#include "VirtualMemoryBuffer.hpp"

namespace MemoryEngine {
    class LinearArena {
        public:
            explicit LinearArena(size_t total_capacity_bytes) 
                : m_buffer(total_capacity_bytes, MemoryPermission::ReadWrite),
                  m_capacity(m_buffer.size()), m_offset(0), m_pin_count(0) {}

            ~LinearArena() = default;
            LinearArena(const LinearArena&) = delete;
            LinearArena& operator=(const LinearArena&) = delete;

            LinearArena(LinearArena&& other) noexcept : 
                m_buffer(std::move(other.m_buffer)), 
                m_capacity(other.m_capacity), 
                m_offset(other.m_offset),
                m_pin_count(other.m_pin_count)
            {
                assert(other.m_pin_count == 0 && "Lifecycle Violation: Cannot move a LinearArena while active dependencies are pinned.");
                other.m_capacity = 0;
                other.m_offset = 0;    
                other.m_pin_count = 0;
            }

            LinearArena& operator=(LinearArena&& other) noexcept {
                if (this != &other) {
                    assert(m_pin_count == 0 && "Lifecycle Violation: Cannot overwrite an active pinned LinearArena.");
                    assert(other.m_pin_count == 0 && "Lifecycle Violation: Cannot move an active pinned LinearArena.");

                    m_buffer = std::move(other.m_buffer);
                    m_capacity = other.m_capacity;
                    m_offset = other.m_offset;
                    m_pin_count = other.m_pin_count;
                    
                    other.m_capacity = 0;
                    other.m_offset = 0;
                    other.m_pin_count = 0;
                }
                return *this; 
            }

            // for when i want to allocate but not instantiate (think resizing vector space but not emplacing objects just yet)
            template <typename T>
            [[nodiscard]] void* allocate_raw(size_t count = 1UZ) {
                size_t bytes_needed = sizeof(T) * count;
                size_t alignment = alignof(T);

                auto* base_ptr = static_cast<std::byte*>(m_buffer.data());
                auto current_ptr = static_cast<void*>(base_ptr + m_offset);
                size_t space_left = m_capacity - m_offset; 
                
                void* aligned_ptr = std::align(alignment, bytes_needed, current_ptr, space_left);
                if (!aligned_ptr) {
                    throw std::bad_alloc();
                }

                m_offset = static_cast<size_t>((static_cast<std::byte*>(aligned_ptr) + bytes_needed) - base_ptr);
                return aligned_ptr;
            }

            template <typename T, typename... Args>
            [[nodiscard]] T* allocate(Args&&... args) {
                static_assert(std::is_trivially_destructible_v<T>, "Architecture Constraint Violation: LinearArena can only allocate trivially destructible types to ensure safe fast resets.");

                void* aligned_ptr = allocate_raw<T>(1UZ);
                return ::new (aligned_ptr) T(std::forward<Args>(args)...); 
            }

            void reset() {
                
                if (m_pin_count > 0) {
                    throw std::runtime_error("Security Violation: Cannot reset LinearArena while active subsystems hold live memory pins.");
                }
                m_offset = 0; 
            }

            void pin() noexcept { ++m_pin_count; }
            void unpin() noexcept { if (m_pin_count > 0) --m_pin_count; }
            [[nodiscard]] bool is_pinned() const noexcept { return m_pin_count > 0; }

            [[nodiscard]] size_t capacity() const noexcept { return m_capacity; }
            [[nodiscard]] size_t bytes_used() const noexcept { return m_offset; }
            [[nodiscard]] std::span<std::byte> current_allocations() noexcept {
                return std::span<std::byte>(static_cast<std::byte*>(m_buffer.data()), m_offset);
            }

            [[nodiscard]] std::span<const std::byte> current_allocations() const noexcept {
                return std::span<const std::byte>(static_cast<const std::byte*>(m_buffer.data()), m_offset);
            }

        private:
            VirtualMemoryBuffer m_buffer; 
            size_t m_capacity{0};
            size_t m_offset{0};
            size_t m_pin_count{0};          // a counter to track system dependencies in Arena (for safe clearing)
    };
}