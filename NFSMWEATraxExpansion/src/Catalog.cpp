#include "Catalog.h"

#include "AudioEngine.h"
#include "Utilities.h"
#include "VgmSource.h"
#include "Loudness.h"
#include "PursuitPolicy.h"

#include <algorithm>
#include <cwctype>
#include <system_error>
#include <map>
#include <Windows.h>
#include <cwchar>
#include <fstream>

namespace eatrax {
namespace {

std::wstring LowerWide(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const wchar_t character) { return std::towlower(character); });
    return value;
}

// Content hashes are reused only while size and last-write FILETIME agree.
// Generated hash records live under Cache and never alter source metadata.
std::string CachedBankHash(const std::filesystem::path& path, const std::filesystem::path& cache) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return {};
    const auto size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    const auto time = (static_cast<std::uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
    const auto key = Utf8ToWide(Hex64(Fnv1a64(WideToUtf8(LowerWide(path.lexically_normal().wstring())))));
    const auto stamp = std::to_wstring(size) + L":" + std::to_wstring(time);
    const auto file = cache / L"NativeHashes.ini";
    if (ReadIniString(file, key.c_str(), L"Stamp", L"") == stamp) {
        const auto saved = WideToUtf8(ReadIniString(file, key.c_str(), L"SHA256", L""));
        if (saved.size() == 64) return saved;
    }
    const auto hash = Sha256File(path);
    if (hash.size() == 64) {
        WritePrivateProfileStringW(key.c_str(), L"SHA256", Utf8ToWide(hash).c_str(), file.c_str());
        WritePrivateProfileStringW(key.c_str(), L"Stamp", stamp.c_str(), file.c_str());
    }
    return hash;
}

void FinishTrack(Track& track, const Config& config) {
    track.stateSection = "Track_" + Hex64(Fnv1a64(track.stableKey));
    const std::wstring section = Utf8ToWide(track.stateSection);
    const std::wstring savedMode = ReadIniString(config.statePath, section.c_str(), L"Mode", L"");
    if (!savedMode.empty()) {
        track.mode = ParseTrackMode(savedMode, track.mode);
    }
    track.playbackMode = PlaybackModeText(track.mode);
}

std::string MetadataValue(const std::filesystem::path& path, const wchar_t* section,
                          const wchar_t* key, const wchar_t* fallback) {
    return WideToAnsi(ReadIniString(path, section, key, fallback));
}

void LoadExternalTracks(CatalogResult& result) {
    std::error_code error;
    std::filesystem::create_directories(result.config.tracksDirectory, error);
    std::vector<std::filesystem::path> files;
    std::filesystem::recursive_directory_iterator iterator(
        result.config.tracksDirectory,
        std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;
    for (; iterator != end; iterator.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }
        if (!iterator->is_regular_file(error)) continue;
        const std::wstring extension = LowerWide(iterator->path().extension().wstring());
        if (extension == L".mp3" || extension == L".wav") {
            files.push_back(iterator->path());
        }
    }
    std::sort(files.begin(), files.end(), [](const auto& left, const auto& right) {
        return LowerWide(left.wstring()) < LowerWide(right.wstring());
    });

    for (const auto& path : files) {
        if (result.tracks.size() >= result.config.maxCustomTracks) {
            result.warnings.push_back("custom track limit reached; remaining external files skipped");
            return;
        }
        std::string probeError;
        if (!AudioEngine::ProbeExternalFile(path, &probeError)) {
            result.warnings.push_back("skipped " + WideToUtf8(path.filename().wstring()) + ": " +
                                      probeError);
            continue;
        }

        std::filesystem::path sidecar = path;
        sidecar.replace_extension(L".ini");
        Track track;
        track.sourceKind = SourceKind::External;
        track.sourcePath = path;
        std::error_code relativeError;
        const auto relative = std::filesystem::relative(path, result.config.tracksDirectory,
                                                         relativeError);
        const std::wstring stablePath =
            relativeError ? path.filename().wstring() : relative.generic_wstring();
        track.stableKey = "external:" + LowerAscii(WideToUtf8(stablePath));
        track.title = MetadataValue(sidecar, L"Track", L"Title", path.stem().c_str());
        track.artist = MetadataValue(sidecar, L"Track", L"Artist", L"Custom");
        track.album = MetadataValue(sidecar, L"Track", L"Album", L"NFSMW EA TRAX Expansion");
        track.mode = ParseTrackMode(ReadIniString(sidecar, L"Track", L"Mode", L"ALL"),
                                    TrackMode::All);
        if (track.title.empty()) track.title = WideToAnsi(path.stem().wstring());
        if (track.artist.empty()) track.artist = "Custom";
        if (track.album.empty()) track.album = "NFSMW EA TRAX Expansion";
        FinishTrack(track, result.config);
        result.tracks.push_back(std::move(track));
    }
}

