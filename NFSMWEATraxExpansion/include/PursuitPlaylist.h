#pragma once
#include "Catalog.h"
#include "PursuitPolicy.h"
#include "Utilities.h"
#include <Windows.h>
#include <cwchar>
namespace eatrax {
inline std::vector<std::size_t> EnabledPursuitList(const CatalogResult& catalog) {
    std::vector<std::size_t> result;
    std::array<bool, kPursuitScores.size() + 1> listed{};
    const auto& ini = catalog.config.iniPath;
    auto add = [&](const wchar_t* key, unsigned definition) {
        if (listed[definition]) return;
        listed[definition] = true;
        if (!ReadIniBool(ini, L"Pursuit", key, true)) return;
        if (definition == 0) { result.push_back(0); return; }
        const auto event = kPursuitScores[definition - 1].event;
        for (std::size_t i = 0; i < catalog.pursuitTracks.size(); ++i)
            if ((catalog.pursuitTracks[i].eventId & 0xFFFFFFu) == event) { result.push_back(i + 1); return; }
    };
    // Preserve the order in which the user wrote song keys in [Pursuit].
    wchar_t keys[8192]{};
    GetPrivateProfileStringW(L"Pursuit", nullptr, L"", keys, 8192, ini.c_str());
    for (const auto* key = keys; *key; key += std::wcslen(key) + 1) {
        const auto name = PursuitName(key);
        if (name == L"vanilla") add(key, 0);
        for (unsigned i = 0; i < kPursuitScores.size(); ++i)
            if (name == PursuitName(kPursuitScores[i].key)) add(key, i + 1);
    }
    add(L"Vanilla", 0);
    for (unsigned i = 0; i < kPursuitScores.size(); ++i) add(kPursuitScores[i].key, i + 1);
    return result;
}
}
