// Game library: systems from config.ini, games scanned from the shared ROM folder,
// favourites and recently played.
#pragma once
#include <set>
#include <string>
#include <vector>

struct System {
    std::string id;       // config section, e.g. "gba"
    std::string name;     // shown in the list
    std::string folder;   // sub-folder of rom_root
    std::vector<std::string> extensions;
    std::vector<std::string> emulators; // candidate NROs, first existing one is used
    std::string args;     // e.g. "--game {path}"
    bool heavy = false;   // needs title-takeover memory
    bool wiiu_folders = false; // accept extracted Wii U games (folder/code/*.rpx)
};

struct Game {
    std::string title;
    std::string path; // sdmc path passed to the emulator
    int system = 0;
};

class Library {
public:
    static constexpr const char* kConfigDir = "sdmc:/config/nxlauncher";

    void Load();          // config + state
    void Scan();          // (re)scan rom_root
    void SaveState() const;

    const std::string& RomRoot() const { return rom_root_; }
    const std::vector<System>& Systems() const { return systems_; }
    const std::vector<Game>& Games() const { return games_; }
    std::vector<int> GamesOf(int system) const;
    std::vector<int> Favorites() const;
    std::vector<int> Recent() const;

    bool IsFavorite(const Game& g) const { return favorites_.count(g.path) != 0; }
    void ToggleFavorite(const Game& g);
    void MarkPlayed(const Game& g);

    // Existing emulator NRO for the system, empty if none is installed.
    std::string ResolveEmulator(const System& s) const;
    // Full argv string for the homebrew loader.
    std::string BuildArgv(const std::string& nro, const System& s, const Game& g) const;

private:
    void LoadConfig();
    void WriteDefaultConfig() const;
    void LoadState();
    void ScanSystem(int index);

    std::string rom_root_ = "sdmc:/roms";
    std::vector<System> systems_;
    std::vector<Game> games_;
    std::set<std::string> favorites_;
    std::vector<std::string> recent_;
};

// Small file helpers shared with the UI.
bool FileExists(const std::string& sdmc_path);
long FileSize(const std::string& sdmc_path);
bool CopyFile(const std::string& from_sdmc, const std::string& to_sdmc);
void MakeDirs(const std::string& sdmc_path);
