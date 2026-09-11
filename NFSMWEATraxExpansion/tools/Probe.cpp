#include "AudioEngine.h"
#include "Catalog.h"
#include "PlaylistSelector.h"
#include "PursuitPolicy.h"
#include "PursuitPlaylist.h"
#include "PursuitIntensity.h"
#include "Types.h"
#include "Utilities.h"
#include "VgmSource.h"
#include "Loudness.h"
#include "EaXaEncoder.h"
#include "TraxHudAspect.h"

#include <Windows.h>

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

int SelfTest() {
    using namespace eatrax;
    bool ok = true;
    for(const auto dims:std::array<std::array<int,2>,5>{{{640,480},{1920,1080},{3440,1440},{3840,1600},{5120,1440}}}) {
        for(float scale: {1.0f,0.92f}) {
            const float aspect=static_cast<float>(dims[0])/dims[1];
            const float shift=TraxHudOffset(dims[0],dims[1],true,true,scale);
            const float left=240.0f*aspect+(-376.0f+shift)*scale;
            const float reference=240.0f*(16.0f/9.0f)-376.0f*scale;
            ok=ok && std::abs(left-reference)<0.001f;
            ok=ok && TraxHudOffset(dims[0],dims[1],true,false,scale)==0.0f;
        }
    }
    ok=ok && TraxHudOffset(640,480,false,true)==0.0f;
    ok=ok && TraxHudOffset(3840,0,true,true)==0.0f;
    ok = ok && PursuitPressure(3, 6, 220) >= 60;
    ok = ok && PursuitPressure(1, 8, 200) >= 60;
    ok = ok && PursuitPressure(5, 4, 240) >= 60;
    ok = ok && PursuitPressure(5, 1, 60) < 48;
    ok = ok && PursuitPressure(20, 256, 1500) == 100;
    PursuitIntensityLatch pressureTest;
    ok = ok && pressureTest.Update(65, 0, 1000) >= 85;
    ok = ok && pressureTest.Update(30, 0, 2000) >= 85;
    ok = ok && pressureTest.Update(30, 0, 25999) >= 85;
    ok = ok && pressureTest.Update(30, 0, 26000) < 85;
    pressureTest.Update(65, 0, 30000);
    pressureTest.Update(30, 0, 31000);
    ok = ok && pressureTest.Update(50, 0, 55000) >= 85;
    ok = ok && pressureTest.Update(30, 0, 56000) >= 85;
    ok = ok && pressureTest.Update(30, 0, 80000) < 85;
    ok = ok && ParseTrackMode(L"OFF", TrackMode::All) == TrackMode::Off;
    ok = ok && ParseTrackMode(L"fe", TrackMode::All) == TrackMode::FrontEnd;
    ok = ok && ParseTrackMode(L"IG", TrackMode::All) == TrackMode::InGame;
    ok = ok && ParseTrackMode(L"all", TrackMode::Off) == TrackMode::All;
    ok = ok && Fnv1a32("hello") == 0x4F9F2CABu;
    ok = ok && Fnv1a64("hello") == 0xA430D84680AABD0Bull;
    ok = ok && ExpectedMusicSfxSubsongs().size() == 27;
    ok = ok && IsExpectedMusicSfxSubsong(2) && IsExpectedMusicSfxSubsong(40);
    ok = ok && !IsExpectedMusicSfxSubsong(1) && !IsExpectedMusicSfxSubsong(41);
    ok = ok && std::abs(LoudnessGainDb(-8.6f, -5.3f) + 3.3f) < 0.0001f;
    ok = ok && std::abs(LoudnessGainDb(-8.6f, -13.4f) - 4.8f) < 0.0001f;
    ok = ok && ApplyLoudnessGain(0.0f, 2.0f) == 0.0f;
    LoudnessLimiter limiter;
    limiter.Reset(48000);
    std::vector<float> limiterInput(4800 * 2, 0.05f);
    limiterInput[400 * 2] = 4.0f;
    limiterInput[400 * 2 + 1] = -2.0f;
    // Process irregular chunks to exercise lookahead and hold across callbacks.
    for (unsigned i = 0; i < 4800; i += 137) {
        limiter.Process(limiterInput.data() + i * 2, std::min(137u, 4800u - i));
    }
    ok = ok && limiterInput[0] == 0.0f;
    ok = ok && std::abs(limiterInput[240 * 2] - 0.05f) < 0.0001f;
    ok = ok && std::abs(limiterInput[640 * 2] - 0.891250938f) < 0.0001f;
    ok = ok && std::abs(limiterInput[640 * 2 + 1] + 0.445625469f) < 0.0001f;
    for (float sample : limiterInput) ok = ok && std::abs(sample) <= 0.891251f;
    unsigned groupCounts[7]{};
    for (unsigned random = 0; random < 700; ++random) ++groupCounts[SelectPursuitGroup(6, random)];
    for (unsigned count : groupCounts) ok = ok && count == 100;
    ok = ok && SelectPursuitGroup(0, 123) == 0;
    std::vector<Track> testTracks(3);
    testTracks[0].eventId = 0x01E20401;
    testTracks[1].eventId = 0x01E20001;
    testTracks[2].eventId = 0x01E20101;
    bool validTest = false;
    ok = ok && SelectTestPursuitGroup(L"  checkpointracetheme3  ", testTracks, 99, validTest) == 1 && validTest;
    ok = ok && SelectTestPursuitGroup(L"BattleTheme2", testTracks, 99, validTest) == 2 && validTest;
    ok = ok && SelectTestPursuitGroup(L"SprintRaceTheme1", testTracks, 99, validTest) == 3 && validTest;
    ok = ok && SelectTestPursuitGroup(L"Vanilla", testTracks, 99, validTest) == 0 && validTest;
    for (unsigned random = 0; random < 100; ++random)
        ok = ok && SelectTestPursuitGroup(L"Random", testTracks, random, validTest) == SelectPursuitGroup(3, random) && validTest;
    ok = ok && SelectTestPursuitGroup(L"typo", testTracks, 99, validTest) == 0 && !validTest;
    testTracks.erase(testTracks.begin());
    ok = ok && SelectTestPursuitGroup(L"CheckpointRaceTheme3", testTracks, 99, validTest) == 0 && !validTest;
    ok = ok && SelectTestPursuitGroup(L"BattleTheme2", testTracks, 99, validTest) == 1 && validTest;
    ok = ok && SelectTestPursuitGroup(L"Random", {}, 99, validTest) == 0 && validTest;
    ok = ok && IsInteractiveMusicEvent(0x016E7282) && IsInteractiveMusicEvent(0x006E7282);
    ok = ok && !IsInteractiveMusicEvent(0x01E12345) && !IsInteractiveMusicEvent(0xFFFFFFFF);
    std::vector<TrackMode> modes(69, TrackMode::Off);
    modes[0] = TrackMode::FrontEnd;
    modes[27] = TrackMode::All;
    modes[32] = TrackMode::FrontEnd;
    modes[64] = TrackMode::InGame;
    modes[68] = TrackMode::InGame;
    ok = ok && CountEligibleTracks(modes.data(), modes.size(), PlaybackContext::FrontEnd) == 3;
    ok = ok && CountEligibleTracks(modes.data(), modes.size(), PlaybackContext::InGame) == 3;
    for (std::uint32_t random = 0; random < 256; ++random) {
        const std::int32_t frontEnd = SelectTrackIndex(
            modes.data(), modes.size(), PlaybackContext::FrontEnd, random, 32);
        const std::int32_t inGame = SelectTrackIndex(
            modes.data(), modes.size(), PlaybackContext::InGame, random, 64);
        ok = ok && frontEnd >= 0 && TrackModeAllowsContext(
                                           modes[frontEnd], PlaybackContext::FrontEnd);
        ok = ok && inGame >= 0 &&
             TrackModeAllowsContext(modes[inGame], PlaybackContext::InGame);
        ok = ok && frontEnd != 32 && inGame != 64;
    }
    std::vector<TrackMode> none(80, TrackMode::Off);
    ok = ok && SelectTrackIndex(none.data(), none.size(), PlaybackContext::FrontEnd, 0, -1) ==
                   -1;
    std::cout << "self_test=" << (ok ? "PASS" : "FAIL") << '\n';
    return ok ? 0 : 1;
}