void LoadMusicSfxTracks(CatalogResult& result) {
    const std::filesystem::path mpf = result.config.musicSfxDirectory / L"MusicSFx.mpf";
    const std::filesystem::path mus = result.config.musicSfxDirectory / L"MusicSFx.mus";
    if (!std::filesystem::is_regular_file(mpf) && !std::filesystem::is_regular_file(mus)) {
        return;
    }
    if (!std::filesystem::is_regular_file(mpf) || !std::filesystem::is_regular_file(mus)) {
        result.warnings.push_back("MusicSFx import disabled: MPF/MUS pair is incomplete");
        return;
    }

    const MusicSfxInspection inspection = InspectMusicSfx(mpf);
    if (!inspection.supported) {
        result.warnings.push_back("MusicSFx import disabled: " + inspection.error);
        return;
    }

    std::size_t ordinal = 0;
    for (const auto& stream : inspection.importedStreams) {
        if (result.tracks.size() >= result.config.maxCustomTracks) {
            result.warnings.push_back("custom track limit reached; remaining MusicSFx tracks skipped");
            break;
        }
        ++ordinal;
        const std::wstring section = L"Subsong" + std::to_wstring(stream.subsong);
        const std::wstring fallbackTitle = L"UG2 EA TRAX " + std::to_wstring(ordinal);
        const TrackMode defaultMode = ordinal <= 6 ? TrackMode::FrontEnd : TrackMode::InGame;

        Track track;
        track.sourceKind = SourceKind::Underground2MusicSfx;
        track.sourcePath = mpf;
        track.subsong = stream.subsong;
        track.stableKey = "ug2-stock-musicsfx:subsong:" + std::to_string(stream.subsong);
        track.title = MetadataValue(result.config.musicSfxManifest, section.c_str(), L"Title",
                                    fallbackTitle.c_str());
        track.artist = MetadataValue(result.config.musicSfxManifest, section.c_str(), L"Artist",
                                     L"Need for Speed Underground 2");
        track.album = MetadataValue(result.config.musicSfxManifest, section.c_str(), L"Album",
                                    L"Need for Speed Underground 2");
        track.mode = ParseTrackMode(
            ReadIniString(result.config.musicSfxManifest, section.c_str(), L"Mode",
                          defaultMode == TrackMode::FrontEnd ? L"FE" : L"IG"),
            defaultMode);
        FinishTrack(track, result.config);
        result.tracks.push_back(std::move(track));
    }
    result.musicSfxImported = ordinal == 27;
}

