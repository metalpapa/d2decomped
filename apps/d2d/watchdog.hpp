// SPDX-License-Identifier: GPL-3.0-or-later
// Main-loop phase tracking for the hang watchdog.
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace d2d::client {

// Watchdog — a background thread that fires a diagnostic when the main
// thread stops advancing its heartbeat. This is our only visibility
// into a beachball, because a hung main thread stops running our
// per-frame `slow frame` / `alive` prints. The watchdog needs to touch
// NO SDL state (SDL is main-thread only on macOS); it only reads two
// atomics and writes to stderr.
enum class MainPhase : std::uint32_t {
    Idle           = 0,
    PollEvents     = 1,
    Devctl         = 2,
    Render         = 3,
    Upload         = 4,   // SDL_UpdateTexture
    Present        = 5,   // SDL_RenderPresent
    PaceDelay      = 6,   // the pacer's sleep at end of frame
    // InGame sub-phases so we can pinpoint the stuck one exactly.
    IngameClear    = 10,
    IngameFloor    = 11,
    IngameShadow   = 12,
    IngameWalls    = 13,
    IngamePlayer   = 14,
    IngameHudText  = 15,
    IngameRain     = 16,
    IngameLabels   = 17,
    IngameCopy     = 18,
    IngameAutomap  = 19,
};
inline const char* main_phase_name(std::uint32_t phase) {
    switch (MainPhase(phase)) {
        case MainPhase::Idle:          return "idle";
        case MainPhase::PollEvents:    return "poll-events";
        case MainPhase::Devctl:        return "devctl-pump";
        case MainPhase::Render:        return "render";
        case MainPhase::Upload:        return "sdl-update-texture";
        case MainPhase::Present:       return "sdl-render-present";
        case MainPhase::PaceDelay:     return "sdl-delay-pace";
        case MainPhase::IngameClear:   return "ingame:fb-clear";
        case MainPhase::IngameFloor:   return "ingame:world-floor";
        case MainPhase::IngameShadow:  return "ingame:world-shadow";
        case MainPhase::IngameWalls:   return "ingame:world-walls";
        case MainPhase::IngamePlayer:  return "ingame:player-dcc";
        case MainPhase::IngameHudText: return "ingame:hud-text";
        case MainPhase::IngameRain:    return "ingame:rain";
        case MainPhase::IngameLabels:  return "ingame:item-labels";
        case MainPhase::IngameCopy:    return "ingame:wide-copy";
        case MainPhase::IngameAutomap: return "ingame:automap";
    }
    return "?";
}

// Set by the main loop before entering render_ingame so the watchdog
// can report which sub-phase we're stuck in. Declared ahead of
// render_ingame so it can write the ingame sub-phases.
inline std::atomic<std::uint32_t>* g_current_phase_ptr = nullptr;

// Microseconds spent in each ingame sub-phase (phase 10..19) since the last
// report, summed by set_phase / close_phase for the 5 s `alive` line.
inline std::array<std::uint64_t, 10> g_phase_us{};
inline std::uint64_t g_phase_mark_us = 0;
inline std::uint64_t phase_now_us() {
    return std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
inline void set_phase(MainPhase phase) {
    if (!g_current_phase_ptr) return;
    const auto previous = g_current_phase_ptr->load(std::memory_order_relaxed);
    const auto now = phase_now_us();
    if (previous >= 10 && previous < 20 && g_phase_mark_us) g_phase_us[previous - 10] += now - g_phase_mark_us;
    g_phase_mark_us = now;
    g_current_phase_ptr->store(std::uint32_t(phase), std::memory_order_relaxed);
}
// The render is over: charge what's left to the ingame sub-phase it ended in.
inline void close_phase() {
    if (!g_current_phase_ptr) return;
    const auto previous = g_current_phase_ptr->load(std::memory_order_relaxed);
    if (previous >= 10 && previous < 20 && g_phase_mark_us) g_phase_us[previous - 10] += phase_now_us() - g_phase_mark_us;
    g_phase_mark_us = 0;
}

}  // namespace d2d::client
