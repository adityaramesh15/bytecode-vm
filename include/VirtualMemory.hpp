#pragma once
#include "LinearArena.hpp"
#include <cstdint>
#include <expected>
#include <cstddef>

namespace MemoryEngine {
    enum class VirtualMemoryError : uint8_t {
        PageFault,
        AccessViolation,
        OutOfPhysicalMemory,
        InvalidPermissions,
        ResourceRecyclerOverflow
    };

    struct PageTableEntry {
        /*
            since each frame has to be a multiple of 2**12, the last 12 bits are always empty
            the bits 31 to 12 are the actual physical frame number
            we can use the remaining 12 bits for metadata, and then shift them away to grab the PFN

            Layout: Bits 12-31 are for Page Tables = Physical Frame Number
                    Bit 0                          = Present flag
                    Bit 1                          = Writable flag
                    Bit 2                          = Readbale flag
        */
        uint32_t value{0U}; // represents the frame number + meta data (leftover 12 bits of virtual mem address)

        static constexpr uint32_t FLAG_PRESENT = 1U << 0;
        static constexpr uint32_t FLAG_READ = 1U << 1;
        static constexpr uint32_t FLAG_WRITE = 1U << 2;

        void configure(uint32_t pfn, bool readable, bool writable) noexcept {
            value = (pfn << 12) | FLAG_PRESENT;
            if(readable) value |= FLAG_READ; 
            if(writable) value |= FLAG_WRITE; 
        }

        [[nodiscard]] constexpr uint32_t pfn() const noexcept { return value >> 12; }
        [[nodiscard]] constexpr bool is_present() const noexcept { return (value & FLAG_PRESENT) != 0; }
        [[nodiscard]] constexpr bool is_readable() const noexcept { return (value & FLAG_READ) != 0; }
        [[nodiscard]] constexpr bool is_writable() const noexcept { return (value & FLAG_WRITE) != 0; }
    };

    // aligning page tables and physical frames since both are on same system ram contiguously
    struct alignas(4096) PageTable {
        PageTableEntry entries[1024UZ]; // 10 bits outer, 10 bits inner
    };

    struct alignas(4096) PhysicalFrame {
        std::byte data[4096];
    }; 

    // Translation Lookaside Buffer (to cache frequent page to frame lookups)
    struct TLBEntry {
        uint32_t virtual_page_number{0xFFFFFFFFU}; 
        uint32_t physical_frame_number{0U}; 
        uint32_t permissions{0U};
        bool valid{false};
    }; 


    class MemoryManagementUnit {
        public:
            static constexpr uint32_t PAGE_SIZE = 4096U;
            static constexpr size_t TLB_SIZE = 16UZ;

            explicit MemoryManagementUnit(LinearArena& physical_ram) noexcept
                : m_ram(&physical_ram) {
                
                m_ram -> pin();  // claiming pinned memory in Arena
                for (size_t i = 0; i < 1024UZ; ++i) {
                    m_directory[i] = nullptr;
                    m_page_table_ref_counts[i] = 0U;
                }
                for (size_t i = 0; i < TLB_SIZE; ++i) {
                    m_tlb[i] = TLBEntry{};
                }
            }
            
            ~MemoryManagementUnit() noexcept {
                if (m_ram) {
                    m_ram -> unpin();
                }
            }
            MemoryManagementUnit(const MemoryManagementUnit&) = delete;
            MemoryManagementUnit& operator=(const MemoryManagementUnit&) = delete;

            std::expected<void, VirtualMemoryError> map_page(uint32_t virtual_address, bool readable, bool writable) noexcept {
                const uint32_t pdi = (virtual_address >> 22) & 0x3FFU;
                const uint32_t pti = (virtual_address >> 12) & 0x3FFU;
                const uint32_t vpn = virtual_address >> 12;

                if (!m_directory[pdi]) {
                    if (m_table_free_list_size > 0UZ) {
                        m_directory[pdi] = m_table_free_list[--m_table_free_list_size];
                        for (size_t i = 0; i < 1024UZ; ++i) {
                            m_directory[pdi]->entries[i].value = 0U;
                        }
                    } else {
                        const size_t space_needed = sizeof(PageTable) + alignof(PageTable);
                        if (m_ram->capacity() - m_ram->bytes_used() < space_needed) {
                            return std::unexpected(VirtualMemoryError::OutOfPhysicalMemory);
                        }
                        try {
                            m_directory[pdi] = m_ram->allocate<PageTable>();
                        } catch (...) {
                            return std::unexpected(VirtualMemoryError::OutOfPhysicalMemory);
                        }
                    }
                    m_page_table_ref_counts[pdi] = 0U;
                }
                
                PageTable* table = m_directory[pdi];
                uint32_t assigned_pfn = 0U;

                if (table->entries[pti].is_present()) {
                    assigned_pfn = table->entries[pti].pfn();
                } else {
                    if (m_free_list_size > 0UZ) {
                        assigned_pfn = m_frame_free_list[--m_free_list_size];
                    } else {
                        const size_t frame_space_needed = sizeof(PhysicalFrame) + alignof(PhysicalFrame);
                        if (m_ram->capacity() - m_ram->bytes_used() < frame_space_needed) {
                            return std::unexpected(VirtualMemoryError::OutOfPhysicalMemory);
                        }
                        try {
                            void* frame_ptr = m_ram -> allocate_raw<PhysicalFrame>(1UZ);
                            std::byte* arena_base = static_cast<std::byte*>(m_ram -> current_allocations().data()); 
                            
                            // take the allocation ptr, subtract arena start to get offset, divide by frame size to get PFN
                            assigned_pfn = static_cast<uint32_t>((static_cast<std::byte*>(frame_ptr) - arena_base) / PAGE_SIZE);
                        } catch (...) {
                            return std::unexpected(VirtualMemoryError::OutOfPhysicalMemory);
                        }
                    }
                    m_page_table_ref_counts[pdi]++;
                }

                table->entries[pti].configure(assigned_pfn, readable, writable);
                
                const size_t tlb_index = vpn % TLB_SIZE;
                if (m_tlb[tlb_index].virtual_page_number == vpn) {
                    m_tlb[tlb_index].valid = false;
                }

                return {};
            }

