#pragma once
#include <cstddef>
#include <sys/mman.h>
#include <unistd.h>
#include <stdexcept>
#include <utility>
#include <memory>

namespace MemoryEngine {
    enum class MemoryPermission : uint8_t {
        None = 0,
        Read = 1 << 0,
        Write = 1 << 1,
        Exec = 1 << 2,
        ReadWrite = static_cast<uint8_t>(Read) | static_cast<uint8_t>(Write),
        ReadExec  = static_cast<uint8_t>(Read) | static_cast<uint8_t>(Exec)
    }; 

    class VirtualMemoryBuffer {
        public:
            VirtualMemoryBuffer() noexcept : m_address(nullptr), m_size(0) {}

            explicit VirtualMemoryBuffer(size_t size, MemoryPermission initial_perm = MemoryPermission::ReadWrite) {
                size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE)); 
                
                m_size = ((size + page_size - 1) / page_size) * page_size; 

                int prot = translate_permissions(initial_perm);

                m_address = ::mmap(nullptr, m_size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); 
                if (m_address == MAP_FAILED) {
                    throw std::bad_alloc();
                }
            }
            

            VirtualMemoryBuffer(const VirtualMemoryBuffer&) = delete;
            VirtualMemoryBuffer& operator=(const VirtualMemoryBuffer&) = delete;

            VirtualMemoryBuffer(VirtualMemoryBuffer&& other) noexcept 
                : m_address(other.m_address), m_size(other.m_size) {
                other.m_address = nullptr;
                other.m_size = 0;
            }

            VirtualMemoryBuffer& operator=(VirtualMemoryBuffer&& other) noexcept {
                if (this != &other) {
                    release();
                    m_address = other.m_address;
                    m_size = other.m_size;
                    other.m_address = nullptr;
                    other.m_size = 0;
                }
                return *this; 
            }
      
            ~VirtualMemoryBuffer() noexcept {
                release();
            }
      
            bool protect(MemoryPermission perm) noexcept {
                if (!m_address || m_size == 0) return false;
                int prot = translate_permissions(perm);
                return ::mprotect(m_address, m_size, prot) == 0;
            }

            [[nodiscard]] void* data() const noexcept { return m_address; }
            [[nodiscard]] size_t size() const noexcept { return m_size; }
            [[nodiscard]] bool is_allocated() const noexcept { return m_address != nullptr; }
      
      
        private:
            void* m_address{nullptr};
            size_t m_size{0};

            void release() noexcept {
                if (m_address && m_size > 0) {
                    ::munmap(m_address, m_size);
                    m_address = nullptr;
                    m_size = 0;
                }
            }

            [[nodiscard]] constexpr int translate_permissions(MemoryPermission perm) {
                int prot = PROT_NONE;
                uint8_t raw_perm = static_cast<uint8_t>(perm);
                if (raw_perm & static_cast<uint8_t>(MemoryPermission::Read))  prot |= PROT_READ;
                if (raw_perm & static_cast<uint8_t>(MemoryPermission::Write)) prot |= PROT_WRITE;
                if (raw_perm & static_cast<uint8_t>(MemoryPermission::Exec))  prot |= PROT_EXEC;
                return prot;
            }
    };
}