#pragma once

#include <cstdint>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace eatrax {

enum class TrackMode : std::uint8_t {
    Off = 0,
    FrontEnd = 1,
    InGame = 2,
    All = 3,
};

enum class SourceKind {
    External,
    Underground2MusicSfx,
    PursuitSps,
};

struct Track {
    SourceKind sourceKind = SourceKind::External;
    std::filesystem::path sourcePath;
    int subsong = 0;
    std::string stableKey;
    std::string stateSection;
    std::string title;
    std::string album;
    std::string artist;
    std::string playbackMode;
    TrackMode mode = TrackMode::All;
    std::uint32_t eventId = 0;
    bool loop = false;
    bool loudnessMeasured = false;
    float loudnessGain = 1.0f;
    float loudnessGainDb = 0.0f;
    // Optional native adaptive bank, indexed by the user's MW situation IDs.
    std::array<std::uint32_t, 13> adaptiveEvents{};
    std::array<std::uint32_t, 4> nativeBodyEvents{};
    std::array<std::uint32_t, 7> nativeParts{};
    std::array<std::uint32_t, 3> nativeIntensityParts{};
};

struct Config {
    bool enabled = true;
    bool loadExternalTracks = true;
    bool loadMusicSfx = true;
    float volumeMultiplier = 1.0f;
    std::uint32_t maxCustomTracks = 256;
    std::filesystem::path modRoot;
    std::filesystem::path iniPath;
    std::filesystem::path tracksDirectory;
    std::filesystem::path musicSfxDirectory;
    std::filesystem::path musicSfxManifest;
    std::filesystem::path statePath;
    std::filesystem::path logPath;
    bool enablePursuit = false;
    std::filesystem::path pursuitDirectory;
};

struct MusicSfxStreamInfo {
    int subsong = 0;
    int channels = 0;
    int sampleRate = 0;
    std::int64_t sampleCount = 0;
    std::string codec;
};

struct MusicSfxInspection {
    bool supported = false;
    int subsongCount = 0;
    std::vector<MusicSfxStreamInfo> importedStreams;
    std::string error;
};

}  // namespace eatrax
