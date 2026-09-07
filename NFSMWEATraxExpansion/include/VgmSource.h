#pragma once

#include "Types.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace eatrax {

class VgmDecoder {
public:
    VgmDecoder();
    ~VgmDecoder();
    VgmDecoder(const VgmDecoder&) = delete;
    VgmDecoder& operator=(const VgmDecoder&) = delete;

    bool Open(const std::filesystem::path& mpfPath, int subsong, std::string* error);
    int ReadFrames(std::int16_t* destination, int frameCount);
    void Seek(std::uint64_t frame);
    std::uint64_t Cursor() const;
    std::uint64_t Length() const;
    int Channels() const;
    int SampleRate() const;
    bool AtEnd() const;

private:
    void* handle_ = nullptr;
};

MusicSfxInspection InspectMusicSfx(const std::filesystem::path& mpfPath);
bool DecodeMusicSfxPrefix(const std::filesystem::path& mpfPath, int subsong,
                          std::uint32_t framesToDecode, std::uint64_t* checksum,
                          std::uint32_t* framesDecoded, std::string* error);

}  // namespace eatrax
