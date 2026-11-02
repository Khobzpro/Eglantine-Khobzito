// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Common {

enum class MemoryPermission : u32 {
    Read = 1 << 0,
    Write = 1 << 1,
    ReadWrite = Read | Write,
    Execute = 1 << 2,
};
DECLARE_ENUM_FLAG_OPERATORS(MemoryPermission)

/**
 * A low level linear memory buffer, which supports multiple mappings
 * Its purpose is to rebuild a given sparse memory layout, including mirrors.
 */
class HostMemory {
public:
    explicit HostMemory(size_t backing_size_, size_t virtual_size_);
    ~HostMemory();

    /**
     * Copy constructors. They shall return a copy of the buffer without the mappings.
     * TODO: Implement them with COW if needed.
     */
    HostMemory(const HostMemory& other) = delete;
    HostMemory& operator=(const HostMemory& other) = delete;

    /**
     * Move constructors. They will move the buffer and the mappings to the new object.
     */
    HostMemory(HostMemory&& other) noexcept;
    HostMemory& operator=(HostMemory&& other) noexcept;

    void Map(size_t virtual_offset, size_t host_offset, size_t length, MemoryPermission perms,
             bool separate_heap);

    void Unmap(size_t virtual_offset, size_t length, bool separate_heap);

    void Protect(size_t virtual_offset, size_t length, MemoryPermission perms);

    void EnableDirectMappedAddress();

    /**
     * Fills backing pages the kernel's allocator just handed out. With fill_value 0, pages that
     * are still zero since the backing was created are skipped (for Switch 2 programs, or for all
     * with EDEN_NX2_LAZY_ZERO=1; =0 turns it off): writing them would only make them resident.
     * Only call it on pages just taken from the allocator, never on pages already in use.
     */
    void ClearBackingRegion(size_t physical_offset, size_t length, u32 fill_value);

    /// The kernel's allocator opened the first reference to these backing pages.
    void MarkBackingHandedOut(size_t physical_offset, size_t length);

    /// References to these backing pages were closed (or the allocator was set up again): a page
    /// handed out while zero may have been written since, so it no longer counts as zero.
    void MarkBackingReturned(size_t physical_offset, size_t length);

    /// MarkBackingReturned for the whole backing (the kernel's allocator is set up again).
    void MarkAllBackingReturned();

    /// Pages written outside the allocator (kernel regions, management data): never zero again.
    void MarkBackingNotZero(size_t physical_offset, size_t length);

    [[nodiscard]] u8* BackingBasePointer() noexcept {
        return backing_base;
    }
    [[nodiscard]] const u8* BackingBasePointer() const noexcept {
        return backing_base;
    }

    [[nodiscard]] u8* VirtualBasePointer() noexcept {
        return virtual_base;
    }
    [[nodiscard]] const u8* VirtualBasePointer() const noexcept {
        return virtual_base;
    }

    [[nodiscard]] size_t VirtualSize() const noexcept {
        return virtual_size;
    }

    bool IsInVirtualRange(void* address) const noexcept {
        return address >= virtual_base && address < virtual_base + virtual_size;
    }

private:
    void InitializePageBitmaps();
    bool GetBackingPages(size_t physical_offset, size_t length, size_t& first,
                         size_t& count) const;
    // What a fill did before the page bitmaps: punch a hole (Linux, zero fills), else memset.
    void FillBackingRange(size_t physical_offset, size_t length, u32 fill_value);

    size_t backing_size{};
    size_t virtual_size{};

#if !(defined(__OPENORBIS__) || defined(__managarm__))
    // Low level handler for the platform dependent memory routines
    class Impl;
    std::unique_ptr<Impl> impl;
#endif
    u8* backing_base{};
    u8* virtual_base{};
    size_t virtual_base_offset{};
    // Windows requires it for kernels whom lack proper support for some functions!
    bool fallback_buffer{false};

    // One bit per backing page, set at creation (fresh backing reads as zero):
    // never_handed_out: the page never left the kernel's allocator.
    // handed_out_zero: it left the allocator while still zero and has not been filled since.
    // A page with both bits clear may hold data, so ClearBackingRegion fills it.
    std::unique_ptr<std::atomic<u64>[]> never_handed_out;
    std::unique_ptr<std::atomic<u64>[]> handed_out_zero;
    size_t page_bitmap_words{};
};

} // namespace Common