void WriteU16(std::ofstream& output, const std::uint16_t value) {
    const unsigned char bytes[] = {
        static_cast<unsigned char>(value & 0xFFu),
        static_cast<unsigned char>((value >> 8u) & 0xFFu),
    };
    output.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

void WriteU32(std::ofstream& output, const std::uint32_t value) {
    const unsigned char bytes[] = {
        static_cast<unsigned char>(value & 0xFFu),
        static_cast<unsigned char>((value >> 8u) & 0xFFu),
        static_cast<unsigned char>((value >> 16u) & 0xFFu),
        static_cast<unsigned char>((value >> 24u) & 0xFFu),
    };
    output.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

bool DumpMusicSfxWav(const std::filesystem::path& mpfPath, const int subsong,
                     const std::filesystem::path& outputPath, const double startSeconds,
                     const double durationSeconds, std::uint64_t* framesWritten,
                     std::string* error) {
    using namespace eatrax;
    VgmDecoder decoder;
    if (!decoder.Open(mpfPath, subsong, error)) return false;
    if (startSeconds < 0.0 || durationSeconds < 0.0) {
        if (error) *error = "start and duration must not be negative";
        return false;
    }

    const auto rate = static_cast<std::uint64_t>(decoder.SampleRate());
    const std::uint64_t start = std::min<std::uint64_t>(
        decoder.Length(), static_cast<std::uint64_t>(startSeconds * static_cast<double>(rate)));
    const std::uint64_t available = decoder.Length() - start;
    const std::uint64_t requested =
        durationSeconds == 0.0
            ? available
            : static_cast<std::uint64_t>(durationSeconds * static_cast<double>(rate));
    const std::uint64_t frameLimit = std::min(available, requested);
    const std::uint64_t byteCount64 =
        frameLimit * static_cast<std::uint64_t>(decoder.Channels()) * sizeof(std::int16_t);
    if (byteCount64 > std::numeric_limits<std::uint32_t>::max() - 36u) {
        if (error) *error = "WAV output exceeds the RIFF size limit";
        return false;
    }

    std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        if (error) *error = "could not create WAV output";
        return false;
    }
    const auto byteCount = static_cast<std::uint32_t>(byteCount64);
    output.write("RIFF", 4);
    WriteU32(output, 36u + byteCount);
    output.write("WAVEfmt ", 8);
    WriteU32(output, 16u);
    WriteU16(output, 1u);
    WriteU16(output, static_cast<std::uint16_t>(decoder.Channels()));
    WriteU32(output, static_cast<std::uint32_t>(decoder.SampleRate()));
    WriteU32(output, static_cast<std::uint32_t>(decoder.SampleRate() * decoder.Channels() * 2));
    WriteU16(output, static_cast<std::uint16_t>(decoder.Channels() * 2));
    WriteU16(output, 16u);
    output.write("data", 4);
    WriteU32(output, byteCount);

    decoder.Seek(start);
    std::vector<std::int16_t> samples(static_cast<std::size_t>(4096) * decoder.Channels());
    std::uint64_t written = 0;
    while (written < frameLimit) {
        const int request = static_cast<int>(
            std::min<std::uint64_t>(4096u, static_cast<std::uint64_t>(frameLimit - written)));
        const int decoded = decoder.ReadFrames(samples.data(), request);
        if (decoded < 0) {
            if (error) *error = "decode error while writing WAV";
            return false;
        }
        if (decoded == 0) break;
        output.write(reinterpret_cast<const char*>(samples.data()),
                     static_cast<std::streamsize>(decoded * decoder.Channels() * 2));
        if (!output) {
            if (error) *error = "write error while creating WAV";
            return false;
        }
        written += static_cast<std::uint64_t>(decoded);
    }
    if (written != frameLimit) {
        if (error) *error = "decoder ended before the requested WAV range";
        return false;
    }
    if (framesWritten) *framesWritten = written;
    return true;
}

void Usage() {
    std::cout << "NFSMWEATraxProbe commands:\n"
              << "  --self-test\n"
              << "  --inspect-musicsfx <MusicSFx.mpf> [frames_per_track]\n"
              << "  --dump-musicsfx <MusicSFx.mpf> <subsong> <output.wav> [start_seconds] "
                 "[duration_seconds]\n"
              << "  --probe-external <file.mp3|file.wav>\n"
              << "  --playback-lifecycle <file.mp3|file.wav> [timeout_ms]\n"
              << "  --catalog <mod_data_root>\n"
              << "  --load-plugin <NFSMWEATraxExpansion.asi> [wait_ms]\n";
}

}  // namespace

