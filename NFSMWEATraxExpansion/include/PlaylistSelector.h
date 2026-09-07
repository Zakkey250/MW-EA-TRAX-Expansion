#pragma once

#include "Types.h"

#include <cstddef>
#include <cstdint>

namespace eatrax {

enum class PlaybackContext : std::uint8_t {
    FrontEnd = 0,
    InGame = 1,
};

bool TrackModeAllowsContext(TrackMode mode, PlaybackContext context);
std::size_t CountEligibleTracks(const TrackMode* modes, std::size_t count,
                                PlaybackContext context);
std::int32_t SelectTrackIndex(const TrackMode* modes, std::size_t count,
                              PlaybackContext context, std::uint32_t randomValue,
                              std::int32_t previousTrack);

}  // namespace eatrax
