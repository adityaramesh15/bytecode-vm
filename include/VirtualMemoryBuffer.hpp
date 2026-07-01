#pragma once
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <memory>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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
                size_t page_size = 0;
#if defined(_WIN32)
                SYSTEM_INFO si;
                GetSystemInfo(&si);
                page_size = static_cast<size_t>(si.dwPageSize);
#else
                page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE)); 
#endif
                
                m_size = ((size + page_size - 1) / page_size) * page_size; 

#if defined(_WIN32)
                DWORD flProtect = translate_permissions_win(initial_perm);
                m_address = ::VirtualAlloc(nullptr, m_size, MEM_COMMIT | MEM_RESERVE, flProtect);
                if (!m_address) {
                    throw std::bad_alloc();
                }
#else
                int prot = translate_permissions_posix(initial_perm);
                m_address = ::mmap(nullptr, m_size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); 
                if (m_address == MAP_FAILED) {
                    throw std::bad_alloc();
                }
#endif
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
#if defined(_WIN32)
                DWORD flOldProtect = 0;
                DWORD flNewProtect = translate_permissions_win(perm);
                return ::VirtualProtect(m_address, m_size, flNewProtect, &flOldProtect) != 0;
#else
                int prot = translate_permissions_posix(perm);
                return ::mprotect(m_address, m_size, prot) == 0;
#endif
            }

            [[nodiscard]] void* data() const noexcept { return m_address; }
            [[nodiscard]] size_t size() const noexcept { return m_size; }
            [[nodiscard]] bool is_allocated() const noexcept { return m_address != nullptr; }
      
        private:
            void* m_address{nullptr};
            size_t m_size{0};

            void release() noexcept {
                if (m_address && m_size > 0) {
#if defined(_WIN32)
                    ::VirtualFree(m_address, 0, MEM_RELEASE);
#else
                    ::munmap(m_address, m_size);
#endif
                    m_address = nullptr;
                    m_size = 0;
                }
            }

#if defined(_WIN32)
            [[nodiscard]] constexpr DWORD translate_permissions_win(MemoryPermission perm) noexcept {
                uint8_t raw_perm = static_cast<uint8_t>(perm);
                bool has_read = (raw_perm & static_cast<uint8_t>(MemoryPermission::Read)) != 0;
                bool has_write = (raw_perm & static_cast<uint8_t>(MemoryPermission::Write)) != 0;
                bool has_exec = (raw_perm & static_cast<uint8_t>(MemoryPermission::Exec)) != 0;

                if (has_exec) {
                    if (has_write) return PAGE_EXECUTE_READWRITE;
                    if (has_read) return PAGE_EXECUTE_READ;
                    return PAGE_EXECUTE;
                } else {
                    if (has_write) return PAGE_READWRITE;
                    if (has_read) return PAGE_READONLY;
                    return PAGE_NOACCESS;
                }
            }
#else
            [[nodiscard]] constexpr int translate_permissions_posix(MemoryPermission perm) noexcept {
                int prot = PROT_NONE;
                uint8_t raw_perm = static_cast<uint8_t>(perm);
                if (raw_perm & static_cast<uint8_t>(MemoryPermission::Read))  prot |= PROT_READ;
                if (raw_perm & static_cast<uint8_t>(MemoryPermission::Write)) prot |= PROT_WRITE;
                if (raw_perm & static_cast<uint8_t>(MemoryPermission::Exec))  prot |= PROT_EXEC;
                return prot;
            }
#endif
    };
}