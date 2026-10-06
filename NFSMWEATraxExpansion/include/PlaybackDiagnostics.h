#pragma once
#include <cstdint>
namespace eatrax {
struct PlaybackSnapshot {
    int music = -1, pool = -1, channel = -1, sound = -1;
    std::uint32_t event = 0, flags = 0;
    bool paused = false;
    bool SameContext(const PlaybackSnapshot& s) const {
        return music==s.music && pool==s.pool && channel==s.channel && sound==s.sound &&
               event==s.event && flags==s.flags && paused==s.paused;
    }
};
// Observe only. No audio restart, timer override or synthetic next-song command.
struct PlaybackDiagnosticState {
    PlaybackSnapshot previous;
    bool initialized = false, hasPolled = false, gap = false, warned = false;
    std::uint64_t gapSince = 0, lastPoll = 0, lastStateLog = 0;
    bool Poll(std::uint64_t now) {
        if (hasPolled && now-lastPoll<1000) return false;
        hasPolled=true;lastPoll=now;return true;
    }
    bool StateChanged(const PlaybackSnapshot& s, std::uint64_t now) {
        const bool changed=!initialized || !previous.SameContext(s);
        if(changed){previous=s;initialized=true;lastStateLog=now;}
        return changed;
    }
    bool SuspectedGap(const PlaybackSnapshot& s, std::uint64_t now) {
        // Native channel state 1 is stopped; 3 is paused. The native scheduler
        // uses this same boundary for selecting another EA TRAX song.
        const bool candidate=s.music==0 && s.channel==1 && !s.paused &&
                             (s.flags&4) && !(s.flags&0xC00);
        if(!candidate){gap=false;warned=false;return false;}
        if(!gap){gap=true;gapSince=now;}
        if(!warned && now-gapSince>=10000){warned=true;return true;}
        return false;
    }
};
} // namespace eatrax
