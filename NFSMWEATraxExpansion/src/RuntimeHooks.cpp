#include "RuntimeHooks.h"

#include "Logging.h"
#include "PlaylistSelector.h"
#include "PursuitPolicy.h"
#include "PursuitPlaylist.h"
#include "PursuitIntensity.h"
#include "Utilities.h"
#include "GameImports.h"
#include "TraxHudAspect.h"

#include <Windows.h>
#include <MinHook.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace eatrax {
namespace {

constexpr std::uintptr_t kPreferredBase = 0x00400000;
constexpr int kExpectedNativeTracks = 26;

// MW constructs bank paths in a short stack buffer. Keep its logical names short
// and redirect only our two files at the Win32 boundary, after native scheduling.
decltype(&CreateFileA) g_originalCreateFileA = nullptr;
decltype(&FindFirstFileA) g_originalFindFirstFileA = nullptr;
std::array<std::wstring, 2> g_cacheBankPaths;
std::array<std::string, 2> g_cacheBankPathsAnsi;
std::array<std::string, 2> g_logicalBankPaths;

int CacheBankIndex(const char* name) {
    if (!name) return -1;
    auto key = LowerAscii(name);
    std::replace(key.begin(), key.end(), '/', '\\');
    while (key.rfind(".\\", 0) == 0) key.erase(0, 2);
    for (int i = 0; i != 2; ++i) {
        const auto relative = i == 0 ? "sound\\pfdata\\ea_trax.mpf" : "sound\\pfdata\\ea_trax.mus";
        if (key == relative || key == g_logicalBankPaths[i]) return i;
    }
    return -1;
}

HANDLE WINAPI CacheCreateFileA(LPCSTR name, DWORD access, DWORD share,
    LPSECURITY_ATTRIBUTES security, DWORD disposition, DWORD flags, HANDLE fileTemplate) {
    const int index = CacheBankIndex(name);
    if (index < 0) return g_originalCreateFileA(name, access, share, security, disposition, flags, fileTemplate);
    const auto result = CreateFileW(g_cacheBankPaths[index].c_str(), access, share, security, disposition, flags, fileTemplate);
    const auto error = GetLastError();
    Log(LogLevel::Info, "Native cache file open: %s success=%d", index == 0 ? "MPF" : "MUS", result != INVALID_HANDLE_VALUE);
    SetLastError(error);
    return result;
}

HANDLE WINAPI CacheFindFirstFileA(LPCSTR name, LPWIN32_FIND_DATAA data) {
    const int index = CacheBankIndex(name);
    return g_originalFindFirstFileA(index < 0 ? name : g_cacheBankPathsAnsi[index].c_str(), data);
}

constexpr std::uintptr_t kJukeboxInit = 0x005AB450;
constexpr std::uintptr_t kJukeboxReserve = 0x005AB070;
constexpr std::uintptr_t kGameNew = 0x00652AD0;
constexpr std::uintptr_t kGameDelete = 0x00652B00;
constexpr std::uintptr_t kRefreshJukebox = 0x004DF330;
constexpr std::uintptr_t kSelectJukeboxTrack = 0x004F4CD0;
constexpr std::uintptr_t kEATraxEventHandler = 0x004F6C80;
constexpr std::uintptr_t kNativeTrackCount = 0x008F42C4;
constexpr std::uintptr_t kPlaylistPoolBase = 0x008F2080;
constexpr std::uintptr_t kJukeboxVector = 0x009123CC;
constexpr std::uintptr_t kJukeboxBegin = 0x009123D0;
constexpr std::uintptr_t kJukeboxEnd = 0x009123D4;
constexpr std::uintptr_t kJukeboxCapacity = 0x009123D8;
constexpr std::uintptr_t kCareerManager = 0x0091CF90;

constexpr std::uintptr_t kPathGetEvent = 0x0082BFD0;
constexpr std::uintptr_t kPathPlay = 0x0082B4A0;
constexpr std::uintptr_t kPathStop = 0x0082B5A0;
constexpr std::uintptr_t kPathPause = 0x0082A580;
constexpr std::uintptr_t kPathTimeRemaining = 0x0082B460;
constexpr std::uintptr_t kPathSetVolume = 0x0082B290;
constexpr std::uintptr_t kPauseChannel = 0x004B24F0;
constexpr std::uintptr_t kResumeChannel = 0x004B2510;
constexpr std::uintptr_t kClearAllEvents = 0x0082C730;

constexpr std::uintptr_t kStartPursuitClearCall = 0x004DF219;
constexpr std::uintptr_t kStartAmbienceClearCall = 0x004DF12E;
constexpr std::uintptr_t kStopMusic = 0x004DF2D0;
constexpr std::uintptr_t kMusicControl = 0x004DFB80;
constexpr std::uintptr_t kQueueMusicEvent = 0x0082C490;
constexpr std::uintptr_t kRunMusicEvents = 0x0082D560;
constexpr std::uintptr_t kNativeGain = 0x0081D5F2;
constexpr std::uintptr_t kAudioLock = 0x0081BCCB;
constexpr std::uintptr_t kAudioUnlock = 0x0081BCD9;

constexpr std::uintptr_t kSaveCave1 = 0x004DF373;
constexpr std::uintptr_t kSaveExit1 = 0x004DF37C;
constexpr std::uintptr_t kSaveCave2 = 0x0051E000;
constexpr std::uintptr_t kSaveExit2 = 0x0051E008;
constexpr std::uintptr_t kSaveCave3 = 0x005509D3;
constexpr std::uintptr_t kSaveExit3 = 0x005509DB;
constexpr std::uintptr_t kSaveCave4 = 0x004F4D32;
constexpr std::uintptr_t kSaveExit4 = 0x004F4D3B;
constexpr std::uintptr_t kSaveWriter = 0x0051E07C;
constexpr std::uintptr_t kSaveWriterExit = 0x0051E083;

constexpr std::uintptr_t kStopVtable = 0x008C5F3C;
constexpr std::uintptr_t kPauseVtable = 0x008C5F40;
constexpr std::uintptr_t kTimerVtable = 0x008C5F50;
constexpr std::uintptr_t kPlayVtable = 0x008C5F58;
constexpr std::uintptr_t kVolumeVtable = 0x008C5F60;

struct JukeboxTrack {
    char* title;
    char* album;
    char* artist;
    char* playbackMode;
    std::uint32_t eventId;
};
static_assert(sizeof(JukeboxTrack) == 20);

struct SaveTrack {
    std::uint32_t trackNumber;
    std::uint8_t mode;
    std::uint8_t reserved[3];
};
static_assert(sizeof(SaveTrack) == 8);

struct PlaylistPoolState {
    std::uint32_t enabledMask;
    std::uint32_t remainingMask;
    std::uint32_t enabledCount;
    std::int32_t previousTrack;
    std::int32_t selectedTrack;
    std::uint32_t selectionMode;
};
static_assert(sizeof(PlaylistPoolState) == 24);

struct PatchRecord {
    std::uintptr_t address = 0;
    std::vector<std::uint8_t> original;
};

using JukeboxInitFn = void(__thiscall*)(void*, int, int);
using ReserveFn = void(__thiscall*)(void*, std::uint32_t);
using GameNewFn = void*(__cdecl*)(std::size_t);
using GameDeleteFn = void(__cdecl*)(void*);
using RefreshJukeboxFn = void(__cdecl*)(int);
using SelectJukeboxTrackFn = void(__thiscall*)(void*);
using EATraxEventHandlerFn = void(__thiscall*)(void*, std::uint32_t);
using PathGetEventFn = void*(__cdecl*)(std::uint32_t, std::uint32_t);
using PathPlayFn = int(__thiscall*)(void*, int, std::uint32_t, int, int, std::uint32_t);
using ChannelFn = void(__thiscall*)(void*);
using ClearAllEventsFn = void(__cdecl*)(std::uint32_t);

std::uintptr_t g_moduleBase = 0;
CatalogResult* g_catalog = nullptr;
// EA TRAX stores the resource type in the high byte, while some Pathfinder
// callers pass only the 24-bit resource hash.  Key custom tracks by that
// canonical low-24 value so both call forms resolve to the same entry.
std::unordered_map<std::uint32_t, std::size_t> g_eventToTrack;
std::vector<TrackMode> g_customModes;
std::vector<TrackMode> g_combinedModes;
std::vector<std::unique_ptr<SaveTrack[]>> g_retainedShadows;
SaveTrack* g_shadowEntries = nullptr;
SaveTrack* g_nativeEntries = nullptr;
std::uintptr_t g_saveBase = 0;
std::uintptr_t g_saveExit1 = 0;
std::uintptr_t g_saveExit2 = 0;
std::uintptr_t g_saveExit3 = 0;
std::uintptr_t g_saveExit4 = 0;
std::uintptr_t g_saveWriterExit = 0;
std::vector<PatchRecord> g_permanentPatches;
std::mutex g_runtimeMutex;
constexpr std::uint32_t kNoPendingEvent = 0xFFFFFFFFu;
// Diagnostic correlation only; all handles, cursors and output remain native.
std::atomic<std::uint32_t> g_pendingEvent{kNoPendingEvent};
std::atomic<bool> g_savePatchesInstalled{false};
RuntimeStatus g_status;
std::uint32_t g_randomState = 0x6D2B79F5u;

JukeboxInitFn g_originalJukeboxInit = nullptr;
RefreshJukeboxFn g_originalRefreshJukebox = nullptr;
SelectJukeboxTrackFn g_originalSelectJukeboxTrack = nullptr;
PathGetEventFn g_originalGetEvent = nullptr;
PathPlayFn g_originalPlay = nullptr;
ClearAllEventsFn g_originalClearAllEvents = nullptr;
ChannelFn g_originalStopMusic = nullptr;
std::size_t g_pursuitGroup = 0;

bool g_pursuitStarted = false;
using MusicControlFn = void(__thiscall*)(void*, void*);
MusicControlFn g_originalMusicControl = nullptr;
using PartLookupFn = int(__cdecl*)(int, int);
PartLookupFn g_originalPartLookup = nullptr;
int g_nativeBodyPart = 4;
int g_lastIntensityBucket = -1;
std::uintptr_t Address(std::uintptr_t preferredAddress);
int g_adaptiveStatus = -1;
bool g_adaptiveEnding = false;
std::uint32_t g_adaptiveCrashStamp = 0;
PursuitIntensityLatch g_intensityLatch;
std::uint32_t g_lastPressureLog = 0;
int g_pressureReadStage = 0;
int g_lastPressureFailure = -1;

const Track* AdaptiveTrack() {
    if (!g_pursuitGroup || g_pursuitGroup > g_catalog->pursuitTracks.size()) return nullptr;
    const auto& track = g_catalog->pursuitTracks[g_pursuitGroup - 1];
    return track.adaptiveEvents[1] ? &track : nullptr;
}

int __cdecl PartLookupHook(int channel, int part) {
    int mapped = -1;
    if (const auto* track = AdaptiveTrack()) {
        if (part == static_cast<int>(track->nativeParts[0]) ||
            std::find(track->nativeIntensityParts.begin(), track->nativeIntensityParts.end(),
                      static_cast<std::uint32_t>(part)) != track->nativeIntensityParts.end()) mapped = g_nativeBodyPart;
        else if (part == static_cast<int>(track->nativeParts[1])) mapped = 2;
        else if (part == static_cast<int>(track->nativeParts[2])) mapped = 7;
        else {
            constexpr int bodyParts[] = {4, 1, 3, 2};
            for (unsigned i = 0; i < 4; ++i) {
                if (part == static_cast<int>(track->nativeParts[i + 3])) {
                    mapped = bodyParts[i];
                    // A deferred request is acknowledged only when its node actually starts.
                    g_nativeBodyPart = mapped;
                }
            }
        }
        static thread_local int lastPart = -999;
        static thread_local int lastMapped = -999;
        if (part != lastPart || mapped != lastMapped) {
            lastPart = part; lastMapped = mapped;
            Log(LogLevel::Info, "Adaptive part lookup: raw=%d mapped=%d", part, mapped);
        }
    }
    return mapped >= 0 ? mapped : g_originalPartLookup(channel, part);
}

// Same pursuit interface and status getter used by native music AI at 71B1AE.
// No cached gameplay-object pointer is retained across callbacks.
int ReadNativePursuitStatus(std::uint32_t* crashStamp) {
    __try {
        const auto manager = *reinterpret_cast<std::uintptr_t*>(Address(0x00993CC8));
        if (!manager) return -1;
        *crashStamp = *reinterpret_cast<std::uint32_t*>(manager + 0x220);
        const auto pursuit = *reinterpret_cast<std::uintptr_t*>(manager + 0x130);
        if (!pursuit) return -1;
        const auto vtable = *reinterpret_cast<std::uintptr_t*>(pursuit);
        const auto getter = *reinterpret_cast<std::uintptr_t*>(vtable + 0x114);
        if (getter < Address(0x00401000) || getter >= Address(0x00890000)) return -1;
        return reinterpret_cast<int(__thiscall*)(void*)>(getter)(reinterpret_cast<void*>(pursuit));
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}

// Exact retail interfaces, independently checked against the installed executable.
// No gameplay object is cached, and no pursuit/vehicle fields are modified.
bool ReadPursuitPressure(float* heat, int* cops, float* kmh, std::uint32_t* tick) {
    __try {
        g_pressureReadStage = 1;
        const auto manager = *reinterpret_cast<std::uintptr_t*>(Address(0x00993CC8));
        if (!manager) return false;
        const auto pursuit = *reinterpret_cast<std::uintptr_t*>(manager + 0x130);
        if (!pursuit) return false;
        g_pressureReadStage = 2;
        const auto pursuitMethods = *reinterpret_cast<std::uintptr_t**>(pursuit);
        if (pursuitMethods[3] != Address(0x00433C90) || pursuitMethods[0x114/4] != Address(0x00433B60)) return false;
        // AIPursuit::GetNumCops (433C90) and GetPursuitStatus (433B60).
        const int status = *reinterpret_cast<int*>(pursuit + 0x218);
        if (status != 0 && status != 1) { g_pressureReadStage = 0; return false; }
        g_pressureReadStage = 3;
        *cops = *reinterpret_cast<int*>(pursuit + 0x20);
        const auto count = *reinterpret_cast<unsigned*>(Address(0x0092CD24));
        const auto vehicles = *reinterpret_cast<std::uintptr_t**>(Address(0x0092CD1C));
        if (!vehicles || count > 512 || *cops < 0 || *cops > 256) return false;
        for (unsigned i = 0; i < count; ++i) {
            const auto vehicle = vehicles[i];
            if (!vehicle) continue;
            const auto methods = *reinterpret_cast<std::uintptr_t**>(vehicle);
            if (methods[22] != Address(0x006880B0) || methods[35] != Address(0x006881A0) || methods[43] != Address(0x00688230)) continue;
            // PVehicle getters: driver class 6880B0; speed in m/s 6881A0.
            if (*reinterpret_cast<int*>(vehicle + 0x94) != 0) continue;
            g_pressureReadStage = 4;
            // Perpetrator belongs to the AI behavior; query it before the physics object's COM table.
            const auto ai = *reinterpret_cast<std::uintptr_t*>(vehicle + 0x54);
            void* perp = nullptr;
            for (const auto component : {ai, vehicle}) {
                if (!component) continue;
                const auto object = *reinterpret_cast<void**>(component + 4);
                if (!object) continue;
                perp = reinterpret_cast<void*(__thiscall*)(void*, std::uintptr_t)>(Address(0x005D59F0))(object, Address(0x004037E0));
                if (perp) break;
            }
            if (!perp) return false;
            g_pressureReadStage = 5;
            const auto perpMethods = *reinterpret_cast<std::uintptr_t**>(perp);
            if (perpMethods[1] != Address(0x00409400)) return false;
            // IPerpetrator::GetHeat (409400) is a pure float getter at +1C.
            *heat = *reinterpret_cast<float*>(static_cast<unsigned char*>(perp) + 0x1c);
            *kmh = std::fabs(*reinterpret_cast<float*>(vehicle + 0x78)) * 3.6f;
            *tick = *reinterpret_cast<std::uint32_t*>(Address(0x00925AE8));
            g_pressureReadStage = 6;
            const bool valid = std::isfinite(*heat) && std::isfinite(*kmh) && *heat >= 0 && *heat <= 20 && *kmh <= 1500;
            if (valid) g_pressureReadStage = 0;
            return valid;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return false;
}

void __fastcall MusicControlHook(void* self, void*, void* message) {
    alignas(4) unsigned char adjusted[0x1c];
    float heat = 0, kmh = 0;
    int cops = 0;
    std::uint32_t tick = 0;
    if (AdaptiveTrack() && g_pursuitStarted && !g_adaptiveEnding &&
        ReadPursuitPressure(&heat, &cops, &kmh, &tick)) {
        g_lastPressureFailure = -1;
        // The original handler reads only +18 and retains no message pointer.
        std::memcpy(adjusted, message, sizeof(adjusted));
        const int native = *reinterpret_cast<int*>(adjusted + 0x18);
        const float pressure = PursuitPressure(heat, cops, kmh);
        const int corrected = g_intensityLatch.Update(pressure, native, tick);
        *reinterpret_cast<int*>(adjusted + 0x18) = corrected;
        g_originalMusicControl(self, adjusted);
        if (tick - g_lastPressureLog >= 8000u || g_lastIntensityBucket != (corrected <= 42 ? 0 : corrected <= 84 ? 1 : 2)) {
            g_lastPressureLog = tick;
            Log(LogLevel::Info, "Adaptive pressure: heat=%.2f cops=%d kmh=%.1f score=%.1f native=%d corrected=%d high=%d", heat, cops, kmh, pressure, native, corrected, g_intensityLatch.high);
        }
    } else {
        if (AdaptiveTrack() && g_pursuitStarted && !g_adaptiveEnding &&
            g_pressureReadStage && g_pressureReadStage != g_lastPressureFailure) {
            g_lastPressureFailure = g_pressureReadStage;
            Log(LogLevel::Warning, "Adaptive pressure unavailable: stage=%d; native intensity retained", g_pressureReadStage);
        }
        g_intensityLatch = {};
        g_originalMusicControl(self, message);
    }
    const auto* track = AdaptiveTrack();
    if (!track || !g_pursuitStarted || g_adaptiveEnding) return;
    const int intensity = *reinterpret_cast<unsigned char*>(static_cast<unsigned char*>(self) + 0x138);
    const int bucket = intensity <= 42 ? 0 : (intensity <= 84 ? 1 : 2);
    if (bucket != g_lastIntensityBucket) {
        g_lastIntensityBucket = bucket;
        Log(LogLevel::Info, "Adaptive native intensity: C0=%d band=%d part=%d", intensity, bucket,
            *reinterpret_cast<int*>(static_cast<unsigned char*>(self) + 0x160));
    }
    std::uint32_t crash = 0;
    const int status = ReadNativePursuitStatus(&crash);
    unsigned situation = 0;
    if (status == 3) { situation = 9; g_adaptiveEnding = true; }
    else if (status == 4) { situation = 8; g_adaptiveEnding = true; }
    else if (status == 2 && g_adaptiveStatus != 2) situation = 5;
    else if ((status == 0 || status == 1) && g_adaptiveStatus == 2) situation = 7;
    // The provisional crash cue reused the low body and restarted it after collisions.
    // Keep the current phrase; native collision/SpeedBreaker effects remain native.
    if (status >= 0) g_adaptiveStatus = status;
    g_adaptiveCrashStamp = crash;
    if (!situation) return;
    const auto event = 0x01000000u | track->adaptiveEvents[situation];
    const auto channelIndex = *reinterpret_cast<int*>(static_cast<std::uint8_t*>(self) + 0x13c);
    if (channelIndex < 0 || channelIndex > 1) return;
    const auto channel = *reinterpret_cast<int*>(static_cast<std::uint8_t*>(self) + 0x4c + channelIndex * 0x74);
    reinterpret_cast<int(__cdecl*)(int, std::uint32_t)>(Address(kQueueMusicEvent))(channel, event);
    reinterpret_cast<void(__cdecl*)(int, int)>(Address(kRunMusicEvents))(channel, 0);
    Log(LogLevel::Info, "Adaptive pursuit situation=MW%02u pursuit_status=%d event=%08X", situation, status, event);
}

std::uintptr_t Address(const std::uintptr_t preferredAddress) {
    return g_moduleBase + (preferredAddress - kPreferredBase);
}

bool BytesEqual(const std::uintptr_t address, const std::initializer_list<std::uint8_t> expected) {
    return std::memcmp(reinterpret_cast<const void*>(Address(address)), expected.begin(),
                       expected.size()) == 0;
}

bool WriteBytes(const std::uintptr_t runtimeAddress, const void* bytes, const std::size_t length) {
    DWORD oldProtection = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(runtimeAddress), length, PAGE_EXECUTE_READWRITE,
                        &oldProtection)) {
        return false;
    }
    std::memcpy(reinterpret_cast<void*>(runtimeAddress), bytes, length);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(runtimeAddress), length);
    DWORD ignored = 0;
    VirtualProtect(reinterpret_cast<void*>(runtimeAddress), length, oldProtection, &ignored);
    return true;
}

bool ApplyRelativePatch(const std::uintptr_t preferredAddress, const void* destination,
                        const std::initializer_list<std::uint8_t> expected, const std::uint8_t opcode,
                        std::vector<PatchRecord>* records, std::string* error) {
    const std::uintptr_t source = Address(preferredAddress);
    if (expected.size() < 5 || std::memcmp(reinterpret_cast<const void*>(source), expected.begin(),
                                           expected.size()) != 0) {
        if (error) *error = "byte guard failed at 0x" + Hex32(static_cast<std::uint32_t>(preferredAddress));
        return false;
    }
    PatchRecord record;
    record.address = source;
    record.original.assign(reinterpret_cast<const std::uint8_t*>(source),
                           reinterpret_cast<const std::uint8_t*>(source) + expected.size());
    std::vector<std::uint8_t> patch(expected.size(), 0x90);
    patch[0] = opcode;
    const auto displacement = static_cast<std::int32_t>(
        reinterpret_cast<std::uintptr_t>(destination) - (source + 5));
    std::memcpy(patch.data() + 1, &displacement, sizeof(displacement));
    if (!WriteBytes(source, patch.data(), patch.size())) {
        if (error) *error = "VirtualProtect/write failed";
        return false;
    }
    records->push_back(std::move(record));
    return true;
}

void RestorePatches(std::vector<PatchRecord>* records) {
    for (auto iterator = records->rbegin(); iterator != records->rend(); ++iterator) {
        WriteBytes(iterator->address, iterator->original.data(), iterator->original.size());
    }
    records->clear();
}

bool InstallCacheFileImports(std::string* error) {
    auto** create = FindGameImport(GetModuleHandleW(nullptr), "KERNEL32.dll", "CreateFileA");
    auto** find = FindGameImport(GetModuleHandleW(nullptr), "KERNEL32.dll", "FindFirstFileA");
    if (!create || !find || !*create || !*find) {
        if (error) *error = "Game file import entries are unavailable";
        return false;
    }
    g_originalCreateFileA = reinterpret_cast<decltype(g_originalCreateFileA)>(*create);
    g_originalFindFirstFileA = reinterpret_cast<decltype(g_originalFindFirstFileA)>(*find);
    std::vector<PatchRecord> pending;
    auto patch = [&](void** slot, void* replacement) {
        PatchRecord record;
        record.address = reinterpret_cast<std::uintptr_t>(slot);
        auto* bytes = reinterpret_cast<const std::uint8_t*>(slot);
        record.original.assign(bytes, bytes + sizeof(void*));
        if (!WriteBytes(record.address, &replacement, sizeof(replacement))) return false;
        pending.push_back(std::move(record));
        return true;
    };
    if (!patch(create, reinterpret_cast<void*>(&CacheCreateFileA)) ||
        !patch(find, reinterpret_cast<void*>(&CacheFindFirstFileA))) {
        RestorePatches(&pending);
        if (error) *error = "Game file import patch failed";
        return false;
    }
    g_permanentPatches.insert(g_permanentPatches.end(),
        std::make_move_iterator(pending.begin()), std::make_move_iterator(pending.end()));
    Log(LogLevel::Info, "Cache file imports installed at game boundary: CreateFileA=%p previous=%p FindFirstFileA=%p previous=%p",
        create, g_originalCreateFileA, find, g_originalFindFirstFileA);
    return true;
}

void AssignEventIds(JukeboxTrack**, const std::uint32_t) {
    g_eventToTrack.clear();
    for (std::size_t index = 0; index < g_catalog->tracks.size(); ++index)
        g_eventToTrack.emplace(g_catalog->tracks[index].eventId & 0x00FFFFFFu, index);
}

std::uint32_t NextRandomLocked() {
    std::uint32_t value = g_randomState;
    if (value == 0) value = 0x6D2B79F5u;
    value ^= value << 13u;
    value ^= value >> 17u;
    value ^= value << 5u;
    g_randomState = value;
    return value;
}

void SanitizeExtendedPlaylistPoolsLocked() {
    if (!g_status.tracksAppended || g_combinedModes.empty()) return;
    // The stock pool stores membership in one 32-bit mask and the selector
    // further limits that mask to 28 bits.  Above index 31, x86 shift counts
    // wrap and make FE/IG settings alias earlier tracks.  Once our list-based
    // selector is active these fields are availability signals only.
    auto* pools = reinterpret_cast<PlaylistPoolState*>(Address(kPlaylistPoolBase));
    const std::size_t frontEndCount = CountEligibleTracks(
        g_combinedModes.data(), g_combinedModes.size(), PlaybackContext::FrontEnd);
    const std::size_t inGameCount = CountEligibleTracks(
        g_combinedModes.data(), g_combinedModes.size(), PlaybackContext::InGame);
    const std::size_t counts[] = {frontEndCount, inGameCount};
    for (std::size_t index = 0; index < 2; ++index) {
        const std::uint32_t availability = counts[index] == 0 ? 0u : 1u;
        pools[index].enabledMask = availability;
        pools[index].remainingMask = availability;
        pools[index].enabledCount = static_cast<std::uint32_t>(counts[index]);
        if (availability == 0) pools[index].selectedTrack = -1;
    }
    Log(LogLevel::Info, "Extended playlist pools refreshed: FE=%u IG=%u total=%u",
        static_cast<unsigned int>(frontEndCount), static_cast<unsigned int>(inGameCount),
        static_cast<unsigned int>(g_combinedModes.size()));
}

void __cdecl RefreshJukeboxHook(const int clearHistory) {
    g_originalRefreshJukebox(clearHistory);
    std::lock_guard<std::mutex> lock(g_runtimeMutex);
    SanitizeExtendedPlaylistPoolsLocked();
}

void __fastcall SelectJukeboxTrackHook(void* self, void*) {
    if (self == nullptr) {
        g_originalSelectJukeboxTrack(self);
        return;
    }
    const int poolIndex =
        *reinterpret_cast<int*>(static_cast<std::uint8_t*>(self) + 0x150);
    if (poolIndex < 0 || poolIndex > 1) {
        g_originalSelectJukeboxTrack(self);
        return;
    }
    // This is the EATrax music-flow controller.  Retain it only for the
    // lifetime of the currently selected custom track so an unpaired
    // Pathfinder replay at EOF can be promoted back to the native high-level
    // next-song/HUD path.

    std::unique_lock<std::mutex> lock(g_runtimeMutex);
    if (!g_status.tracksAppended || g_combinedModes.empty()) {
        lock.unlock();
        g_originalSelectJukeboxTrack(self);
        return;
    }
    auto* pools = reinterpret_cast<PlaylistPoolState*>(Address(kPlaylistPoolBase));
    PlaylistPoolState& pool = pools[poolIndex];
    const PlaybackContext context =
        poolIndex == 0 ? PlaybackContext::FrontEnd : PlaybackContext::InGame;
    pool.selectedTrack = SelectTrackIndex(g_combinedModes.data(), g_combinedModes.size(), context,
                                          NextRandomLocked(), pool.previousTrack);
    Log(LogLevel::Info, "Extended playlist selected: context=%s index=%d previous=%d",
        poolIndex == 0 ? "FE" : "IG", pool.selectedTrack, pool.previousTrack);
}

extern "C" void __stdcall OnTrackModeChanged(const std::uint32_t trackIndex,
                                                const std::uint32_t rawMode) {
    if (rawMode > 3 || g_catalog == nullptr) return;
    const auto mode = static_cast<TrackMode>(rawMode);
    std::lock_guard<std::mutex> lock(g_runtimeMutex);
    if (trackIndex < g_combinedModes.size()) g_combinedModes[trackIndex] = mode;
    if (trackIndex < g_status.nativeTrackCount) {
        if (g_nativeEntries != nullptr) g_nativeEntries[trackIndex].mode = static_cast<std::uint8_t>(mode);
        return;
    }
    const std::size_t customIndex = trackIndex - g_status.nativeTrackCount;
    if (customIndex >= g_catalog->tracks.size() || customIndex >= g_customModes.size()) return;
    g_customModes[customIndex] = mode;
    const std::wstring section = Utf8ToWide(g_catalog->tracks[customIndex].stateSection);
    WritePrivateProfileStringW(section.c_str(), L"Mode", StateModeText(mode),
                               g_catalog->config.statePath.c_str());
    Log(LogLevel::Info, "Saved custom track mode: index=%u mode=%u", trackIndex, rawMode);
}

__declspec(naked) void SaveCave1Hook() {
    __asm {
        mov edx, dword ptr [g_saveBase]
        mov ecx, edx
        add edx, 324h
        jmp dword ptr [g_saveExit1]
    }
}

__declspec(naked) void SaveCave2Hook() {
    __asm {
        mov eax, dword ptr [g_saveBase]
        jmp dword ptr [g_saveExit2]
    }
}

__declspec(naked) void SaveCave3Hook() {
    __asm {
        mov esi, dword ptr [g_saveBase]
        jmp dword ptr [g_saveExit3]
    }
}

__declspec(naked) void SaveCave4Hook() {
    __asm {
        mov edx, dword ptr [g_saveBase]
        jmp dword ptr [g_saveExit4]
    }
}

__declspec(naked) void SaveWriterHook() {
    __asm {
        pushfd
        pushad
        movzx eax, byte ptr [esi + 4]
        push eax
        push ebx
        call OnTrackModeChanged
        popad
        popfd
        movzx eax, byte ptr [esi + 4]
        cmp eax, 3
        jmp dword ptr [g_saveWriterExit]
    }
}

bool InstallSavePatches(std::string* error) {
    if (g_savePatchesInstalled.load()) return true;
    std::vector<PatchRecord> pending;
    g_saveExit1 = Address(kSaveExit1);
    g_saveExit2 = Address(kSaveExit2);
    g_saveExit3 = Address(kSaveExit3);
    g_saveExit4 = Address(kSaveExit4);
    g_saveWriterExit = Address(kSaveWriterExit);
    if (!ApplyRelativePatch(kSaveCave1, SaveCave1Hook,
                            {0x8B, 0x50, 0x10, 0x81, 0xC2, 0x24, 0x03, 0x00, 0x00}, 0xE9,
                            &pending, error) ||
        !ApplyRelativePatch(kSaveCave2, SaveCave2Hook,
                            {0xA1, 0x90, 0xCF, 0x91, 0x00, 0x8B, 0x40, 0x10}, 0xE9, &pending,
                            error) ||
        !ApplyRelativePatch(kSaveCave3, SaveCave3Hook,
                            {0xA1, 0x90, 0xCF, 0x91, 0x00, 0x8B, 0x70, 0x10}, 0xE9, &pending,
                            error) ||
        !ApplyRelativePatch(kSaveCave4, SaveCave4Hook,
                            {0x8B, 0x0D, 0x90, 0xCF, 0x91, 0x00, 0x8B, 0x51, 0x10}, 0xE9,
                            &pending, error) ||
        !ApplyRelativePatch(kSaveWriter, SaveWriterHook,
                            {0x0F, 0xB6, 0x46, 0x04, 0x83, 0xF8, 0x03}, 0xE9, &pending,
                            error)) {
        RestorePatches(&pending);
        return false;
    }
    g_permanentPatches.insert(g_permanentPatches.end(),
                              std::make_move_iterator(pending.begin()),
                              std::make_move_iterator(pending.end()));
    g_savePatchesInstalled.store(true);
    return true;
}

void RollBackAllocatedTracks(const std::vector<JukeboxTrack*>& allocated) {
    auto gameDelete = reinterpret_cast<GameDeleteFn>(Address(kGameDelete));
    for (JukeboxTrack* track : allocated) gameDelete(track);
}

bool AppendCustomTracks(void* nativeSaveBase) {
    std::unique_lock<std::mutex> lock(g_runtimeMutex);
    if (g_catalog == nullptr || g_catalog->tracks.empty()) return false;

    auto* nativeCountPointer = reinterpret_cast<int*>(Address(kNativeTrackCount));
    auto** nativeBegin = *reinterpret_cast<JukeboxTrack***>(Address(kJukeboxBegin));
    auto** nativeEnd = *reinterpret_cast<JukeboxTrack***>(Address(kJukeboxEnd));
    const int nativeCount = *nativeCountPointer;
    if (nativeCount != kExpectedNativeTracks || nativeBegin == nullptr || nativeEnd == nullptr ||
        nativeEnd - nativeBegin != nativeCount) {
        Log(LogLevel::Error,
            "Additive registration refused: native list is not the expected 26-track vector");
        return false;
    }

    AssignEventIds(nativeBegin, static_cast<std::uint32_t>(nativeCount));
    const std::uint32_t combinedCount =
        static_cast<std::uint32_t>(nativeCount + g_catalog->tracks.size());
    auto shadow = std::unique_ptr<SaveTrack[]>(new (std::nothrow) SaveTrack[combinedCount]{});
    if (!shadow) {
        Log(LogLevel::Error, "Could not allocate the isolated EA TRAX state array");
        return false;
    }
    auto* actualNativeEntries = reinterpret_cast<SaveTrack*>(
        static_cast<std::uint8_t*>(nativeSaveBase) + 0x324);
    std::memcpy(shadow.get(), actualNativeEntries, sizeof(SaveTrack) * nativeCount);
    for (std::size_t index = 0; index < g_catalog->tracks.size(); ++index) {
        SaveTrack& entry = shadow[nativeCount + index];
        entry.trackNumber = static_cast<std::uint32_t>(nativeCount + index);
        entry.mode = static_cast<std::uint8_t>(g_customModes[index]);
    }

    std::vector<JukeboxTrack*> allocated;
    allocated.reserve(g_catalog->tracks.size());
    auto gameNew = reinterpret_cast<GameNewFn>(Address(kGameNew));
    for (Track& source : g_catalog->tracks) {
        auto* target = static_cast<JukeboxTrack*>(gameNew(sizeof(JukeboxTrack)));
        if (target == nullptr) {
            RollBackAllocatedTracks(allocated);
            Log(LogLevel::Error, "Game allocator failed while appending custom tracks");
            return false;
        }
        target->title = source.title.data();
        target->album = source.album.data();
        target->artist = source.artist.data();
        target->playbackMode = source.playbackMode.data();
        target->eventId = source.eventId;
        allocated.push_back(target);
    }

    auto reserve = reinterpret_cast<ReserveFn>(Address(kJukeboxReserve));
    reserve(reinterpret_cast<void*>(Address(kJukeboxVector)), combinedCount);
    auto** begin = *reinterpret_cast<JukeboxTrack***>(Address(kJukeboxBegin));
    auto** end = *reinterpret_cast<JukeboxTrack***>(Address(kJukeboxEnd));
    auto** capacity = *reinterpret_cast<JukeboxTrack***>(Address(kJukeboxCapacity));
    if (begin == nullptr || end == nullptr || capacity == nullptr ||
        static_cast<std::size_t>(capacity - end) < allocated.size()) {
        RollBackAllocatedTracks(allocated);
        Log(LogLevel::Error, "Game vector reserve did not provide the requested capacity");
        return false;
    }

    for (JukeboxTrack* track : allocated) *end++ = track;
    *reinterpret_cast<JukeboxTrack***>(Address(kJukeboxEnd)) = end;
    *nativeCountPointer = static_cast<int>(combinedCount);

    g_nativeEntries = actualNativeEntries;
    g_shadowEntries = shadow.get();
    g_combinedModes.clear();
    g_combinedModes.reserve(combinedCount);
    for (std::uint32_t index = 0; index < combinedCount; ++index) {
        const std::uint8_t rawMode = shadow[index].mode;
        g_combinedModes.push_back(rawMode <= 3 ? static_cast<TrackMode>(rawMode)
                                               : TrackMode::Off);
    }
    g_saveBase = reinterpret_cast<std::uintptr_t>(shadow.get()) - 0x324;
    std::string patchError;
    if (!InstallSavePatches(&patchError)) {
        *reinterpret_cast<JukeboxTrack***>(Address(kJukeboxEnd)) = begin + nativeCount;
        *nativeCountPointer = nativeCount;
        RollBackAllocatedTracks(allocated);
        g_shadowEntries = nullptr;
        g_combinedModes.clear();
        g_saveBase = 0;
        Log(LogLevel::Error, "Save-state isolation refused: %s", patchError.c_str());
        return false;
    }
    g_retainedShadows.push_back(std::move(shadow));

    g_status.tracksAppended = true;
    g_status.nativeTrackCount = nativeCount;
    g_status.customTrackCount = static_cast<std::uint32_t>(g_catalog->tracks.size());
    lock.unlock();
    reinterpret_cast<RefreshJukeboxFn>(Address(kRefreshJukebox))(1);
    Log(LogLevel::Info, "Appended %u custom tracks after %u native tracks",
        g_status.customTrackCount, g_status.nativeTrackCount);
    return true;
}

void __fastcall JukeboxInitHook(void* self, void*, const int firstArgument,
                                const int secondArgument) {
    g_originalJukeboxInit(self, firstArgument, secondArgument);
    if (firstArgument == 0 && !g_catalog->tracks.empty()) {
        AppendCustomTracks(self);
    }
}

void AcknowledgeNativeBodyWithoutRestart(int part) {
    // Same native MusicAI PartUpdate constructor and synchronous dispatcher as 4B6360.
    // This acknowledges AI state only; it does not change MusicFlow or the playing node.
    alignas(4) std::uint32_t message[5]{};
    const auto recipient = reinterpret_cast<std::uint32_t(__cdecl*)(const char*)>(Address(0x005CC240))(
        reinterpret_cast<const char*>(Address(0x00896E78)));
    reinterpret_cast<void*(__thiscall*)(void*, int)>(Address(0x004AE340))(message, part);
    message[1] = recipient;
    g_nativeBodyPart = part;
    reinterpret_cast<void(__thiscall*)(void*)>(Address(0x0065FAF0))(message);
}

void* __cdecl GetEventHook(const std::uint32_t eventId, const std::uint32_t mask) {
    const auto low = eventId & 0x00FFFFFFu;
    if (low == 0xC3FA91) {
        // Native stop is authoritative during scene teardown; never queue an outro here.
        g_pursuitGroup = 0;
        g_pursuitStarted = false;
        g_adaptiveEnding = false;
        g_adaptiveStatus = -1;
        Log(LogLevel::Info, "Native pursuit stop: preserving event=%08X", eventId);
        return g_originalGetEvent(eventId, mask);
    }
    std::uint32_t nativeEvent = eventId;
    const auto custom = g_eventToTrack.find(low);
    if (custom != g_eventToTrack.end()) {
        g_pursuitGroup = 0;
        g_pursuitStarted = false;
        Log(LogLevel::Info, "Native EA TRAX event=%08X title=%s", eventId,
            g_catalog->tracks[custom->second].title.c_str());
    } else if (g_pursuitGroup && IsInteractiveMusicEvent(low)) {
        if (!g_pursuitStarted) {
            nativeEvent = (eventId & 0xFF000000u) |
                (g_catalog->pursuitTracks[g_pursuitGroup - 1].eventId & 0xFFFFFFu);
            g_pursuitStarted = true;
            Log(LogLevel::Info, "Native pursuit start: group=%u event=%08X original=%08X title=%s",
                static_cast<unsigned>(g_pursuitGroup), nativeEvent, eventId,
                g_catalog->pursuitTracks[g_pursuitGroup - 1].title.c_str());
        } else if (const auto* adaptive = AdaptiveTrack()) {
            unsigned situation = 0;
            int nativeBody = -1;
            if (low == 0xC0DC6F || low == 0x90B6DC || low == 0x6E7282) nativeBody = 0;
            else if (low == 0xE9222F) nativeBody = 1;
            else if (low == 0x518C15) nativeBody = 2;
            else if (low == 0xB39639) nativeBody = 3;
            if (low == 0xE2814E) situation = 8;
            else if (low == 0x4C6876) situation = 9;
            else if (low == 0xD12570 || low == 0xD874B1 || low == 0xDF442E) situation = 11;
            if (nativeBody >= 0 && !g_adaptiveEnding) {
                constexpr int bodyParts[] = {4, 1, 3, 2};
                AcknowledgeNativeBodyWithoutRestart(bodyParts[nativeBody]);
                const auto control = g_catalog->pursuitControlEvents.find(low);
                if (control != g_catalog->pursuitControlEvents.end())
                    nativeEvent = (eventId & 0xFF000000u) | control->second;
                Log(LogLevel::Info, "Adaptive native body acknowledged without audio restart: part=%d event=%08X", bodyParts[nativeBody], nativeEvent);
            } else if (situation) {
                nativeEvent = (eventId & 0xFF000000u) | adaptive->adaptiveEvents[situation];
                g_adaptiveEnding = situation >= 8;
                Log(LogLevel::Info, "Adaptive pursuit native situation=MW%02u original=%08X event=%08X", situation, eventId, nativeEvent);
            } else {
                const auto control = g_catalog->pursuitControlEvents.find(low);
                if (control != g_catalog->pursuitControlEvents.end())
                    nativeEvent = (eventId & 0xFF000000u) | control->second;
            }
        } else {
            const auto control = g_catalog->pursuitControlEvents.find(low);
            if (control != g_catalog->pursuitControlEvents.end())
                nativeEvent = (eventId & 0xFF000000u) | control->second;
        }
    }
    g_pendingEvent.store(nativeEvent);
    void* result = g_originalGetEvent(nativeEvent, mask);
    if (!result && nativeEvent != eventId) {
        Log(LogLevel::Error, "Native pursuit event unavailable; restoring MW event=%08X", eventId);
        g_pursuitGroup = 0;
        g_pursuitStarted = false;
        g_pendingEvent.store(eventId);
        return g_originalGetEvent(eventId, mask);
    }
    if (!result && (nativeEvent & 0xFFFFFFu) >= 0xE00000u)
        Log(LogLevel::Error, "Native event missing: event=%08X mask=%08X", nativeEvent, mask);
    return result;
}

int __fastcall PlayHook(void* self, void*, int a1, std::uint32_t a2, int a3, int a4,
                        std::uint32_t a5) {
    const auto event = g_pendingEvent.exchange(kNoPendingEvent);
    const int result = g_originalPlay(self, a1, a2, a3, a4, a5);
    const auto low = event & 0xFFFFFFu;
    if (event != kNoPendingEvent && low >= 0xE00000u && low < 0xE30000u) {
        Log(LogLevel::Info, "Native Pathfinder play: event=%08X result=%d player=%p handle=%d sample_arg=%d",
            event, result, self, *reinterpret_cast<int*>(static_cast<std::uint8_t*>(self) + 4), a1);
    }
    return result;
}

void __cdecl TransitionClearAllHook(const std::uint32_t mask) {
    g_pursuitGroup = 0;
    g_pursuitStarted = false;
    g_originalClearAllEvents(mask);
}

void __cdecl PursuitClearAllHook(const std::uint32_t mask) {
    g_intensityLatch = {};
    g_lastPressureLog = 0;
    g_lastPressureFailure = -1;
    g_pressureReadStage = 0;
    g_pursuitGroup = 0;
    g_pursuitStarted = false;
    g_adaptiveStatus = -1;
    g_adaptiveEnding = false;
    g_adaptiveCrashStamp = 0;
    g_nativeBodyPart = 4;
    g_lastIntensityBucket = -1;
    g_originalClearAllEvents(mask);
    // Read only at the pursuit-start boundary: edits affect the next pursuit,
    // never interrupt or restart the phrase currently playing.
    const auto testTrack = ReadIniString(g_catalog->config.iniPath, L"Pursuit", L"TestTrack", L"Vanilla");
    const bool testMode = ReadIniBool(g_catalog->config.iniPath, L"Pursuit", L"TestMode", false);
    const auto mode = PursuitName(ReadIniString(g_catalog->config.iniPath, L"Pursuit", L"Mode", L"Random"));
    bool validTestTrack = true;
    if (g_catalog->config.enablePursuit) {
        std::lock_guard<std::mutex> lock(g_runtimeMutex);
        if (testMode) {
            g_pursuitGroup = SelectTestPursuitGroup(testTrack, g_catalog->pursuitTracks,
                                                 NextRandomLocked(), validTestTrack);
        } else {
            const auto enabled = EnabledPursuitList(*g_catalog);
            if (enabled.empty()) Log(LogLevel::Warning, "Pursuit list is empty; using Vanilla");
            if (mode != L"random" && mode != L"list") Log(LogLevel::Warning, "Invalid Pursuit Mode; using Random");
            g_pursuitGroup = SelectListedPursuitGroup(enabled, NextRandomLocked());
        }
    }
    if (!validTestTrack) Log(LogLevel::Warning, "Pursuit TestTrack invalid or unavailable: %ls; using Vanilla", testTrack.c_str());
    Log(LogLevel::Info, "Pursuit selection: TestMode=%d Mode=%ls requested=%ls enabled=%d selected=%s",
        testMode, mode.c_str(), testTrack.c_str(), g_catalog->config.enablePursuit,
        g_pursuitGroup ? g_catalog->pursuitTracks[g_pursuitGroup - 1].title.c_str() : "Vanilla");
    Log(LogLevel::Info, "Native pursuit selection: group=%u total_groups=%u",
        static_cast<unsigned>(g_pursuitGroup),
        static_cast<unsigned>(g_catalog->pursuitTracks.size() + 1));
}

void __fastcall StopMusicHook(void* self, void*) {
    g_pursuitGroup = 0;
    g_pursuitStarted = false;
    g_originalStopMusic(self);
}

bool ValidateTargetSurface(std::string* error) {
    const struct Guard {
        std::uintptr_t address;
        std::vector<std::uint8_t> bytes;
    } guards[] = {
        {0x00688230, {0x8B,0x41,0x54,0xC3}},
        {0x004AE340, {0x51,0x56,0x8D,0x44,0x24,0x04,0x50,0x8B,0xF1}},
        {0x0065FAF0, {0x56,0x57,0x8B,0xF9,0x51,0x8B,0x0F}},
        {0x005CC240, {0x8B,0x44,0x24,0x04}},
        {0x00409400, {0xD9,0x41,0x1C,0xC3}},
        {0x00433C90, {0x8B,0x41,0x20,0xC3}},
        {0x00433B60, {0x8B,0x81,0x18,0x02,0x00,0x00,0xC3}},
        {0x006881A0, {0xD9,0x41,0x78,0xC3}},
        {0x006880B0, {0x8B,0x81,0x94,0x00,0x00,0x00,0xC3}},
        {0x005D59F0, {0x83,0xEC,0x08,0x56,0x8B,0x71,0x08}},
        {0x004B6310, {0xA1,0xE8,0x21,0x91,0x00}},
        {kMusicControl, {0xA1,0xFC,0x86,0x8F,0x00,0x85,0xC0}},
        {0x0071B1AE, {0x8B,0x8F,0x30,0x01,0x00,0x00,0x85,0xC9}},
        {kStopMusic, {0x56, 0x8B, 0xF1, 0x68, 0x00, 0x00, 0x00, 0x0F}},
        {kNativeGain, {0x55, 0x8B, 0xEC, 0x51, 0x80, 0x3D, 0x88, 0x21, 0x9C, 0x00, 0x00}},
        {kAudioLock, {0xFF, 0x15, 0xE8, 0xBC, 0x90, 0x00}},
        {kAudioUnlock, {0x80, 0x2D, 0x89, 0x21, 0x9C, 0x00, 0x01}},
        {kJukeboxInit, {0x6A, 0xFF, 0x68, 0x77, 0x3D, 0x87, 0x00}},
        {kRefreshJukebox, {0xA1, 0x90, 0xCF, 0x91, 0x00, 0x8B, 0x48, 0x10}},
        {kSelectJukeboxTrack, {0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x86, 0x50, 0x01, 0x00, 0x00}},
        {kEATraxEventHandler, {0x55, 0x56, 0x8B, 0xF1, 0x8B, 0x86, 0x14, 0x01, 0x00, 0x00}},
        {kPathGetEvent, {0x55, 0x8B, 0xEC, 0xA1, 0x64, 0x29, 0x9C, 0x00}},
        {kPathPlay, {0x55, 0x8B, 0xEC, 0x56, 0x8B, 0xF1}},
        {kPathStop, {0x56, 0x57, 0x8B, 0xF1}},
        {kPathPause, {0x55, 0x8B, 0xEC, 0x56, 0x8B, 0xF1}},
        {kPathTimeRemaining, {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08}},
        {kPathSetVolume, {0x55, 0x8B, 0xEC, 0x53, 0x8B, 0x5D, 0x08}},
        {kPauseChannel, {0x56, 0x8B, 0xF1, 0xE8, 0xD3, 0x97, 0x36, 0x00}},
        {kResumeChannel, {0x56, 0x8B, 0xF1, 0xE8, 0xB3, 0x97, 0x36, 0x00}},
    };
    for (const auto& guard : guards) {
        if (std::memcmp(reinterpret_cast<const void*>(Address(guard.address)), guard.bytes.data(),
                        guard.bytes.size()) != 0) {
            if (error) *error = "hook byte guard failed at 0x" +
                                Hex32(static_cast<std::uint32_t>(guard.address));
            return false;
        }
    }

    const struct VtableGuard {
        std::uintptr_t slot;
        std::uintptr_t expected;
    } vtables[] = {{kStopVtable, kPathStop},       {kPauseVtable, kPathPause},
                   {kTimerVtable, kPathTimeRemaining}, {kPlayVtable, kPathPlay},
                   {kVolumeVtable, kPathSetVolume}};
    for (const auto& guard : vtables) {
        if (*reinterpret_cast<std::uintptr_t*>(Address(guard.slot)) != Address(guard.expected)) {
            if (error) *error = "audio vtable is already modified at 0x" +
                                Hex32(static_cast<std::uint32_t>(guard.slot));
            return false;
        }
    }
    return true;
}

template <typename T>
bool CreateHook(const std::uintptr_t address, void* replacement, T* original, std::string* error) {
    const MH_STATUS status = MH_CreateHook(reinterpret_cast<void*>(Address(address)), replacement,
                                           reinterpret_cast<void**>(original));
    if (status != MH_OK) {
        if (error) *error = std::string("MinHook create failed: ") + MH_StatusToString(status);
        return false;
    }
    return true;
}

// The native chyron selects TRAX_POS_1 for world HUD and POS_2/POS_3
// for frontend/garage. Shift the parent and all its animation positions by
// the same relative delta, removing our prior delta before native relayout.
using TraxLayoutFn = void(__thiscall*)(void*);
TraxLayoutFn g_originalTraxLayout = nullptr;
void* g_shiftedTraxRoot = nullptr;
void* g_shiftedTraxData = nullptr;
float g_traxShift = 0.0f;
bool g_traxWorldPosition = false;
float g_traxFeScale = 1.0f;
bool g_traxAutoFit = false;

struct TraxWindowSize { int width=0,height=0; };
BOOL CALLBACK FindTraxWindow(HWND window,LPARAM data) {
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    if(pid!=GetCurrentProcessId() || !IsWindowVisible(window) || GetWindow(window,GW_OWNER))return TRUE;
    RECT r{};if(!GetClientRect(window,&r))return TRUE;
    auto& size=*reinterpret_cast<TraxWindowSize*>(data);
    if(static_cast<long long>(r.right)*r.bottom>static_cast<long long>(size.width)*size.height)
        size={r.right,r.bottom};
    return TRUE;
}
void* TraxRoot() {
    return reinterpret_cast<void*(__cdecl*)(const char*,std::uint32_t)>(Address(0x00524850))(
        "EA_TRAX.fng",0xA1341735u);
}
void ShiftTraxRoot(void* root,float delta) {
    if(!root || delta==0.0f)return;
    const float shift[3]={delta,0.0f,0.0f};
    reinterpret_cast<void(__thiscall*)(void*,const float*,int)>(Address(0x005B8190))(root,shift,1);
}
void __fastcall TraxPositionMessage(void* instance,void*,std::uint32_t message,const char* package,int parameter) {
    const auto worldHash=reinterpret_cast<std::uint32_t(__cdecl*)(const char*)>(Address(0x005AF1C0))("TRAX_POS_1");
    g_traxWorldPosition=message==worldHash;
    reinterpret_cast<void(__thiscall*)(void*,std::uint32_t,const char*,int)>(Address(0x00516C90))(
        instance,message,package,parameter);
}
void __fastcall TraxLayoutHook(void* self,void*) {
    const bool hadShift=g_traxShift!=0.0f;
    void* root=TraxRoot();
    void* data=root?*reinterpret_cast<void**>(static_cast<unsigned char*>(root)+0x2c):nullptr;
    // Never touch a pointer retained from an unloaded/replaced package.
    if(root==g_shiftedTraxRoot && data==g_shiftedTraxData)ShiftTraxRoot(root,-g_traxShift);
    g_shiftedTraxRoot=nullptr;g_shiftedTraxData=nullptr;g_traxShift=0.0f;g_traxWorldPosition=false;
    g_originalTraxLayout(self);
    const bool world=g_traxWorldPosition && *reinterpret_cast<int*>(Address(0x00925E90))==6;
    if(!world) {
        if(hadShift)Log(LogLevel::Info,"EA TRAX HUD aspect assist: native menu/garage layout restored");
        return;
    }
    root=TraxRoot();if(!root || *reinterpret_cast<int*>(static_cast<unsigned char*>(root)+0x18)!=5)return;
    bool widescreen=false;
    const auto* select=reinterpret_cast<const unsigned char*>(Address(0x0058D883));
    if(select[0]==0xB0 && select[1]==1)widescreen=true; // Widescreen Fix's native selection override
    else {
        auto* manager=*reinterpret_cast<unsigned char**>(Address(kCareerManager));
        auto* profile=manager?*reinterpret_cast<unsigned char**>(manager+0x10):nullptr;
        if(profile)widescreen=profile[0x34]!=0;
    }
    TraxWindowSize size;EnumWindows(FindTraxWindow,reinterpret_cast<LPARAM>(&size));
    float scale=g_traxFeScale;
    if(g_traxAutoFit && size.height>0)scale*=std::min(1.0f,static_cast<float>(size.width)/size.height/(4.0f/3.0f));
    const float delta=TraxHudOffset(size.width,size.height,widescreen,world,scale);
    ShiftTraxRoot(root,delta);g_shiftedTraxRoot=root;
    g_shiftedTraxData=*reinterpret_cast<void**>(static_cast<unsigned char*>(root)+0x2c);g_traxShift=delta;
    static int lastWidth=0,lastHeight=0;static float lastDelta=9999.0f;
    if(lastWidth!=size.width || lastHeight!=size.height || lastDelta!=delta) {
        lastWidth=size.width;lastHeight=size.height;lastDelta=delta;
        Log(LogLevel::Info,"EA TRAX HUD aspect assist: world=1 size=%dx%d widescreen=%d scale=%.3f shift_x=%.3f",
            size.width,size.height,widescreen,scale,delta);
    }
}

bool InstallTraxHudAssist(std::string* error) {
    if(!ReadIniBool(g_catalog->config.iniPath,L"Main",L"HudAspectAssist",true))return true;
    const unsigned char layout[]={0x81,0xEC,0x08,0x02,0x00,0x00};
    const unsigned char position[]={0x83,0xEC,0x0C,0x56,0x8B,0xF1};
    const unsigned char lookup[]={0x8B,0x44,0x24,0x04,0x85,0xC0};
    if(std::memcmp(reinterpret_cast<void*>(Address(0x0058D670)),layout,sizeof(layout)) ||
       std::memcmp(reinterpret_cast<void*>(Address(0x005B8190)),position,sizeof(position)) ||
       std::memcmp(reinterpret_cast<void*>(Address(0x00524850)),lookup,sizeof(lookup))) {
        Log(LogLevel::Warning,"EA TRAX HUD assist disabled: unsupported layout hook surface");return true;
    }
    if(GetModuleHandleW(L"NFSMostWanted.WidescreenFix.asi")) {
        const auto ini=g_catalog->config.modRoot.parent_path()/L"NFSMostWanted.WidescreenFix.ini";
        g_traxFeScale=ReadIniFloat(ini,L"MAIN",L"FEScale",1.0f);
        g_traxAutoFit=ReadIniBool(ini,L"MAIN",L"AutoFitFE",true);
    }
    if(!CreateHook(0x0058D670,TraxLayoutHook,&g_originalTraxLayout,error))return false;
    std::vector<PatchRecord> pending;
    if(!ApplyRelativePatch(0x0058D86F,TraxPositionMessage,{0xE8,0x1C,0x94,0xF8,0xFF},0xE8,&pending,error))return false;
    g_permanentPatches.insert(g_permanentPatches.end(),std::make_move_iterator(pending.begin()),std::make_move_iterator(pending.end()));
    Log(LogLevel::Info,"EA TRAX HUD aspect assist enabled for native world position only");return true;
}

bool ApplyTransitionPatches(std::string* error) {
    std::vector<PatchRecord> pending;
    if (!ApplyRelativePatch(kStartPursuitClearCall, PursuitClearAllHook,
                            {0xE8, 0x12, 0xD5, 0x34, 0x00}, 0xE8, &pending, error) ||
        !ApplyRelativePatch(kStartAmbienceClearCall, TransitionClearAllHook,
                            {0xE8, 0xFD, 0xD5, 0x34, 0x00}, 0xE8, &pending, error)) {
        RestorePatches(&pending);
        return false;
    }
    g_permanentPatches.insert(g_permanentPatches.end(),
                              std::make_move_iterator(pending.begin()),
                              std::make_move_iterator(pending.end()));
    return true;
}

void TryLateAttach() {
    if (g_catalog->tracks.empty()) return;
    const int nativeCount = *reinterpret_cast<int*>(Address(kNativeTrackCount));
    auto** begin = *reinterpret_cast<JukeboxTrack***>(Address(kJukeboxBegin));
    auto** end = *reinterpret_cast<JukeboxTrack***>(Address(kJukeboxEnd));
    if (nativeCount != kExpectedNativeTracks || begin == nullptr || end - begin != nativeCount) return;
    void* manager = *reinterpret_cast<void**>(Address(kCareerManager));
    if (manager == nullptr) return;
    void* nativeSaveBase = *reinterpret_cast<void**>(static_cast<std::uint8_t*>(manager) + 0x10);
    if (nativeSaveBase != nullptr) {
        Log(LogLevel::Info, "Native jukebox was already initialized; using late additive attach");
        AppendCustomTracks(nativeSaveBase);
    }
}

}  // namespace

