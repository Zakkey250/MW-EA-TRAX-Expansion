#include "PlaylistSelector.h"

namespace eatrax {

bool TrackModeAllowsContext(const TrackMode mode, const PlaybackContext context) {
    if (mode == TrackMode::All) return true;
    if (context == PlaybackContext::FrontEnd) return mode == TrackMode::FrontEnd;
    return mode == TrackMode::InGame;
}

std::size_t CountEligibleTracks(const TrackMode* modes, const std::size_t count,
                                const PlaybackContext context) {
    if (modes == nullptr) return 0;
    std::size_t eligible = 0;
    for (std::size_t index = 0; index < count; ++index) {
        if (TrackModeAllowsContext(modes[index], context)) ++eligible;
    }
    return eligible;
}

std::int32_t SelectTrackIndex(const TrackMode* modes, const std::size_t count,
                              const PlaybackContext context, const std::uint32_t randomValue,
                              const std::int32_t previousTrack) {
    const std::size_t eligible = CountEligibleTracks(modes, count, context);
    if (eligible == 0) return -1;

    const bool excludePrevious =
        eligible > 1 && previousTrack >= 0 && static_cast<std::size_t>(previousTrack) < count &&
        TrackModeAllowsContext(modes[previousTrack], context);
    const std::size_t selectable = eligible - (excludePrevious ? 1u : 0u);
    std::size_t target = randomValue % selectable;

    for (std::size_t index = 0; index < count; ++index) {
        if (!TrackModeAllowsContext(modes[index], context)) continue;
        if (excludePrevious && index == static_cast<std::size_t>(previousTrack)) continue;
        if (target == 0) return static_cast<std::int32_t>(index);
        --target;
    }
    return -1;
}

}  // namespace eatrax