            [[nodiscard]] std::expected<size_t, VirtualMemoryError> translate(uint32_t virtual_address, bool require_write) noexcept {
                const uint32_t vpn = virtual_address >> 12;
                const uint32_t offset = virtual_address & 0xFFFU;
                const size_t tlb_index = vpn % TLB_SIZE;

                if (m_tlb[tlb_index].valid && m_tlb[tlb_index].virtual_page_number == vpn) {
                    if (require_write) {
                        if (!(m_tlb[tlb_index].permissions & PageTableEntry::FLAG_WRITE)) {
                            return std::unexpected(VirtualMemoryError::AccessViolation);
                        }
                    } else {
                        if (!(m_tlb[tlb_index].permissions & PageTableEntry::FLAG_READ)) {
                            return std::unexpected(VirtualMemoryError::AccessViolation);
                        }
                    }
                    m_tlb_hits++;
                    return (static_cast<size_t>(m_tlb[tlb_index].physical_frame_number) * PAGE_SIZE) + offset;
                }

                m_tlb_misses++;
                const uint32_t pdi = (virtual_address >> 22) & 0x3FFU;
                const uint32_t pti = (virtual_address >> 12) & 0x3FFU;

                if (!m_directory[pdi]) {
                    return std::unexpected(VirtualMemoryError::PageFault);
                }

                const PageTable* table = m_directory[pdi];
                const PageTableEntry pte = table->entries[pti];

                if (!pte.is_present()) {
                    return std::unexpected(VirtualMemoryError::PageFault);
                }

                if (require_write) {
                    if (!pte.is_writable()) return std::unexpected(VirtualMemoryError::AccessViolation);
                } else {
                    if (!pte.is_readable()) return std::unexpected(VirtualMemoryError::AccessViolation);
                }

                m_tlb[tlb_index] = TLBEntry{
                    .virtual_page_number = vpn,
                    .physical_frame_number = pte.pfn(),
                    .permissions = pte.value & 0x7U,
                    .valid = true
                };

                return (static_cast<size_t>(pte.pfn()) * PAGE_SIZE) + offset;
            }            

            std::expected<void, VirtualMemoryError> unmap_page(uint32_t virtual_address) noexcept {
                const uint32_t pdi = (virtual_address >> 22) & 0x3FFU;
                const uint32_t pti = (virtual_address >> 12) & 0x3FFU;
                const uint32_t vpn = virtual_address >> 12;

                if (!m_directory[pdi]) return {};

                PageTable* table = m_directory[pdi];
                if (table->entries[pti].is_present()) {
                    const uint32_t pfn = table->entries[pti].pfn();
                    const bool will_release_table = (m_page_table_ref_counts[pdi] == 1U);

                    if (m_free_list_size >= MAX_TRACKED_FRAMES) {
                        return std::unexpected(VirtualMemoryError::ResourceRecyclerOverflow);
                    }
                    if (will_release_table && m_table_free_list_size >= MAX_TRACKED_TABLES) {
                        return std::unexpected(VirtualMemoryError::ResourceRecyclerOverflow);
                    }

                    m_frame_free_list[m_free_list_size++] = pfn;
                    table->entries[pti].value = 0U;

                    if (--m_page_table_ref_counts[pdi] == 0U) {
                        m_table_free_list[m_table_free_list_size++] = m_directory[pdi];
                        m_directory[pdi] = nullptr;
                    }
                }

                const size_t tlb_index = vpn % TLB_SIZE;
                if (m_tlb[tlb_index].virtual_page_number == vpn) {
                    m_tlb[tlb_index].valid = false;
                }

                return {};
            }

            [[nodiscard]] size_t tlb_hits() const noexcept { return m_tlb_hits; }
            [[nodiscard]] size_t tlb_misses() const noexcept { return m_tlb_misses; }         

        private:
            LinearArena* m_ram{nullptr};
            PageTable* m_directory[1024UZ];
            uint16_t m_page_table_ref_counts[1024UZ];

            TLBEntry m_tlb[TLB_SIZE]; 

            static constexpr size_t MAX_TRACKED_FRAMES = 4096UZ;
            uint32_t m_frame_free_list[MAX_TRACKED_FRAMES]{0U};
            size_t m_free_list_size{0UZ};

            static constexpr size_t MAX_TRACKED_TABLES = 1024UZ;
            PageTable* m_table_free_list[MAX_TRACKED_TABLES]{nullptr};
            size_t m_table_free_list_size{0UZ};
            
            size_t m_tlb_hits{0UZ};
            size_t m_tlb_misses{0UZ};
    };
}