void LoadLoudness(CatalogResult& result) {
    if (!ReadIniBool(result.config.iniPath, L"Loudness", L"Enabled", true)) return;
    const auto profile = result.config.modRoot / L"Loudness.ini";
    const float target = ReadIniFloat(profile, L"Loudness", L"TargetLUFS", NAN);
    const auto reference = (result.config.modRoot / L"..\\..\\SOUND\\PFDATA\\MW_Music.mpf").lexically_normal();
    auto referenceMus = reference;
    referenceMus.replace_extension(L".mus");
    const auto expectedMpf = WideToUtf8(ReadIniString(profile, L"Loudness", L"ReferenceMpfSHA256", L""));
    const auto expectedMus = WideToUtf8(ReadIniString(profile, L"Loudness", L"ReferenceMusSHA256", L""));
    if (ReadIniUInt(profile, L"Loudness", L"Version", 0) != 1 ||
        ReadIniUInt(profile, L"Loudness", L"ReferenceSongs", 0) != 26 ||
        !std::isfinite(target) || target < -40.0f || target > -5.0f ||
        expectedMpf.size() != 64 || expectedMus.size() != 64 ||
        Sha256File(reference) != expectedMpf || Sha256File(referenceMus) != expectedMus) {
        result.warnings.push_back("Loudness profile missing/invalid or MW reference changed; run Measure-Loudness.py; unity gain retained");
        return;
    }
    std::map<std::filesystem::path, std::string> hashes;
    auto hash = [&](const std::filesystem::path& path) -> std::string {
        auto found = hashes.find(path);
        if (found == hashes.end()) found = hashes.emplace(path, Sha256File(path)).first;
        return found->second;
    };
    auto apply = [&](Track& track) {
        std::string identity = hash(track.sourcePath);
        bool validHash = identity.size() == 64;
        if (track.sourceKind == SourceKind::Underground2MusicSfx) {
            auto mus = track.sourcePath;
            mus.replace_extension(L".mus");
            const auto musHash = hash(mus);
            validHash = validHash && musHash.size() == 64;
            identity += "_" + musHash;
        }
        const auto section = Utf8ToWide("Source_" + identity + "_" + std::to_string(track.subsong));
        const float integrated = ReadIniFloat(profile, section.c_str(), L"IntegratedLUFS", NAN);
        const float peak = ReadIniFloat(profile, section.c_str(), L"TruePeakDBTP", NAN);
        if (!validHash || !std::isfinite(integrated) || !std::isfinite(peak) ||
            integrated <= -70.0f || integrated > 5.0f || peak < -100.0f || peak > 12.0f) {
            result.warnings.push_back("Loudness measurement missing/stale: " +
                WideToUtf8(track.sourcePath.filename().wstring()) + " #" +
                std::to_string(track.subsong) + "; unity gain retained");
            return;
        }
        track.loudnessGainDb = LoudnessGainDb(target, integrated);
        track.loudnessGain = std::pow(10.0f, track.loudnessGainDb / 20.0f);
        track.loudnessMeasured = true;
    };
    for (auto& track : result.tracks) apply(track);
    for (auto& track : result.pursuitTracks) apply(track);
}

}  // namespace

Config LoadConfig(const std::filesystem::path& modRoot) {
    Config config;
    config.modRoot = modRoot;
    config.iniPath = modRoot / L"NFSMWEATraxExpansion.ini";
    config.tracksDirectory = modRoot / L"Tracks";
    config.musicSfxDirectory = modRoot / L"UG2MusicSFx";
    config.musicSfxManifest = modRoot / L"UG2MusicSFx" / L"MusicSFx.metadata.ini";
    config.statePath = modRoot / L"State.ini";
    config.logPath = modRoot / L"NFSMWEATraxExpansion.log";
    config.enablePursuit = ReadIniBool(config.iniPath, L"Pursuit", L"Enabled", false);
    config.pursuitDirectory = modRoot / L"Pursuit";
    config.enabled = ReadIniBool(config.iniPath, L"Main", L"Enabled", true);
    config.loadExternalTracks =
        ReadIniBool(config.iniPath, L"Main", L"LoadExternalTracks", true);
    config.loadMusicSfx = ReadIniBool(config.iniPath, L"Main", L"LoadMusicSFx", true);
    config.volumeMultiplier =
        std::clamp(ReadIniFloat(config.iniPath, L"Main", L"VolumeMultiplier", 1.0f), 0.0f,
                   2.0f);
    config.maxCustomTracks =
        std::clamp<std::uint32_t>(ReadIniUInt(config.iniPath, L"Main", L"MaxCustomTracks", 94),
                                  1, 94);
    return config;
}

