#include "Utilities.h"

#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <cwctype>
#include <fstream>
#include <limits>

namespace eatrax {
namespace {

std::string WideToCodePage(const std::wstring& value, const UINT codePage) {
    if (value.empty()) {
        return {};
    }
    const int required = WideCharToMultiByte(codePage, 0, value.data(),
                                              static_cast<int>(value.size()), nullptr, 0, nullptr,
                                              nullptr);
    if (required <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(codePage, 0, value.data(), static_cast<int>(value.size()), result.data(),
                        required, nullptr, nullptr);
    return result;
}

std::wstring TrimUpper(std::wstring value) {
    const auto notSpace = [](const wchar_t character) { return !std::iswspace(character); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const wchar_t character) { return std::towupper(character); });
    return value;
}

}  // namespace

std::string WideToUtf8(const std::wstring& value) { return WideToCodePage(value, CP_UTF8); }

std::string WideToAnsi(const std::wstring& value) { return WideToCodePage(value, CP_ACP); }

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                              static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), required);
    return result;
}

std::wstring ReadIniString(const std::filesystem::path& path, const wchar_t* section,
                           const wchar_t* key, const wchar_t* fallback) {
    std::vector<wchar_t> buffer(1024);
    for (;;) {
        const DWORD length = GetPrivateProfileStringW(section, key, fallback, buffer.data(),
                                                       static_cast<DWORD>(buffer.size()),
                                                       path.c_str());
        if (length + 1 < buffer.size() || buffer.size() >= 32768) {
            return std::wstring(buffer.data(), length);
        }
        buffer.resize(buffer.size() * 2);
    }
}

bool ReadIniBool(const std::filesystem::path& path, const wchar_t* section, const wchar_t* key,
                 const bool fallback) {
    const std::wstring value = TrimUpper(ReadIniString(path, section, key, fallback ? L"1" : L"0"));
    if (value == L"1" || value == L"TRUE" || value == L"YES" || value == L"ON") {
        return true;
    }
    if (value == L"0" || value == L"FALSE" || value == L"NO" || value == L"OFF") {
        return false;
    }
    return fallback;
}

std::uint32_t ReadIniUInt(const std::filesystem::path& path, const wchar_t* section,
                          const wchar_t* key, const std::uint32_t fallback) {
    const std::wstring text = ReadIniString(path, section, key, L"");
    wchar_t* end = nullptr;
    const unsigned long parsed = std::wcstoul(text.c_str(), &end, 0);
    if (end == text.c_str() || *end != L'\0' || parsed > std::numeric_limits<std::uint32_t>::max()) {
        return fallback;
    }
    return static_cast<std::uint32_t>(parsed);
}

float ReadIniFloat(const std::filesystem::path& path, const wchar_t* section, const wchar_t* key,
                   const float fallback) {
    const std::wstring text = ReadIniString(path, section, key, L"");
    wchar_t* end = nullptr;
    const float parsed = std::wcstof(text.c_str(), &end);
    if (end == text.c_str() || *end != L'\0') {
        return fallback;
    }
    return parsed;
}

TrackMode ParseTrackMode(const std::wstring& value, const TrackMode fallback) {
    const std::wstring mode = TrimUpper(value);
    if (mode == L"0" || mode == L"OFF" || mode == L"OF") {
        return TrackMode::Off;
    }
    if (mode == L"1" || mode == L"FE" || mode == L"FRONTEND" || mode == L"FRONT_END") {
        return TrackMode::FrontEnd;
    }
    if (mode == L"2" || mode == L"IG" || mode == L"INGAME" || mode == L"IN_GAME") {
        return TrackMode::InGame;
    }
    if (mode == L"3" || mode == L"ALL" || mode == L"AL") {
        return TrackMode::All;
    }
    return fallback;
}

const char* PlaybackModeText(const TrackMode mode) {
    switch (mode) {
        case TrackMode::FrontEnd:
            return "FE";
        case TrackMode::InGame:
            return "IG";
        case TrackMode::All:
            return "AL";
        case TrackMode::Off:
        default:
            return "";
    }
}

const wchar_t* StateModeText(const TrackMode mode) {
    switch (mode) {
        case TrackMode::FrontEnd:
            return L"FE";
        case TrackMode::InGame:
            return L"IG";
        case TrackMode::All:
            return L"ALL";
        case TrackMode::Off:
        default:
            return L"OFF";
    }
}

std::uint32_t Fnv1a32(const std::string& value) {
    std::uint32_t hash = 2166136261u;
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

std::uint64_t Fnv1a64(const std::string& value) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string Hex32(const std::uint32_t value) {
    char buffer[9]{};
    sprintf_s(buffer, sizeof(buffer), "%08X", value);
    return buffer;
}

std::string Hex64(const std::uint64_t value) {
    char buffer[17]{};
    sprintf_s(buffer, sizeof(buffer), "%016llX", static_cast<unsigned long long>(value));
    return buffer;
}

std::string Sha256File(const std::filesystem::path& path, std::string* error) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::vector<unsigned char> object;
    std::array<unsigned char, 32> digest{};
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (error) *error = "open failed";
        return {};
    }

    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    DWORD objectSize = 0;
    DWORD resultSize = 0;
    if (status >= 0) {
        status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                                   reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize),
                                   &resultSize, 0);
    }
    if (status >= 0) {
        object.resize(objectSize);
        status = BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0);
    }

    std::vector<char> buffer(1024 * 1024);
    while (status >= 0 && input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize read = input.gcount();
        if (read > 0) {
            status = BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()),
                                    static_cast<ULONG>(read), 0);
        }
    }
    if (status >= 0) {
        status = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
    }
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0) {
        if (error) *error = "BCrypt SHA-256 failed";
        return {};
    }

    static constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(digest.size() * 2);
    for (const unsigned char byte : digest) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0F]);
    }
    return result;
}

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        if (character >= 'A' && character <= 'Z') {
            return static_cast<char>(character - 'A' + 'a');
        }
        return static_cast<char>(character);
    });
    return value;
}

const std::vector<int>& ExpectedMusicSfxSubsongs() {
    // Pathfinder's raw stream order is not the UG2 jukebox order. Keep this list in the
    // licensed soundtrack order so the imported catalog matches the original EA TRAX UI.
    static const std::vector<int> indices = {
        39, 4, 35, 8, 40, 23, 2, 19, 20, 38, 33, 34, 30, 9,
        5,  6, 3,  31, 37, 36, 7, 10, 11, 18, 22, 21, 32,
    };
    return indices;
}

bool IsExpectedMusicSfxSubsong(const int subsong) {
    const auto& indices = ExpectedMusicSfxSubsongs();
    return std::find(indices.begin(), indices.end(), subsong) != indices.end();
}

}  // namespace eatrax