bool InstallRuntimeHooks(CatalogResult* catalog, std::string* error) {
    if (catalog == nullptr ||
        (catalog->tracks.empty() && catalog->pursuitTracks.empty())) {
        if (error) *error = "no custom tracks are available";
        return false;
    }
    g_moduleBase = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    g_catalog = catalog;
    g_customModes.clear();
    g_combinedModes.clear();
    for (const Track& track : catalog->tracks) g_customModes.push_back(track.mode);
    g_randomState ^= GetTickCount() ^ static_cast<std::uint32_t>(g_moduleBase);

    if (!ValidateTargetSurface(error)) return false;
    if (MH_Initialize() != MH_OK) {
        if (error) *error = "MinHook initialization failed";
        return false;
    }

    for (int i = 0; i != 2; ++i) {
        const auto name = i == 0 ? L"EA_TRAX.mpf" : L"EA_TRAX.mus";
        g_cacheBankPaths[i] = (catalog->config.modRoot / L"Cache" / name).lexically_normal().wstring();
        g_cacheBankPathsAnsi[i] = WideToAnsi(g_cacheBankPaths[i]);
        g_logicalBankPaths[i] = LowerAscii(WideToAnsi((catalog->config.modRoot / L"..\\..\\SOUND\\PFDATA" / name).lexically_normal().wstring()));
    }
    if (!CreateHook(kStopMusic, StopMusicHook, &g_originalStopMusic, error) ||
        !CreateHook(0x004B6310, PartLookupHook, &g_originalPartLookup, error) ||
        !CreateHook(kMusicControl, MusicControlHook, &g_originalMusicControl, error) ||
        !CreateHook(kJukeboxInit, JukeboxInitHook, &g_originalJukeboxInit, error) ||
        !CreateHook(kRefreshJukebox, RefreshJukeboxHook, &g_originalRefreshJukebox, error) ||
        !CreateHook(kSelectJukeboxTrack, SelectJukeboxTrackHook, &g_originalSelectJukeboxTrack, error) ||
        !CreateHook(kPathGetEvent, GetEventHook, &g_originalGetEvent, error) ||
        !CreateHook(kPathPlay, PlayHook, &g_originalPlay, error)) {
        MH_Uninitialize();
        return false;
    }
    g_originalClearAllEvents = reinterpret_cast<ClearAllEventsFn>(Address(kClearAllEvents));
    if (!ApplyTransitionPatches(error) || !InstallTraxHudAssist(error)) {
        RestorePatches(&g_permanentPatches);
        MH_Uninitialize();
        return false;
    }
    const MH_STATUS enableStatus = MH_EnableHook(MH_ALL_HOOKS);
    if (enableStatus != MH_OK) {
        RestorePatches(&g_permanentPatches);
        MH_Uninitialize();
        if (error) *error = std::string("MinHook enable failed: ") + MH_StatusToString(enableStatus);
        return false;
    }
    g_status.hooksInstalled = true;
    if (!InstallCacheFileImports(error)) {
        g_status.hooksInstalled = false;
        MH_DisableHook(MH_ALL_HOOKS);
        RestorePatches(&g_permanentPatches);
        MH_Uninitialize();
        return false;
    }
    Log(LogLevel::Info, "Native-only runtime hooks installed; additional pursuit groups=%u",
        static_cast<unsigned>(catalog->pursuitTracks.size()));
    TryLateAttach();
    return true;
}

void StopRuntimeAudio() {
    // The game's own engine owns every sound and performs its normal shutdown.
    g_pursuitGroup = 0;
    g_pursuitStarted = false;
}

RuntimeStatus GetRuntimeStatus() {
    std::lock_guard<std::mutex> lock(g_runtimeMutex);
    return g_status;
}

}  // namespace eatrax
