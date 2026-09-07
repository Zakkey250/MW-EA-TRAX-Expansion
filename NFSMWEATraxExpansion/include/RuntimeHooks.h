#pragma once

#include "Catalog.h"

#include <cstdint>
#include <string>

namespace eatrax {

struct RuntimeStatus {
    bool hooksInstalled = false;
    bool tracksAppended = false;
    std::uint32_t nativeTrackCount = 0;
    std::uint32_t customTrackCount = 0;
};

bool InstallRuntimeHooks(CatalogResult* catalog, std::string* error);
void StopRuntimeAudio();
RuntimeStatus GetRuntimeStatus();

}  // namespace eatrax
