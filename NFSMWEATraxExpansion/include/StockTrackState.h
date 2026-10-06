#pragma once
#include "Utilities.h"
#include <Windows.h>
#include <optional>

namespace eatrax {
// Name is the bounded native profile name, not an address or installation path.
// Only explicit normal-mode changes create overrides; absent keys use the save.
inline std::wstring StockModeSection(const std::string& profile, std::uint32_t event) {
    return Utf8ToWide("Vanilla_" + Hex64(Fnv1a64(profile)) + "_" + Hex32(event & 0xFFFFFFu));
}
inline std::optional<unsigned> ReadStockMode(const std::filesystem::path& state,
                                            const std::string& profile, std::uint32_t event) {
    if (profile.empty() || !event) return {};
    const auto section = StockModeSection(profile,event);
    const auto value = ReadIniUInt(state,section.c_str(),L"Mode",~0u);
    return value <= 3 ? std::optional<unsigned>(value) : std::nullopt;
}
inline bool WriteStockMode(const std::filesystem::path& state, const std::string& profile,
                           std::uint32_t event, unsigned mode) {
    if (profile.empty() || !event || mode > 3) return false;
    const auto section = StockModeSection(profile,event);
    const auto value = std::to_wstring(mode);
    return WritePrivateProfileStringW(section.c_str(),L"Mode",value.c_str(),state.c_str()) != FALSE;
}
} // namespace eatrax