int wmain(const int argc, wchar_t** argv) {
    using namespace eatrax;
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) {
        Usage();
        return 2;
    }
    const std::wstring command = argv[1];
    if (command == L"--encode-eaxa" && argc == 6) {
        try {
            const auto start=std::stoull(argv[3]), frames=std::stoull(argv[4]);
            if(frames>0x7fffffff) throw std::runtime_error("PCM frame limit exceeded");
            std::cout << eax::EncodeRaw(argv[2],start,static_cast<uint32_t>(frames),argv[5]) << "\n";
            return 0;
        } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
    }
    if (command == L"--eaxa-version") {std::cout<<"1\n";return 0;}
    if (command == L"--self-test") return SelfTest();
    if (command == L"--pursuit-list" && argc >= 3) {
        auto catalog = LoadCatalog(argv[2]);
        std::string error;
        if (!LoadNativeMusic(catalog, &error)) { std::cerr << error; return 1; }
        for (unsigned run = 0; run < 20; ++run) {
            bool valid = true;
            const auto& ini = catalog.config.iniPath;
            const bool test = ReadIniBool(ini, L"Pursuit", L"TestMode", false);
            const auto mode = PursuitName(ReadIniString(ini, L"Pursuit", L"Mode", L"Random"));
            const auto random = 1664525u * (run + 17u) + 1013904223u;
            const auto selected = test
                ? SelectTestPursuitGroup(ReadIniString(ini, L"Pursuit", L"TestTrack", L"Vanilla"), catalog.pursuitTracks, run, valid)
                : SelectListedPursuitGroup(EnabledPursuitList(catalog), random);
            std::cout << (selected ? (catalog.pursuitTracks[selected - 1].eventId & 0xFFFFFFu) : 0) << "\n";
        }
        return 0;
    }
    if (command == L"--export-native-jobs" && argc >= 3) {
        auto catalog = LoadCatalog(argv[2]);
        auto quote = [](const std::string& s) {
            std::string out = "\"";
            for (const unsigned char c : s) {
                if (c == '"' || c == '\\') { out += '\\'; out += c; }
                else if (c < 32) { char escaped[8]; sprintf_s(escaped, "\\u%04x", c); out += escaped; }
                else out += c;
            }
            return out + '"';
        };
        std::cout << "[";
        for (size_t i = 0; i < catalog.tracks.size(); ++i) {
            const auto& t = catalog.tracks[i];
            if (i) std::cout << ',';
            std::cout << "{\"path\":" << quote(WideToUtf8(t.sourcePath.wstring()))
                << ",\"kind\":" << quote(t.sourceKind == SourceKind::External ? "external" : "musicsfx")
                << ",\"subsong\":" << t.subsong << "}";
        }
        std::cout << "]\n";
        return 0;
    }
    if ((command == L"--test-loudness" || command == L"--render-loudness") && argc >= 3) {
        auto catalog = LoadCatalog(argv[2], true);
        std::vector<Track> tracks = catalog.tracks;
        tracks.insert(tracks.end(), catalog.pursuitTracks.begin(), catalog.pursuitTracks.end());
        AudioEngine engine;
        std::string error;
        if (!engine.Initialize(&error, true)) { std::cerr << error; return 1; }
        if (command == L"--render-loudness" && argc >= 5) {
            const int index = _wtoi(argv[3]);
            if (index < 0 || static_cast<size_t>(index) >= tracks.size()) return 2;
            Track track = tracks[index];
            track.loop = false;
            const float volume = argc >= 6 ? static_cast<float>(_wtof(argv[5])) : 0.27f;
            if (!engine.Play(track, volume, &error)) { std::cerr << error; return 1; }
            std::ofstream output(std::filesystem::path(argv[4]), std::ios::binary);
            output.write("RIFF", 4); WriteU32(output, 0); output.write("WAVEfmt ", 8);
            WriteU32(output, 16); WriteU16(output, 3); WriteU16(output, 2);
            WriteU32(output, 48000); WriteU32(output, 48000 * 8);
            WriteU16(output, 8); WriteU16(output, 32); output.write("data", 4); WriteU32(output, 0);
            std::vector<float> pcm(4096 * 2);
            uint32_t bytes = 0;
            bool ended = false;
            for (unsigned block = 0; block < 48000u * 3600u / 4096u; ++block) {
                if (!engine.RenderFrames(pcm.data(), 4096)) return 1;
                output.write(reinterpret_cast<const char*>(pcm.data()), pcm.size() * sizeof(float));
                bytes += static_cast<uint32_t>(pcm.size() * sizeof(float));
                if (engine.HasEnded()) { ended = true; break; }
            }
            if (!ended || !output) return 1;
            output.seekp(4); WriteU32(output, 36 + bytes);
            output.seekp(40); WriteU32(output, bytes);
            std::cout << "render_loudness=PASS index=" << index << " measured=" << track.loudnessMeasured
                      << " gain_db=" << track.loudnessGainDb << " volume=" << volume << '\n';
            return 0;
        }
        unsigned tested = 0;
        for (auto track : tracks) {
            if (!track.loudnessMeasured) { std::cerr << "Unmeasured source\n"; return 1; }
            if (!engine.Play(track, 0.27f, &error)) { std::cerr << error; return 1; }
            for (float volume : {0.27f, 0.11f, 0.0f, 0.4f}) {
                engine.SetVolume(volume);
                if (std::abs(engine.OutputVolume() - volume * track.loudnessGain) > 0.00001f) return 1;
            }
            engine.Pause(); engine.SetVolume(0.2f); engine.Resume();
            if (std::abs(engine.OutputVolume() - 0.2f * track.loudnessGain) > 0.00001f) return 1;
            engine.Stop();
            track.loudnessMeasured = false;
            if (!engine.Play(track, 0.2f, &error) || std::abs(engine.OutputVolume() - 0.2f) > 0.00001f) return 1;
            engine.Stop();
            ++tested;
        }
        std::cout << "loudness_lifecycle=PASS tracks=" << tested << '\n';
        return tracks.empty() ? 1 : 0;
    }
    if (command == L"--inspect-pursuit" && argc >= 3) {
        unsigned count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(argv[2])) {
            if (entry.path().extension() != L".sps") continue;
            VgmDecoder decoder;
            std::string error;
            if (!decoder.Open(entry.path(), 0, &error)) {
                std::cout << "pursuit_decode=FAIL error=" << error << '\n'; return 1;
            }
            std::vector<std::int16_t> pcm(16384 * 2);
            std::uint64_t frames = 0;
            bool nonzero = false;
            for (;;) {
                int decoded = decoder.ReadFrames(pcm.data(), 16384);
                if (decoded < 0) { std::cout << "pursuit_decode=FAIL read\n"; return 1; }
                if (!decoded) break;
                frames += decoded;
                for (int i = 0; i < decoded * 2; ++i) nonzero |= pcm[i] != 0;
            }
            if (!nonzero || frames != decoder.Length()) {
                std::cout << "pursuit_decode=FAIL frames=" << frames << " expected=" << decoder.Length() << '\n';
                return 1;
            }
            decoder.Seek(0);
            if (decoder.ReadFrames(pcm.data(), 4096) != 4096) {
                std::cout << "pursuit_decode=FAIL rewind\n"; return 1;
            }
            std::cout << "pursuit_decode=PASS file=" << entry.path().filename().string()
                      << " rate=" << decoder.SampleRate() << " frames=" << frames << " rewind=PASS\n";
            ++count;
        }
        std::cout << "pursuit_decoded=" << count << '\n';
        return count ? 0 : 1;
    }
    if (command == L"--pursuit-playback" && argc >= 3) {
        AudioEngine audio;
        std::string error;
        Track track;
        track.sourceKind = SourceKind::PursuitSps;
        track.sourcePath = argv[2];
        track.loop = true;
        if (!audio.Initialize(&error) || !audio.Play(track, 0.0f, &error)) {
            std::cout << "pursuit_playback=FAIL error=" << error << '\n'; return 1;
        }
        const int before = audio.RemainingMilliseconds();
        Sleep(1100);
        const int after = audio.RemainingMilliseconds();
        audio.Pause();
        Sleep(100);
        const int paused = audio.RemainingMilliseconds();
        Sleep(400);
        const bool pauseOk = audio.IsPaused() && abs(paused - audio.RemainingMilliseconds()) <= 30;
        audio.Resume();
        Sleep(1100);
        const bool resumeOk = !audio.IsPaused() && audio.RemainingMilliseconds() < paused - 500;
        audio.Stop();
        const bool ok = before - after > 500 && pauseOk && resumeOk && !audio.IsActive();
        std::cout << "pursuit_playback=" << (ok ? "PASS" : "FAIL")
                  << " progress_ms=" << before - after << " pause=" << pauseOk
                  << " resume=" << resumeOk << " stopped=" << !audio.IsActive() << '\n';
        return ok ? 0 : 1;
    }
    if (command == L"--load-plugin" && argc >= 3) {
        HMODULE plugin = LoadLibraryW(argv[2]);
        if (plugin == nullptr) {
            std::cout << "plugin_load=FAIL win32=" << GetLastError() << '\n';
            return 1;
        }
        const DWORD wait = argc >= 4 ? static_cast<DWORD>(_wtoi(argv[3])) : 1500;
        Sleep(wait);
        std::cout << "plugin_load=PASS wait_ms=" << wait << '\n';
        return 0;
    }
    if (command == L"--probe-external" && argc >= 3) {
        std::string error;
        const bool ok = AudioEngine::ProbeExternalFile(argv[2], &error);
        std::cout << "external_probe=" << (ok ? "PASS" : "FAIL") << '\n';
        if (!ok) std::cout << "error=" << error << '\n';
        return ok ? 0 : 1;
    }
    if (command == L"--playback-lifecycle" && argc >= 3) {
        AudioEngine audio;
        std::string error;
        if (!audio.Initialize(&error)) {
            std::cout << "playback_lifecycle=FAIL stage=initialize error=" << error << '\n';
            return 1;
        }
        Track track;
        track.sourceKind = SourceKind::External;
        track.sourcePath = argv[2];
        track.title = "Lifecycle probe";
        if (!audio.Play(track, 0.0f, &error)) {
            std::cout << "playback_lifecycle=FAIL stage=play error=" << error << '\n';
            return 1;
        }
        const DWORD timeout = argc >= 4 ? static_cast<DWORD>(_wtoi(argv[3])) : 5000;
        const ULONGLONG deadline = GetTickCount64() + timeout;
        bool sawActive = false;
        while (GetTickCount64() < deadline) {
            sawActive = sawActive || audio.IsActive();
            if (audio.HasEnded()) break;
            Sleep(10);
        }
        const bool ended = audio.HasEnded();
        const int remaining = audio.RemainingMilliseconds();
        std::cout << "playback_lifecycle=" << (sawActive && ended && remaining == 0 ? "PASS" : "FAIL")
                  << " active_seen=" << (sawActive ? "YES" : "NO")
                  << " ended=" << (ended ? "YES" : "NO")
                  << " remaining_ms=" << remaining << '\n';
        return sawActive && ended && remaining == 0 ? 0 : 1;
    }
    if (command == L"--inspect-musicsfx" && argc >= 3) {
        const std::filesystem::path mpf = argv[2];
        const std::uint32_t frames = argc >= 4 ? static_cast<std::uint32_t>(_wtoi(argv[3])) : 0;
        const MusicSfxInspection inspection = InspectMusicSfx(mpf);
        std::cout << "musicsfx_supported=" << (inspection.supported ? "YES" : "NO") << '\n';
        std::cout << "subsong_count=" << inspection.subsongCount << '\n';
        std::cout << "imported_count=" << inspection.importedStreams.size() << '\n';
        if (!inspection.error.empty()) std::cout << "error=" << inspection.error << '\n';
        if (!inspection.supported) return 1;
        for (const auto& stream : inspection.importedStreams) {
            std::cout << "subsong=" << stream.subsong << " codec=" << stream.codec
                      << " channels=" << stream.channels << " rate=" << stream.sampleRate
                      << " samples=" << stream.sampleCount;
            if (frames > 0) {
                std::uint64_t checksum = 0;
                std::uint32_t decoded = 0;
                std::string error;
                const bool ok = DecodeMusicSfxPrefix(mpf, stream.subsong, frames, &checksum,
                                                     &decoded, &error);
                std::cout << " decode=" << (ok ? "PASS" : "FAIL") << " frames=" << decoded
                          << " fnv64=" << Hex64(checksum);
                if (!error.empty()) std::cout << " decode_error=" << error;
                if (!ok) {
                    std::cout << '\n';
                    return 1;
                }
            }
            std::cout << '\n';
        }
        return 0;
    }
    if (command == L"--dump-musicsfx" && argc >= 5) {
        const std::filesystem::path mpf = argv[2];
        const int subsong = _wtoi(argv[3]);
        const std::filesystem::path output = argv[4];
        const double startSeconds = argc >= 6 ? _wtof(argv[5]) : 0.0;
        const double durationSeconds = argc >= 7 ? _wtof(argv[6]) : 0.0;
        std::uint64_t frames = 0;
        std::string error;
        const bool ok = DumpMusicSfxWav(mpf, subsong, output, startSeconds, durationSeconds,
                                        &frames, &error);
        std::cout << "wav_dump=" << (ok ? "PASS" : "FAIL") << " subsong=" << subsong
                  << " frames=" << frames << '\n';
        if (!error.empty()) std::cout << "error=" << error << '\n';
        return ok ? 0 : 1;
    }
    if ((command == L"--catalog" || command == L"--native-catalog") && argc >= 3) {
        CatalogResult result = LoadCatalog(argv[2]);
        if (command == L"--native-catalog") {
            std::string error;
            if (!LoadNativeMusic(result, &error)) { std::cerr << error << '\n'; return 1; }
            std::cout << "native_bank=PASS controls=" << result.pursuitControlEvents.size() << '\n';
            for (const auto* list : {&result.tracks, &result.pursuitTracks})
                for (const auto& track : *list)
                    std::cout << "native_event=" << Hex32(track.eventId) << " source="
                              << WideToUtf8(track.sourcePath.filename().wstring()) << '\n';
        }
        std::cout << "enabled=" << (result.config.enabled ? "YES" : "NO") << '\n';
        std::cout << "track_count=" << result.tracks.size() << '\n';
        std::cout << "pursuit_external_groups=" << result.pursuitTracks.size() << '\n';
        std::cout << "warning_count=" << result.warnings.size() << '\n';
        for (const auto& warning : result.warnings) std::cout << "warning=" << warning << '\n';
        for (std::size_t index = 0; index < result.tracks.size(); ++index) {
            const Track& track = result.tracks[index];
            std::cout << "track=" << index << " source="
                      << (track.sourceKind == SourceKind::External ? "external" : "musicsfx")
                      << " subsong=" << track.subsong << " mode="
                      << static_cast<unsigned int>(track.mode) << " title=" << track.title << '\n';
            std::cout << "loudness_track=" << index << " measured=" << track.loudnessMeasured
                      << " gain_db=" << track.loudnessGainDb << '\n';
        }
        for (size_t index = 0; index < result.pursuitTracks.size(); ++index)
            std::cout << "loudness_pursuit=" << index << " measured=" << result.pursuitTracks[index].loudnessMeasured
                      << " gain_db=" << result.pursuitTracks[index].loudnessGainDb << '\n';
        return 0;
    }
    Usage();
    return 2;
}
