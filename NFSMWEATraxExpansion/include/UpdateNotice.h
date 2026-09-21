#pragma once
#include <Windows.h>
#include <filesystem>
#include <string>
namespace eatrax {
inline constexpr char kReleaseVersion[]="0.4.8";
// An empty result means that the packaged runtime is complete.
std::wstring RuntimeProblem(const std::filesystem::path& root) noexcept;
// Invoked after native bank selection, outside the loader lock and preparation UI.
void StartNotices(HMODULE module, const std::wstring& problem) noexcept;
}