CatalogResult LoadCatalog(const std::filesystem::path& modRoot, const bool allowMusicSfxCodec) {
    CatalogResult result;
    result.config = LoadConfig(modRoot);
    if (!result.config.enabled) {
        return result;
    }
    if (result.config.loadExternalTracks) {
        LoadExternalTracks(result);
    }
    if (result.config.loadMusicSfx && allowMusicSfxCodec &&
        result.tracks.size() < result.config.maxCustomTracks) {
        LoadMusicSfxTracks(result);
    }
    return result;
}

bool LoadNativeMusic(CatalogResult& catalog, std::string* error) {
    const auto cacheHash = [&](const std::filesystem::path& p) { return CachedBankHash(p, catalog.config.modRoot / L"Cache"); };
    const auto profile = catalog.config.modRoot / L"Cache" / L"NativeMusic.ini";
    const auto game = (catalog.config.modRoot / L"..\\..").lexically_normal();
    const auto mpfHash = WideToUtf8(ReadIniString(profile, L"NativeMusic", L"MpfSHA256", L""));
    const auto musHash = WideToUtf8(ReadIniString(profile, L"NativeMusic", L"MusSHA256", L""));
    if (ReadIniUInt(profile, L"NativeMusic", L"Version", 0) != 2 ||
        mpfHash.size() != 64 || musHash.size() != 64 ||
        cacheHash(catalog.config.modRoot / L"Cache" / L"EA_TRAX.mpf") != mpfHash ||
        cacheHash(catalog.config.modRoot / L"Cache" / L"EA_TRAX.mus") != musHash ||
        cacheHash(game / L"SOUND\\PFDATA\\MW_Music.mpf") != "15C7FDAA626940319A74965D4D68DB1507475BA743D574C5AA6DE214482E785B" ||
        cacheHash(game / L"SOUND\\PFDATA\\MW_Music.mus") != "BB191D34C4C3AC3B5BD33A9E02049A08DF13014E4FB5C8D7D07320ABAE1C5811") {
        if (error) *error = "NativeMusic.ini or native MPF/MUS identity mismatch; rebuild the native bank";
        return false;
    }
    const float bakedMultiplier = ReadIniFloat(profile, L"NativeMusic", L"VolumeMultiplier", NAN);
    if (!std::isfinite(bakedMultiplier) || std::abs(bakedMultiplier - catalog.config.volumeMultiplier) > 0.0001f) {
        catalog.warnings.push_back("VolumeMultiplier changed since native bank conversion; rebuild bank to update that source gain");
    }
    std::map<std::filesystem::path, std::string> hashes;
    auto hash = [&](const std::filesystem::path& path) {
        auto it = hashes.find(path);
        if (it == hashes.end()) it = hashes.emplace(path, cacheHash(path)).first;
        return it->second;
    };
    std::unordered_map<std::uint32_t, bool> used;
    unsigned sourceIndex = 0;
    for (auto* list : {&catalog.tracks, &catalog.pursuitTracks}) {
        for (auto& track : *list) {
            std::string identity = hash(track.sourcePath);
            if (track.sourceKind == SourceKind::Underground2MusicSfx) {
                auto mus = track.sourcePath; mus.replace_extension(L".mus");
                identity += "_" + hash(mus);
            }
            const auto section = Utf8ToWide("Source_" + identity + "_" + std::to_string(track.subsong) + "_" + std::to_string(sourceIndex++));
            const auto event = ReadIniUInt(profile, section.c_str(), L"EventID", 0);
            if (event < 0xE00000 || event >= 0xE10000 || used.count(event)) {
                if (error) *error = "Unregistered/changed native source: " + WideToUtf8(track.sourcePath.filename().wstring());
                return false;
            }
            used[event] = true;
            track.eventId = 0x01000000u | event;
        }
    }
    // Iterate the actual profile keys, then accept only known interactive IDs.
    wchar_t keys[8192]{};
    GetPrivateProfileStringW(L"PursuitControls", nullptr, L"", keys, 8192, profile.c_str());
    for (const wchar_t* key = keys; *key; key += std::wcslen(key) + 1) {
        wchar_t* end = nullptr;
        const auto original = static_cast<std::uint32_t>(std::wcstoul(key, &end, 16));
        const auto replacement = ReadIniUInt(profile, L"PursuitControls", key, 0);
        if (*end || !IsInteractiveMusicEvent(original) || replacement < 0xE10000 || replacement >= 0xE20000 || used.count(replacement)) {
            if (error) *error = "Invalid native pursuit control event map";
            return false;
        }
        used[replacement] = true;
        catalog.pursuitControlEvents[original] = replacement;
    }
    if (!catalog.pursuitTracks.empty() && catalog.pursuitControlEvents.size() != 43) {
        if (error) *error = "Incomplete native pursuit control event map";
        return false;
    }
    // Part IDs are node indices in the verified addon bank. Read its actual
    // bound so another appended score does not depend on the first two banks.
    std::ifstream bank(catalog.config.modRoot / L"Cache" / L"EA_TRAX.mpf", std::ios::binary);
    unsigned char header[20]{};
    if (!bank.read(reinterpret_cast<char*>(header), sizeof(header))) {
        if (error) *error = "Cannot read native bank node count";
        return false;
    }
    const unsigned nodeCount = header[18] | (static_cast<unsigned>(header[19]) << 8);
    for (const auto& score : kPursuitScores) {
        const auto* adaptiveSection = score.section;
        if (!catalog.config.enablePursuit || !ReadIniUInt(profile, adaptiveSection, L"Enabled", 0)) continue;
        if (catalog.pursuitControlEvents.size() != 43) {
            if (error) *error = "Incomplete adaptive pursuit control event map";
            return false;
        }
        Track track;
        track.title = score.title;
        track.loop = true;
        for (unsigned i = 0; i < track.nativeParts.size(); ++i) {
            const auto key = L"NativePart" + std::to_wstring(i);
            const auto part = ReadIniUInt(profile, adaptiveSection, key.c_str(), 0);
            if (part < 3681 || part >= nodeCount ||
                std::find(track.nativeParts.begin(), track.nativeParts.begin() + i, part) != track.nativeParts.begin() + i) {
                if (error) *error = "Invalid adaptive native part registration";
                return false;
            }
            track.nativeParts[i] = part;
        }
        for (unsigned i = 0; i < track.nativeIntensityParts.size(); ++i) {
            const auto key = L"NativeIntensityPart" + std::to_wstring(i);
            const auto part = ReadIniUInt(profile, adaptiveSection, key.c_str(), 0);
            if (part < 3681 || part >= nodeCount) {
                if (error) *error = "Missing native intensity part registration; install the matching phrase profile";
                return false;
            }
            track.nativeIntensityParts[i] = part;
        }
        // Other situation changes are native Part/C0 branches, not separate events.
        for (unsigned i : {1u, 5u, 7u, 8u, 9u, 11u}) {
            const auto key = L"MW" + (i < 10 ? std::wstring(L"0") : std::wstring()) + std::to_wstring(i);
            const auto event = ReadIniUInt(profile, adaptiveSection, key.c_str(), 0);
            if (event < 0xE20000 || event >= 0xE30000 || used.count(event) || (i == 1 && event != score.event)) {
                if (error) *error = "Invalid adaptive pursuit event registration";
                return false;
            }
            used[event] = true;
            track.adaptiveEvents[i] = event;
        }
        track.eventId = 0x01000000u | track.adaptiveEvents[1];
        catalog.pursuitTracks.push_back(std::move(track));
    }
    return true;
}

}  // namespace eatrax
