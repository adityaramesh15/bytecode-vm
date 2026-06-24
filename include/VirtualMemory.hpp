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
        InvalidPermissions
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

    struct PageTable {
        PageTableEntry entries[1024UZ]; // 10 bits outer, 10 bits inner
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
                for (size_t i = 0; i < 1024UZ; ++i) {
                    m_directory[i] = nullptr;
                }
                m_max_frames = m_ram -> capacity(); 
            }
            
            std::expected<void, VirtualMemoryError> map_page(uint32_t virtual_address, bool readable, bool writable) noexcept {
                const uint32_t pdi = (virtual_address >> 22) & 0x3FFU;
                const uint32_t pti = (virtual_address >> 12) & 0x3FFU;
                const uint32_t vpn = virtual_address >> 12;

                if (!m_directory[pdi]) {
                    try {
                        m_directory[pdi] = m_ram -> allocate<PageTable>();
                    } catch (const std::bad_alloc&) {
                        return std::unexpected(VirtualMemoryError::OutOfPhysicalMemory);
                    }
                }
                
                PageTable* table = m_directory[pdi];

                uint32_t assigned_pfn = 0U;
                if (m_free_list_size > 0UZ) {
                    assigned_pfn = m_frame_free_list[--m_free_list_size];
                } else {
                    if (m_allocated_frames_counter >= m_max_frames) {
                        return std::unexpected(VirtualMemoryError::OutOfPhysicalMemory);
                    }
                    assigned_pfn = static_cast<uint32_t>(m_allocated_frames_counter++);
                    try {
                        (void)m_ram->allocate_raw<std::byte>(PAGE_SIZE);
                    } catch (...) {
                        return std::unexpected(VirtualMemoryError::OutOfPhysicalMemory);
                    }
                }

                table->entries[pti].configure(assigned_pfn, readable, writable);
                
                // invalidating the matching line inside local Software TLB cache to guarantee coherence
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
                    m_tlb_hits++;
                    if (require_write && !(m_tlb[tlb_index].permissions & PageTableEntry::FLAG_WRITE)) {
                        return std::unexpected(VirtualMemoryError::AccessViolation);
                    }
                    return (static_cast<size_t>(m_tlb[tlb_index].physical_frame_number) * PAGE_SIZE) + offset;
                }

                m_tlb_misses++; // just keeping track of num types we cant use TLB
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

                if (require_write && !pte.is_writable()) {
                    return std::unexpected(VirtualMemoryError::AccessViolation);
                }

                // populating the TLB index with the newly translated metadata
                m_tlb[tlb_index] = TLBEntry{
                    .virtual_page_number = vpn,
                    .physical_frame_number = pte.pfn(),
                    .permissions = pte.value & 0x7U,
                    .valid = true
                };

                return (static_cast<size_t>(pte.pfn()) * PAGE_SIZE) + offset;
            }            

            void unmap_page(uint32_t virtual_address) noexcept {
                const uint32_t pdi = (virtual_address >> 22) & 0x3FFU;
                const uint32_t pti = (virtual_address >> 12) & 0x3FFU;
                const uint32_t vpn = virtual_address >> 12;

                if (!m_directory[pdi]) return;

                PageTable* table = m_directory[pdi];
                if (table->entries[pti].is_present()) {
                    if (m_free_list_size < MAX_TRACKED_FRAMES) {
                        m_frame_free_list[m_free_list_size++] = table->entries[pti].pfn();
                    }
                    table->entries[pti].value = 0U; 
                }

                const size_t tlb_index = vpn % TLB_SIZE;
                if (m_tlb[tlb_index].virtual_page_number == vpn) {
                    m_tlb[tlb_index].valid = false;
                }
            }

            [[nodiscard]] size_t tlb_hits() const noexcept { return m_tlb_hits; }
            [[nodiscard]] size_t tlb_misses() const noexcept { return m_tlb_misses; }
            [[nodiscard]] size_t allocated_frames() const noexcept { return m_allocated_frames_counter; }            

        private:
            LinearArena* m_ram{nullptr};
            PageTable* m_directory[1024UZ];

            TLBEntry m_tlb[TLB_SIZE]; 

            static constexpr size_t MAX_TRACKED_FRAMES = 4096UZ;
            uint32_t m_frame_free_list[MAX_TRACKED_FRAMES]{0U};
            size_t m_free_list_size{0UZ};

            size_t m_allocated_frames_counter{0UZ};
            size_t m_max_frames{0UZ};
            
            size_t m_tlb_hits{0UZ};
            size_t m_tlb_misses{0UZ};
    };




}