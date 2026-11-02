// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <vector>

#include "common/alignment.h"
#include "common/common_types.h"
#include "common/div_ceil.h"
#include "common/assert.h"
#include "common/slot_vector.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"

namespace VideoCommon {

/// EDEN_NX2_DRAW_FAST=1 (read once at startup): an unchanged texture or sampler descriptor gives
/// back the id resolved when it was last read new from an array next to the descriptors, instead
/// of a hash map lookup per texture and per sampler on every draw. Off: exactly the old path.
inline const bool g_nx2_draw_fast = [] {
    const char* const value = std::getenv("EDEN_NX2_DRAW_FAST");
    return value != nullptr && value[0] == '1' && value[1] == '\0';
}();

template <typename T>
class DescriptorTable {
public:
    [[nodiscard]] bool Synchronize(GPUVAddr gpu_addr, u32 limit) noexcept {
        bool ret = !(current_gpu_addr == gpu_addr && current_limit == limit);
        if (ret) {
            Refresh(gpu_addr, limit);
        }
        return ret;
    }

    void Invalidate() noexcept {
        // Cleared on the next read: the texture cache invalidates every channel's tables for
        // each image it deletes, dozens of times a frame while its collector runs.
        invalidated = true;
    }

    [[nodiscard]] std::pair<T, bool> Read(Tegra::MemoryManager const& gpu_memory, u32 index) noexcept {
        DEBUG_ASSERT(index <= current_limit);
        if (invalidated) [[unlikely]] {
            std::ranges::fill(read_descriptors, 0);
            invalidated = false;
        }
        const GPUVAddr gpu_addr = current_gpu_addr + index * sizeof(T);
        std::pair<T, bool> result;
        gpu_memory.ReadBlockUnsafe(gpu_addr, std::addressof(result.first), sizeof(T));
        if ((read_descriptors[index / 64] & (1ULL << (index % 64))) != 0) {
            result.second = result.first != descriptors[index];
        } else {
            read_descriptors[index / 64] |= 1ULL << (index % 64);
            result.second = true;
        }
        if (result.second) {
            descriptors[index] = result.first;
        }
        return result;
    }

    void Refresh(GPUVAddr gpu_addr, u32 limit) noexcept {
        current_gpu_addr = gpu_addr;
        current_limit = limit;
        // Round up geometrically to keep growth amortized without allocating 16 MiB for every
        // small 32-byte descriptor table (Djihads80/Eglantine PR #277, nowhere02).
        const size_t num_descriptors = std::bit_ceil(static_cast<size_t>(limit) + 1);
        size_t old_size = read_descriptors.size();
        read_descriptors.resize(Common::DivCeil(num_descriptors, 64U));
        old_size = (std::min)(old_size, read_descriptors.size());
        std::fill(read_descriptors.begin(), read_descriptors.begin() + old_size, 0);
        invalidated = false;
        //
        if (descriptors.size() < num_descriptors) {
            descriptors.resize(num_descriptors);
        }
        if (g_nx2_draw_fast && resolved_ids.size() < num_descriptors) {
            resolved_ids.resize(num_descriptors);
        }
    }

    std::vector<u64> read_descriptors;
    std::vector<T> descriptors;
    /// EDEN_NX2_DRAW_FAST: the id the texture cache resolved at entry i's last new read (written
    /// with the cache's index map, read only while entry i's read bit is set, as that map is).
    std::vector<Common::SlotId> resolved_ids;
    GPUVAddr current_gpu_addr{};
    u32 current_limit{};
    bool invalidated{}; ///< read_descriptors is to be cleared before the next read
};

} // namespace VideoCommon
