#include "Catalog.h"
#include "Logging.h"
#include "RuntimeHooks.h"
#include "Utilities.h"
#include "StartupWait.h"

#include <Windows.h>

#include <filesystem>
#include <cstring>
#include <memory>
#include <string>

namespace eatrax {
namespace {

constexpr wchar_t kPluginVersion[] = L"0.4.7";
constexpr std::uintmax_t kSupportedExecutableSize = 6033408;
constexpr char kSupportedExecutableSha256[] =
    "05873CF968E0BDD021C1E67FF22E9350D22E7F433F1D749323FA6AE27F504700";
// Same game image with the original NFSMW_icon.ico resources restored.
constexpr char kStockIconExecutableSha256[] =
    "6A1E41A449751241DE3653BE6BE9750A0B087E210011375C514872789CDE0BA8";

HMODULE g_module = nullptr;
HMODULE g_mpg123Lifetime = nullptr;
CatalogResult* g_catalogLifetime = nullptr;
HANDLE g_prepared = nullptr;
HANDLE g_cancel = nullptr;
bool g_activationAttempted = false;
bool g_addonActive = false;
bool g_restartRequired = false;

// Called on the game thread where MusicFlow sets its two bank filenames.
// The loader lock is no longer held here; the worker only prepares/validates data.
void __cdecl ConfigureBankNames(void* musicFlow) {
    if (!g_activationAttempted) {
        g_activationAttempted = true;
        startup::SelectLanguage(g_module);
        const bool prepared = startup::Wait(g_module, g_prepared, g_cancel);
        Log(LogLevel::Info,"Startup preparation finished: language=%ls prepared=%d restart_required=%d",
            startup::currentText->language,prepared,g_restartRequired);
        if (prepared && g_restartRequired) {
            Log(LogLevel::Info,"CACHE_GENERATED_RESTART_REQUIRED: no addon runtime hooks installed; closing after acknowledgement");
            MessageBoxW(GetActiveWindow(),startup::currentText->restartBody,
                (std::wstring(L"EA TRAX Expansion — ")+startup::currentText->restartTitle).c_str(),
                MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
            ExitProcess(0);
        }
        if (prepared && g_catalogLifetime) {
            std::string error;
            g_addonActive = InstallRuntimeHooks(g_catalogLifetime, &error);
            if (!g_addonActive) Log(LogLevel::Error, "Addon bank activation refused: %s", error.c_str());
        }
    }
    auto* object = static_cast<unsigned char*>(musicFlow);
    *reinterpret_cast<const char**>(object + 0x38) = g_addonActive ? "EA_TRAX.mpf" : "MW_Music.mpf";
    *reinterpret_cast<const char**>(object + 0x3c) = g_addonActive ? "EA_TRAX.mus" : "MW_Music.mus";
    if (g_addonActive) Log(LogLevel::Info, "Native bank files: scripts/NFSMWEATraxExpansion/Cache/EA_TRAX.mpf + EA_TRAX.mus (local generated cache)");
}

__declspec(naked) void BankNamesGate() {
    __asm {
        pushfd
        pushad
        push ecx
        call ConfigureBankNames
        add esp, 4
        popad
        popfd
        ret
    }
}

bool InstallBankNamesGate() {
    auto* address = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr)) + 0xDFA34;
    const unsigned char expected[] = {0xC7,0x41,0x38,0xA4,0x88,0x89,0x00,0xC7,0x41,0x3C,0x94,0x88,0x89,0x00};
    if (std::memcmp(address, expected, sizeof(expected))) return false;
    g_prepared = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_prepared) return false;
    g_cancel = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_cancel) return false;
    DWORD oldProtect = 0;
    if (!VirtualProtect(address, sizeof(expected), PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    unsigned char replacement[sizeof(expected)];
    std::memset(replacement, 0x90, sizeof(replacement)); replacement[0] = 0xE8;
    const auto relative = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(BankNamesGate) - reinterpret_cast<std::uintptr_t>(address + 5));
    std::memcpy(replacement + 1, &relative, 4);
    std::memcpy(address, replacement, sizeof(replacement));
    DWORD ignored; VirtualProtect(address, sizeof(expected), oldProtect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), address, sizeof(expected));
    return true;
}

