#pragma once
#include "LinearArena.hpp"
#include <cstddef>
#include <utility>

namespace MemoryEngine {

    template <typename T>
    class ArenaAllocator {
        public: 
            using value_type = T;
            using propagate_on_container_move_assignment = std::true_type;
            using propagate_on_container_swap = std::true_type;
            using propagate_on_container_copy_assignment = std::false_type;

            template <typename U>
            friend class ArenaAllocator;
            
            ArenaAllocator() = delete;
            explicit ArenaAllocator(LinearArena& arena) noexcept : m_arena(&arena) {}


            template <typename U>
            constexpr ArenaAllocator(const ArenaAllocator<U>& other) noexcept 
                : m_arena(other.m_arena) {}
            
            ~ArenaAllocator() = default;
            
            [[nodiscard]] T* allocate(std::size_t n) {
                if (n == 0) return nullptr;
                void* raw_ptr = m_arena->allocate_raw<T>(n);
                return reinterpret_cast<T*>(raw_ptr);
            }

            void deallocate(T* storage_ptr, std::size_t n) noexcept {
                (void)storage_ptr;
                (void)n;
            }
            
            template <typename U>
            [[nodiscard]] bool operator==(const ArenaAllocator<U>& other) const noexcept {
                return m_arena == other.m_arena;
            }

        private: 
            LinearArena* m_arena{nullptr};  // not using unique_ptr since ArenaAllocator is non-owning, just a bridge for containers to LinearArena
    };


}