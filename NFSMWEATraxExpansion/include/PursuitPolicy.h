#pragma once
#include <cstddef>
#include <cstdint>
#include <cwctype>
#include "Types.h"
#include "PursuitScores.h"
#include <algorithm>

namespace eatrax {
// Group zero delegates the complete stock adaptive score. External groups never
// claim a heat/intensity mapping that is not present in their source assets.
inline std::size_t SelectPursuitGroup(std::size_t externalCount, std::uint32_t random) {
    return static_cast<std::size_t>(random) % (externalCount + 1);
}

inline std::wstring PursuitName(std::wstring name) {
    std::wstring result;
    for (wchar_t c : name) if (!std::iswspace(c) && c != L'_' && c != L'-') result += static_cast<wchar_t>(std::towlower(c));
    if (result == L"battletheme2") return L"battleracetheme2";
    if (result == L"nfsmw") return L"vanilla";
    return result;
}

// Random and List both draw from the enabled song list on each pursuit start.
inline std::size_t SelectListedPursuitGroup(const std::vector<std::size_t>& enabled,
    std::uint32_t random) {
    if (enabled.empty()) return 0; // Keep the game's stock score as a safe fallback.
    return enabled[random % enabled.size()];
}

// Resolve by stable entry event, never by the current catalog order.
// Missing/invalid test scores fall back to vanilla and are reported by the caller.
inline std::size_t SelectTestPursuitGroup(std::wstring name, const std::vector<Track>& tracks,
                                        std::uint32_t random, bool& valid) {
    name = PursuitName(name);
    valid = true;
    if (name.empty() || name == L"random") return SelectPursuitGroup(tracks.size(), random);
    if (name == L"vanilla") return 0;
    std::uint32_t event = 0;
    for (const auto& score : kPursuitScores) if (name == PursuitName(score.key)) event = score.event;
    if (event) for (std::size_t i = 0; i < tracks.size(); ++i)
        if ((tracks[i].eventId & 0xFFFFFFu) == event) return i + 1;
    valid = false;
    return 0;
}

// Published MW interactive-event identifiers (xan1242/XNFSMusicPlayer, MIT).
// These alone are NOT a pursuit detector: the native pursuit-start boundary
// must first arm the session, so ambience/preview events remain unaffected.
inline bool IsInteractiveMusicEvent(std::uint32_t eventId) {
    constexpr std::uint32_t ids[] = {
        0x6DD6BB,0x9DF7DA,0xE6FF17,0x7690D2,0x96E300,0x46EDA3,0xB2D374,
        0x9509F2,0x919B1B,0xE40616,0x1BBC15,0x34209F,0x2F7671,0xC505B7,
        0x27205F,0x4C6876,0xE2814E,0xB39639,0x518C15,0xE9222F,0xC0DC6F,
        0xEA0327,0x1B6B71,0xB29728,0xE391AF,0xDF442E,0xD874B1,0xD12570,
        0x32F37E,0x56667B,0x515634,0x580861,0xD2E818,0x9B7A50,0x7B768A,
        0x4F246E,0xC3FA91,0x2BBA48,0x22E859,0x90B6DC,0x641F27,0x6E7282,0x531659
    };
    for (const auto id : ids) if (id == (eventId & 0x00FFFFFFu)) return true;
    return false;
}
} // namespace eatrax