std::filesystem::path ModulePath(const HMODULE module) {
    std::wstring buffer(1024, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(module, buffer.data(),
                                                static_cast<DWORD>(buffer.size()));
        if (length == 0) return {};
        if (length + 1 < buffer.size()) {
            buffer.resize(length);
            return std::filesystem::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
}

bool PrepareNativeCache(const std::filesystem::path& root) {
    if (WaitForSingleObject(g_cancel,0)==WAIT_OBJECT_0) return false;
    const auto progressPath=root / L"Cache" / L"Build.log";
    std::error_code sizeError;
    const auto oldSize=std::filesystem::file_size(progressPath,sizeError);
    const auto executable = root / L"Runtime" / L"BuildCache.exe";
    std::wstring command = L"\"" + executable.wstring() + L"\" --mod-root \"" + root.wstring() + L"\"";
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) return false;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) { CloseHandle(job); return false; }
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, root.c_str(), &startup, &process)) {
        Log(LogLevel::Error, "Cache builder could not start: win32=%lu", GetLastError());
        CloseHandle(job); return false;
    }
    bool ok = AssignProcessToJobObject(job, process.hProcess) != FALSE;
    if (ok) {
        Log(LogLevel::Info, "Preparing native cache; progress: Cache/Build.log");
        const bool resumed=ResumeThread(process.hThread) != static_cast<DWORD>(-1);
        HANDLE waits[]={process.hProcess,g_cancel};
        DWORD wait=WAIT_FAILED;
        const auto began=GetTickCount64();
        while(resumed && GetTickCount64()-began < 1740000) {
            wait=WaitForMultipleObjects(2,waits,FALSE,200);
            if(wait!=WAIT_TIMEOUT) break;
            HANDLE progress=CreateFileW(progressPath.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(progress!=INVALID_HANDLE_VALUE) {
                LARGE_INTEGER length{};
                if(GetFileSizeEx(progress,&length) && (sizeError || static_cast<ULONGLONG>(length.QuadPart)>oldSize)) {
                    LARGE_INTEGER offset{}; offset.QuadPart=length.QuadPart>1024 ? length.QuadPart-1024 : 0;
                    SetFilePointerEx(progress,offset,nullptr,FILE_BEGIN);
                    char buffer[1025]{}; DWORD count=0;
                    if(ReadFile(progress,buffer,1024,&count,nullptr) && count) {
                        std::string text(buffer,count);
                        const auto end=text.find_last_not_of("\r\n");
                        if(end!=std::string::npos) {
                            const auto begin=text.rfind('\n',end);
                            std::string line=text.substr(begin==std::string::npos ? 0 : begin+1,end-(begin==std::string::npos ? 0 : begin+1)+1);
                            const auto encoded=line.find("Encoded "); const auto reused=line.find("Reused ");
                            const auto resumedLine=line.find("Resumed ");
                            const auto pos=encoded!=std::string::npos ? encoded : reused!=std::string::npos ? reused : resumedLine;
                            if(pos!=std::string::npos) {
                                const auto first=line.find(' ',pos)+1; const auto colon=line.find(':',first);
                                startup::Update(startup::Stage::Generating,Utf8ToWide(line.substr(first,colon-first)));
                            } else if(line.find("Building native cache:")!=std::string::npos) {
                                startup::Update(startup::Stage::Generating);
                            } else if(line.find("COMPLETE:")!=std::string::npos) {
                                startup::Update(startup::Stage::Complete);
                            } else if(line.find("CACHE HIT:")!=std::string::npos) {
                                startup::Update(startup::Stage::Checking);
                            }
                        }
                    }
                }
                CloseHandle(progress);
            }
        }
        DWORD code = 1;
        ok = wait == WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess, &code) && (code == 0 || code == 2);
        g_restartRequired = ok && code == 2;
        if(wait==WAIT_OBJECT_0+1) Log(LogLevel::Info,"Cache preparation cancelled by user; stock bank selected");
        Log(ok ? LogLevel::Info : LogLevel::Error, "Native cache preparation %s exit=%lu", ok ? "complete" : "failed", code);
    } else TerminateProcess(process.hProcess, 1); // Our suspended child only.
    CloseHandle(job); // Also cancels child encoders on timeout or game exit.
    if(!ok) WaitForSingleObject(process.hProcess,5000);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    return ok;
}

