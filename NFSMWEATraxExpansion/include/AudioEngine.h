#pragma once

#include "Types.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace eatrax {

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool Initialize(std::string* error, bool noDevice = false);
    bool Play(const Track& track, float volume, std::string* error);
    void Stop();
    void Pause();
    void Resume();
    void SetVolume(float volume);
    float OutputVolume() const;
    // Offline validation only: rejected when the engine has a playback device.
    bool RenderFrames(float* output, std::uint64_t frames);
    std::int32_t RemainingMilliseconds() const;
    bool IsActive() const;
    bool HasEnded() const;
    bool IsPaused() const;

    static bool ProbeExternalFile(const std::filesystem::path& path, std::string* error);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace eatrax
