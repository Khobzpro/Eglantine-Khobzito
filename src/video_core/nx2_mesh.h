// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdlib>

namespace VideoCommon::Nx2 {

/// EDEN_NX2_MESH=1: translate and draw Switch 2 (NVN) task/mesh programs with
/// VK_EXT_mesh_shader. Off by default: those programs stay rejected and their draws skipped.
/// Read once, at static initialisation.
inline const bool mesh_requested = [] {
    const char* const value = std::getenv("EDEN_NX2_MESH");
    return value != nullptr && value[0] != '\0' && value[0] != '0';
}();

/// Requested and the Vulkan device enabled VK_EXT_mesh_shader (task and mesh stages). Set once
/// by the Vulkan pipeline cache before any draw; false under OpenGL and in every other case.
/// Plain bool: the shader cache and the draw path read it on every refresh.
inline bool mesh_enabled = false;

} // namespace VideoCommon::Nx2