DWORD WINAPI InitializePlugin(void*) {
    struct Completion { ~Completion() { if (g_prepared) SetEvent(g_prepared); } } completion;
    const std::filesystem::path modulePath = ModulePath(g_module);
    const auto directory = modulePath.parent_path();
    const std::filesystem::path modRoot = directory.filename() == L"NFSMWEATraxExpansion"
        ? directory : directory / L"NFSMWEATraxExpansion";
    const Config preliminaryConfig = LoadConfig(modRoot);
    InitializeLogging(preliminaryConfig.logPath);
    Log(LogLevel::Info, "Version %ls initializing", kPluginVersion);
    Log(LogLevel::Info, "Data root: %s", WideToUtf8(modRoot.wstring()).c_str());

    if (!preliminaryConfig.enabled) {
        Log(LogLevel::Info, "Disabled by configuration; stock bank selected without addon playback hooks");
        return 0;
    }

    const std::filesystem::path executable = ModulePath(nullptr);
    std::error_code sizeError;
    const std::uintmax_t executableSize = std::filesystem::file_size(executable, sizeError);
    std::string hashError;
    const std::string executableHash = Sha256File(executable, &hashError);
    Log(LogLevel::Info, "Executable: %s size=%llu sha256=%s",
        WideToUtf8(executable.filename().wstring()).c_str(),
        static_cast<unsigned long long>(executableSize), executableHash.c_str());
    const bool legacy = executableSize == kSupportedExecutableSize &&
        (executableHash == kSupportedExecutableSha256 || executableHash == kStockIconExecutableSha256);
    const bool nfspatcher = executableSize == 6029312 && (executableHash == "80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C" || executableHash == "B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06");
    if (sizeError || (!legacy && !nfspatcher)) {
        Log(LogLevel::Error,
            "Unsupported executable; fail-closed without hooks (expected size=%llu sha256=%s)",
            static_cast<unsigned long long>(kSupportedExecutableSize),
            kSupportedExecutableSha256);
        return 0;
    }

    if (!PrepareNativeCache(modRoot)) {
        Log(LogLevel::Error, "Cache preparation failed; vanilla bank retained. See Cache/Build.log");
        return 0;
    }
    if (g_restartRequired) {
        Log(LogLevel::Info,"Cache generated successfully; restart required before activating addon bank");
        return 0;
    }
    if(WaitForSingleObject(g_cancel,0)==WAIT_OBJECT_0) return 0;
    startup::Update(startup::Stage::Verifying);
    bool allowMusicSfxCodec = true;
    const std::filesystem::path musicSfxMpf =
        preliminaryConfig.musicSfxDirectory / L"MusicSFx.mpf";
    const std::filesystem::path musicSfxMus =
        preliminaryConfig.musicSfxDirectory / L"MusicSFx.mus";
    if (preliminaryConfig.enablePursuit ||
        (preliminaryConfig.loadMusicSfx && std::filesystem::is_regular_file(musicSfxMpf) &&
         std::filesystem::is_regular_file(musicSfxMus))) {
        const std::filesystem::path mpg123Path = modRoot / L"libmpg123-0.dll";
        g_mpg123Lifetime =
            LoadLibraryExW(mpg123Path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (g_mpg123Lifetime == nullptr) {
            allowMusicSfxCodec = false;
            Log(LogLevel::Warning,
                "MusicSFx import disabled: libmpg123-0.dll could not be loaded from the data "
                "root (win32=%lu); external MP3/WAV tracks remain available",
                static_cast<unsigned long>(GetLastError()));
        } else {
            Log(LogLevel::Info, "MusicSFx MPEG decoder loaded from: %s",
                WideToUtf8(mpg123Path.wstring()).c_str());
        }
    }

    auto catalog =
        std::make_unique<CatalogResult>(LoadCatalog(modRoot, allowMusicSfxCodec));
    std::string nativeError;
    if (!LoadNativeMusic(*catalog, &nativeError)) {
        Log(LogLevel::Error, "Native music registration refused: %s; no external-player fallback", nativeError.c_str());
        return 0;
    }
    for (const std::string& warning : catalog->warnings) {
        Log(LogLevel::Warning, "%s", warning.c_str());
    }
    std::uint32_t externalCount = 0;
    std::uint32_t musicSfxCount = 0;
    for (const Track& track : catalog->tracks) {
        if (track.sourceKind == SourceKind::External)
            ++externalCount;
        else
            ++musicSfxCount;
    }
    Log(LogLevel::Info, "Catalog ready: external=%u MusicSFx=%u total=%u", externalCount,
        musicSfxCount, static_cast<unsigned int>(catalog->tracks.size()));
    Log(LogLevel::Info, "Pursuit catalog: native_groups=1 external_groups=%u enabled=%d",
        static_cast<unsigned int>(catalog->pursuitTracks.size()), catalog->config.enablePursuit);
    for (const auto* list : {&catalog->tracks, &catalog->pursuitTracks}) {
        for (const auto& track : *list) {
            Log(LogLevel::Info, "Native bank source: event=%08X source=%s subsong=%d",
                track.eventId,
                WideToUtf8(track.sourcePath.filename().wstring()).c_str(), track.subsong);
        }
    }
    if (catalog->tracks.empty() && catalog->pursuitTracks.empty()) {
        Log(LogLevel::Info, "No valid custom sources; native EA TRAX remains untouched");
        return 0;
    }

    g_catalogLifetime = catalog.release();
    Log(LogLevel::Info,
        "Addon bank verified and prepared; waiting for native MusicFlow bank initialization.");
    return 0;
}

}  // namespace
}  // namespace eatrax

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        eatrax::g_module = module;
        DisableThreadLibraryCalls(module);
        if (!eatrax::InstallBankNamesGate()) return TRUE;
        HANDLE thread = CreateThread(nullptr, 0, eatrax::InitializePlugin, nullptr, 0, nullptr);
        if (thread != nullptr) CloseHandle(thread);
        else SetEvent(eatrax::g_prepared);
    } else if (reason == DLL_PROCESS_DETACH && reserved == nullptr) {
        eatrax::StopRuntimeAudio();
    }
    return TRUE;
}
