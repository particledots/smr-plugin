// src/plugin: text encoding of smr::Snapshot for the project file. Plain C++, no JUCE (tested in CI).
// Format version 1: 86 bytes as 172 hex digits. Byte order is fixed (little endian), so a project saved on the
// M4 mac mini loads on the Intel Mac and the other way round.
#pragma once

#include <string>

#include "Engine.h"

namespace smr_plugin
{
    constexpr int kStateVersion = 1;
    std::string encodeSnapshot (const smr::Snapshot& s);
    // Returns false for the wrong length, a non-hex character or an unknown version. Range checks are done by
    // Engine::loadSnapshot(), not here.
    bool decodeSnapshot (const std::string& text, smr::Snapshot& out);
}
