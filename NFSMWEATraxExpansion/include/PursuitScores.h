#pragma once
#include <array>
#include <cstdint>

namespace eatrax {
struct PursuitScoreDefinition {
    const wchar_t* key;
    const wchar_t* section;
    const char* title;
    std::uint32_t event;
};
inline constexpr std::array<PursuitScoreDefinition, 8> kPursuitScores{{
    {L"Battle Race Theme 2", L"AdaptiveBattleTheme2", "NFS The Run - Battle Race Theme 2 (adaptive)", 0xE20001},
    {L"Sprint Race Theme 1", L"AdaptiveSprintRaceTheme1", "NFS The Run - Sprint Race Theme 1 (adaptive)", 0xE20101},
    {L"Checkpoint Race Theme 3", L"AdaptiveCheckpointRaceTheme3", "NFS The Run - Checkpoint Race Theme 3 (adaptive)", 0xE20401},
    {L"Sprint Race Theme 2", L"AdaptiveSprintRaceTheme2", "NFS The Run - Sprint Race Theme 2 (adaptive)", 0xE20601},
    {L"Checkpoint Race Theme 2", L"AdaptiveCheckpointRaceTheme2", "NFS The Run - Checkpoint Race Theme 2 (adaptive)", 0xE20801},
    {L"Battle Race Theme 1", L"AdaptiveBattleRaceTheme1", "NFS The Run - Battle Race Theme 1 (adaptive)", 0xE20A01},
    {L"Survival Race Theme 3", L"AdaptiveSurvivalRaceTheme3", "NFS The Run - Survival Race Theme 3 (adaptive)", 0xE20C01},
    {L"Survival Race Theme 2", L"AdaptiveSurvivalRaceTheme2", "NFS The Run - Survival Race Theme 2 (adaptive)", 0xE20E01},
}};
}
