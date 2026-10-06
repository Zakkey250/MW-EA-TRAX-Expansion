// Included inside RuntimeHooks.cpp's anonymous namespace, after hook helpers.
// All callbacks execute on the game's native update/message path. No worker thread
// calls game functions, and no gameplay pursuit/heat/AI field is written.
using HeatTryStartFn = bool(__thiscall*)(void*);
using HeatMusicStateFn = int(__thiscall*)(void*);
using HeatUpdateFn = void(__thiscall*)(void*, float);
HeatTryStartFn g_originalHeatTryStart = nullptr;
HeatMusicStateFn g_originalHeatMusicState = nullptr;
HeatUpdateFn g_originalHeatUpdate = nullptr;
ChannelFn g_originalCopIntro = nullptr;

struct HeatSnapshot {
    bool roaming = false;
    bool valid = false;
    bool nativeMusicPursuitReady = false;
    float heat = 0;
    std::uintptr_t pursuit = 0;
    int status = -1;
    std::uint32_t tick = 0;
};

HeatSnapshot ReadHeatSnapshot() {
    HeatSnapshot s;
    __try {
        s.tick = *reinterpret_cast<std::uint32_t*>(Address(0x00925AE8));
        const auto race = *reinterpret_cast<std::uintptr_t*>(Address(0x0091E000));
        s.roaming = *reinterpret_cast<int*>(Address(0x00925E90)) == 6 && race &&
            *reinterpret_cast<int*>(race + 0x1960) == 0;
        const auto manager = *reinterpret_cast<std::uintptr_t*>(Address(0x00993CC8));
        if (manager) {
            const int musicState = *reinterpret_cast<int*>(manager + 0x1D4);
            s.nativeMusicPursuitReady = musicState == 0 || musicState == 1;
            s.pursuit = *reinterpret_cast<std::uintptr_t*>(manager + 0x130);
            if (s.pursuit) {
                const auto methods = *reinterpret_cast<std::uintptr_t**>(s.pursuit);
                if (methods[0x114 / 4] != Address(0x00433B60)) return s;
                s.status = *reinterpret_cast<int*>(s.pursuit + 0x218);
            }
        }
        if (!s.roaming) return s;
        // Unlike adaptive intensity, this must also work before the pursuit is
        // attached and during cooldown, and with no external pursuit bank.
        const auto count = *reinterpret_cast<unsigned*>(Address(0x0092CD24));
        const auto vehicles = *reinterpret_cast<std::uintptr_t**>(Address(0x0092CD1C));
        if (!vehicles || count > 512) return s;
        for (unsigned i = 0; i < count; ++i) {
            const auto vehicle = vehicles[i];
            if (!vehicle) continue;
            const auto methods = *reinterpret_cast<std::uintptr_t**>(vehicle);
            if (methods[22] != Address(0x006880B0) || methods[43] != Address(0x00688230)) continue;
            if (*reinterpret_cast<int*>(vehicle + 0x94) != 0) continue;
            const std::uintptr_t components[] = {
                *reinterpret_cast<std::uintptr_t*>(vehicle + 0x54), vehicle};
            for (auto component : components) {
                if (!component) continue;
                const auto object = *reinterpret_cast<void**>(component + 4);
                if (!object) continue;
                const auto perp = reinterpret_cast<void*(__thiscall*)(void*, std::uintptr_t)>(
                    Address(0x005D59F0))(object, Address(0x004037E0));
                if (!perp) continue;
                const auto pm = *reinterpret_cast<std::uintptr_t**>(perp);
                if (pm[1] != Address(0x00409400)) return s;
                s.heat = *reinterpret_cast<float*>(static_cast<unsigned char*>(perp) + 0x1c);
                s.valid = std::isfinite(s.heat) && s.heat >= 1 && s.heat <= 20;
                return s;
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { s.valid = false; }
    return s;
}

bool ObserveHeat(bool startBoundary = false, void* musicFlow = nullptr) {
    if (!g_catalog->config.heatKeeping) return false;
    const auto s = ReadHeatSnapshot();
    const bool wasHolding = g_heatKeeping.holding;
    const bool wasExitTail = g_heatKeeping.exitTail;
    const bool wasNativeOwned = g_heatKeeping.nativeOwned;
    const bool nativePlaying = musicFlow &&
        *reinterpret_cast<int*>(static_cast<unsigned char*>(musicFlow) + 0x148) == 1;
    const bool result = g_heatKeeping.Observe(true, s.roaming, s.valid,
        s.heat, s.pursuit, s.status, s.tick, startBoundary, s.nativeMusicPursuitReady, nativePlaying);
    if (wasHolding != result) {
        Log(LogLevel::Info, "Heat keeping: hold=%d heat=%.3f status=%d valid=%d roaming=%d escalated=%d musicReady=%d",
            result, s.heat, s.status, s.valid, s.roaming, g_heatKeeping.escalated, s.nativeMusicPursuitReady);
    }
    if (wasExitTail != g_heatKeeping.exitTail)
        Log(LogLevel::Info, "Heat keeping: escape music tail=%d status=%d musicReady=%d",
            g_heatKeeping.exitTail, s.status, s.nativeMusicPursuitReady);
    if (wasNativeOwned != g_heatKeeping.nativeOwned)
        Log(LogLevel::Info, "Heat keeping: native pursuit owns session=%d status=%d roaming=%d",
            g_heatKeeping.nativeOwned, s.status, s.roaming);
    return result;
}

void* FindCurrentIntroCamera(int view) {
    __try {
        const auto count = *reinterpret_cast<unsigned*>(Address(0x00911308));
        const auto cameras = *reinterpret_cast<std::uintptr_t**>(Address(0x00911300));
        if (!cameras || count > 16) return nullptr;
        for (unsigned i = 0; i < count; ++i) {
            const auto camera = cameras[i];
            if (camera && *reinterpret_cast<int*>(camera + 8) == view)
                return reinterpret_cast<void*>(camera);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    return nullptr;
}

void ReplayDeferredIntro() {
    const int view = g_heatKeeping.TakeIntro();
    if (view < 0) return;
    auto* camera = FindCurrentIntroCamera(view);
    // Preserve the native cinematic option, NIS and photo-mode restrictions.
    const bool permitted = camera &&
        *reinterpret_cast<int*>(Address(0x008EE348)) <= 0 &&
        !*reinterpret_cast<int*>(Address(0x009885C8)) &&
        !*reinterpret_cast<unsigned char*>(Address(0x00911055)) &&
        reinterpret_cast<bool(__cdecl*)()>(Address(0x00469E30))();
    if (permitted) {
        g_originalCopIntro(camera);
        Log(LogLevel::Info, "Heat keeping: HEAT 4+ deferred native intro dispatched view=%d", view);
    } else {
        Log(LogLevel::Info, "Heat keeping: deferred intro discarded; native camera unavailable/disabled");
    }
}

void __fastcall HeatUpdateHook(void* self, void*, float elapsed) {
    ObserveHeat(false, self);
    ReplayDeferredIntro();
    g_originalHeatUpdate(self, elapsed);
    PollTrackPresentation();
    ObservePlayback(self);
}

bool __fastcall HeatTryStartHook(void* self, void*) {
    // This gate precedes BOTH the event-id reset and StartPursuit. Returning
    // false lets MusicFlow run its ordinary track-completion/next-song branch.
    if (ObserveHeat(false, self)) return false;
    return g_originalHeatTryStart(self);
}

int __fastcall HeatMusicStateHook(void* self, void*) {
    const int result = g_originalHeatMusicState(self);
    // Only replace the pursuit state. Do not override muted music or ambience.
    return result == 1 && ObserveHeat(false, self) ? 0 : result;
}

void __fastcall CopIntroHook(void* self, void*) {
    if (!ObserveHeat(true)) { g_originalCopIntro(self); return; }
    const int view = *reinterpret_cast<int*>(static_cast<unsigned char*>(self) + 8);
    if (view < 0 || view > 15) { g_originalCopIntro(self); return; }
    const bool first = !g_heatKeeping.introPending;
    g_heatKeeping.DeferIntro(view, *reinterpret_cast<std::uint32_t*>(Address(0x00925AE8)));
    if (first) Log(LogLevel::Info, "Heat keeping: cop camera and intro sound deferred view=%d", view);
    // The native pursuit state and visual treatment outside this dedicated
    // camera/sound function remain untouched. Color-only onset needs visual QA.
}

bool InstallHeatKeeping(std::string* error) {
    if (!BytesEqual(0x004E79A0, {0x53,0x55,0x56,0x8B,0xF1}) ||
        !CreateHook(0x004E79A0, HeatUpdateHook, &g_originalHeatUpdate, error)) return false;
    if (!g_catalog->config.heatKeeping) return true;
    const bool guarded =
        BytesEqual(0x004E7500, {0x56,0x8B,0xF1,0x8B,0x0D,0xA8,0x1F,0x91,0x00}) &&
        BytesEqual(0x004E7470, {0x8B,0x81,0x50,0x01,0x00,0x00}) &&
        BytesEqual(0x004E79A0, {0x53,0x55,0x56,0x8B,0xF1}) &&
        BytesEqual(0x00469EC0, {0x83,0xEC,0x5C,0x53,0x55,0x56,0x57,0x8B,0xF9}) &&
        BytesEqual(0x00469E30, {0x8B,0x0D,0x90,0xCF,0x91,0x00}) &&
        BytesEqual(0x00423E71, {0xE8,0x7A,0xDD,0x04,0x00}) &&
        BytesEqual(0x00471C7C, {0xE9,0x3F,0x82,0xFF,0xFF});
    if (!guarded) { if (error) *error = "Heat keeping native surface mismatch"; return false; }
    return CreateHook(0x004E7500, HeatTryStartHook, &g_originalHeatTryStart, error) &&
        CreateHook(0x004E7470, HeatMusicStateHook, &g_originalHeatMusicState, error) &&
        CreateHook(0x00469EC0, CopIntroHook, &g_originalCopIntro, error);
}
