// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

#include <fmt/format.h>

namespace Common {

/// Frame time distribution over a window of frames. Averages hide stutter and uneven pacing
/// (12 ms / 21 ms alternating frames average 16.7 ms), so this keeps every sample of the window
/// and reports percentiles and how many frames went over the usual budgets.
class FrameTimeStats {
public:
    static constexpr std::size_t Window = 600;
    /// Gaps longer than this (emulation paused, hidden window, loads without presents) are not
    /// frame times: callers skip them.
    static constexpr long long MaxFrameUs = 1'000'000;

    /// Adds one frame time in microseconds. Returns true when the window is full; then call
    /// TakeSummary to get its line and start the next window (samples added to a full window
    /// are dropped).
    bool Add(long long frame_us) {
        if (m_count < Window) {
            m_samples[m_count++] = frame_us;
        }
        return m_count == Window;
    }

    /// One line describing the window ("fps 59.9, frame ms p50 16.7 ..."), then clears it.
    std::string TakeSummary() {
        const std::size_t n = m_count;
        m_count = 0;
        if (n == 0) {
            return "no frames";
        }
        // The window is cleared anyway: sort it in place.
        auto& sorted = m_samples;
        std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(n));
        long long total = 0;
        std::size_t over_18 = 0, over_20 = 0, over_34 = 0, over_50 = 0, over_100 = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const long long us = sorted[i];
            total += us;
            over_18 += us > 18'000 ? 1 : 0;
            over_20 += us > 20'000 ? 1 : 0;
            over_34 += us > 34'000 ? 1 : 0;
            over_50 += us > 50'000 ? 1 : 0;
            over_100 += us > 100'000 ? 1 : 0;
        }
        const auto at = [&](std::size_t permille) {
            return sorted[(std::min)(n - 1, n * permille / 1000)] / 1000.0;
        };
        const double fps = total > 0 ? 1'000'000.0 * static_cast<double>(n) / static_cast<double>(total) : 0.0;
        return fmt::format("fps {:.1f}, frame ms min {:.1f} p50 {:.1f} p95 {:.1f} p99 {:.1f} max "
                           "{:.1f}; frames over 18 ms {}, 20 ms {}, 34 ms {}, 50 ms {}, 100 ms {} "
                           "(of {})",
                           fps, sorted[0] / 1000.0, at(500), at(950), at(990),
                           sorted[n - 1] / 1000.0, over_18, over_20, over_34, over_50, over_100, n);
    }

private:
    std::array<long long, Window> m_samples{};
    std::size_t m_count{};
};

} // namespace Common
