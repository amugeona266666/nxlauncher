// Platform layer: Switch (libnx) or desktop (for testing on a PC).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace plat {

enum Button : uint32_t {
    kA = 1u << 0, kB = 1u << 1, kX = 1u << 2, kY = 1u << 3,
    kL = 1u << 4, kR = 1u << 5, kPlus = 1u << 6, kMinus = 1u << 7,
    kUp = 1u << 8, kDown = 1u << 9, kLeft = 1u << 10, kRight = 1u << 11,
    kZL = 1u << 12, kZR = 1u << 13,
};

bool Init();
void Exit();

// Maps "sdmc:/foo" to a real path for this platform.
std::string Path(const std::string& sdmc_path);

// Font data from the console (shared fonts) or a desktop file. which: 0 = standard, 1 = Korean.
bool FontData(int which, const void** data, size_t* size);

// Buttons pressed this frame and held right now.
void PollInput(uint32_t& down, uint32_t& held, bool& quit_requested);

// Whether the homebrew loader lets us chain into another NRO.
bool CanChainload();
// Queue an NRO to run after we exit. argv is the full argument string (first token = NRO path).
bool Chainload(const std::string& nro_sdmc_path, const std::string& argv);

// Album/applet mode has far less memory than title takeover.
bool IsAppletMode();

std::string IpAddress();

} // namespace plat
