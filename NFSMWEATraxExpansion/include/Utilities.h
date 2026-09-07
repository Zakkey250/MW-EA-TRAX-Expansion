#pragma once

#include "Types.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace eatrax {

std::string WideToUtf8(const std::wstring& value);
std::string WideToAnsi(const std::wstring& value);
std::wstring Utf8ToWide(const std::string& value);
std::wstring ReadIniString(const std::filesystem::path& path, const wchar_t* section,
                           const wchar_t* key, const wchar_t* fallback);
bool ReadIniBool(const std::filesystem::path& path, const wchar_t* section, const wchar_t* key,
                 bool fallback);
std::uint32_t ReadIniUInt(const std::filesystem::path& path, const wchar_t* section,
                          const wchar_t* key, std::uint32_t fallback);
float ReadIniFloat(const std::filesystem::path& path, const wchar_t* section, const wchar_t* key,
                   float fallback);
TrackMode ParseTrackMode(const std::wstring& value, TrackMode fallback);
const char* PlaybackModeText(TrackMode mode);
const wchar_t* StateModeText(TrackMode mode);
std::uint32_t Fnv1a32(const std::string& value);
std::uint64_t Fnv1a64(const std::string& value);
std::string Hex32(std::uint32_t value);
std::string Hex64(std::uint64_t value);
std::string Sha256File(const std::filesystem::path& path, std::string* error = nullptr);
std::string LowerAscii(std::string value);
bool IsExpectedMusicSfxSubsong(int subsong);
const std::vector<int>& ExpectedMusicSfxSubsongs();

}  // namespace eatrax
