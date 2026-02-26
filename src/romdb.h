#pragma once
#include <string>

namespace RomDB {
    // Download DAT from url and save parsed entries into dbPath.
    // Returns true on success.
    bool init(const std::string& url, const std::string& dbPath);
}
