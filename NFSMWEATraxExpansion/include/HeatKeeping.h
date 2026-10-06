#pragma once
#include <cmath>
#include <cstdint>

namespace eatrax {
// Scalar state only: never retain a camera, vehicle, or pursuit object for replay.
struct HeatKeepingState {
    bool holding = false;
    bool escalated = false;
    bool introPending = false;
    bool exitTail = false;
    bool nativeOwned = false;
    int introView = -1;
    std::uintptr_t pursuitIdentity = 0; // comparison only, never dereferenced
    std::uint32_t pendingSince = 0;

    void Reset() { *this = {}; }
    static bool Low(float heat) { return std::isfinite(heat) && heat >= 1.0f && heat < 4.0f; }

    bool Observe(bool enabled, bool roaming, bool valid, float heat,
                 std::uintptr_t pursuit, int status, std::uint32_t now,
                 bool startBoundary = false, bool nativeMusicPursuitReady = false,
                 bool nativePursuitPlaying = false) {
        if (!enabled) { Reset(); return false; }
        if (pursuitIdentity && pursuit && pursuit != pursuitIdentity) Reset();
        // An event can return to free roam without ending its pursuit. Once
        // native pursuit music has started, Keep must not take it over midway.
        if (nativePursuitPlaying) {
            nativeOwned = true;
            holding = exitTail = false;
            if (pursuit && status >= 0 && status <= 2) pursuitIdentity = pursuit;
            if (!escalated) { introPending = false; introView = -1; }
            return false;
        }
        if (nativeOwned) {
            if ((pursuit && status >= 0 && status <= 2) ||
                (!pursuit && nativeMusicPursuitReady)) return false;
            Reset();
        }
        if (!roaming || !valid) { Reset(); return false; }
        if (pursuit && status >= 0 && status <= 2) {
            pursuitIdentity = pursuit;
            exitTail = false;
        } else if (!(startBoundary && !pursuit && status == -1 && !pursuitIdentity)) {
            // MusicAI can still request pursuit music after gameplay reports
            // escape (or detaches the pursuit). Keep gating BEFORE TryStart
            // clears the EA TRAX event, until that native request has cleared.
            if (holding && pursuitIdentity && nativeMusicPursuitReady &&
                (status == 4 || (!pursuit && status == -1))) {
                introPending = false;
                introView = -1;
                exitTail = true;
                return true;
            }
            // The intro can be requested before the pursuit interface is installed.
            if (introPending && !pursuitIdentity && now - pendingSince < 3000u)
                return holding;
            Reset();
            return false;
        }
        if (escalated) return false;
        if (Low(heat)) { holding = true; return true; }
        // HEAT 4+ is latched until this pursuit ends, even if another mod lowers heat.
        escalated = true;
        holding = false;
        return false;
    }

    void DeferIntro(int view, std::uint32_t now) {
        if (introPending || exitTail) return;
        introPending = true;
        introView = view;
        pendingSince = now;
    }
    int TakeIntro() {
        if (!escalated || !introPending) return -1;
        const int view = introView;
        introPending = false;
        introView = -1;
        return view;
    }
};
} // namespace eatrax
