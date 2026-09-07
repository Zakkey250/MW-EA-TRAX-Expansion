#pragma once
#include <Windows.h>
#include <cstring>

namespace eatrax {
// Resolve the game's own import entry, rather than the kernel export that
// another ASI may already bypass through its private trampoline.
inline void** FindGameImport(HMODULE module, const char* library, const char* function) {
    auto* base = reinterpret_cast<unsigned char*>(module);
    if (!base) return nullptr;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0) return nullptr;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return nullptr;
    const auto imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!imports.VirtualAddress || !imports.Size) return nullptr;
    auto* entry = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + imports.VirtualAddress);
    for (; entry->Name; ++entry) {
        if (_stricmp(reinterpret_cast<const char*>(base + entry->Name), library)) continue;
        if (!entry->OriginalFirstThunk || !entry->FirstThunk) return nullptr;
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + entry->OriginalFirstThunk);
        auto* slots = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + entry->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal)) continue;
            auto* name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            if (!std::strcmp(reinterpret_cast<const char*>(name->Name), function))
                return reinterpret_cast<void**>(&slots->u1.Function);
        }
    }
    return nullptr;
}
}
