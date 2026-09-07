#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace eatrax {
inline float PursuitPressure(float heat, int cops, float kmh) {
    const auto unit = [](float x) { return std::clamp(x, 0.0f, 1.0f); };
    const float h = unit((heat - 1.0f) / 4.0f);
    const float c = unit(cops / 8.0f);
    const float s = unit((kmh - 80.0f) / 180.0f);
    return 20*h + 30*c + 20*s + 30*c*s;
}
struct PursuitIntensityLatch {
    bool high = false;
    bool dropping = false;
    std::uint32_t dropStart = 0;
    int Update(float pressure, int native, std::uint32_t simulationTick) {
        native = std::clamp(native, 0, 127);
        if (pressure >= 60.0f || native >= 85) {
            high = true;
            dropping = false;
        } else if (high && pressure < 48.0f && native < 75) {
            if (!dropping) { dropping = true; dropStart = simulationTick; }
            // MW's simulation clock has 4000 ticks per second; pauses do not expire the hold.
            if (simulationTick - dropStart >= 24000u) { high = false; dropping = false; }
        } else {
            dropping = false;
        }
        const int target = std::max(native, static_cast<int>(std::lround(pressure*1.27f)));
        return high ? std::clamp(std::max(target, 90), 0, 127) : std::clamp(target, 0, 84);
    }
};
}
