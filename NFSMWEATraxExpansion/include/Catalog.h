#pragma once

#include "Types.h"

#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

namespace eatrax {

struct CatalogResult {
    Config config;
    std::vector<Track> tracks;
    std::vector<Track> pursuitTracks;
    std::vector<std::string> warnings;
    bool musicSfxImported = false;
    std::unordered_map<std::uint32_t, std::uint32_t> pursuitControlEvents;
};

Config LoadConfig(const std::filesystem::path& modRoot);
CatalogResult LoadCatalog(const std::filesystem::path& modRoot,
                          bool allowMusicSfxCodec = true);
bool LoadNativeMusic(CatalogResult& catalog, std::string* error);

}  // namespace eatrax
