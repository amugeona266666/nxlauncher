#include "library.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <map>
#include <sstream>
#include <sys/stat.h>

#include "platform.h"

namespace {

std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string Trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    const auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, sep)) {
        item = Trim(item);
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

std::string Join(const std::vector<std::string>& v, const char* sep) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) out += (i ? sep : "") + v[i];
    return out;
}

bool IsDir(const std::string& sdmc) {
    struct stat st{};
    return stat(plat::Path(sdmc).c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

struct Entry {
    std::string name;
    bool dir;
};

std::vector<Entry> List(const std::string& sdmc) {
    std::vector<Entry> out;
    DIR* d = opendir(plat::Path(sdmc).c_str());
    if (!d) return out;
    while (dirent* e = readdir(d)) {
        const std::string n = e->d_name;
        if (n.empty() || n[0] == '.') continue; // ., .., ._ mac files, hidden
        out.push_back({n, IsDir(sdmc + "/" + n)});
    }
    closedir(d);
    return out;
}

std::string StripExtension(const std::string& name) {
    const auto dot = name.rfind('.');
    return dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
}

std::string Extension(const std::string& name) {
    const auto dot = name.rfind('.');
    return dot == std::string::npos ? std::string{} : Lower(name.substr(dot));
}

// Default systems. Every value can be changed in config.ini.
std::vector<System> Defaults() {
    const std::string ra = "sdmc:/retroarch/cores/";
    auto core = [&](std::initializer_list<const char*> names) {
        std::vector<std::string> v;
        for (const char* n : names) v.push_back(ra + n + "_libretro_libnx.nro");
        return v;
    };
    return {
        {"nes", "패미컴", "nes", {".nes", ".unf", ".unif"}, core({"fceumm", "nestopia", "mesen"}), "{path}"},
        {"fds", "디스크시스템", "fds", {".fds"}, core({"fceumm", "nestopia", "mesen"}), "{path}"},
        {"snes", "슈퍼패미컴", "snes", {".sfc", ".smc", ".bs"}, core({"snes9x", "snes9x2010"}), "{path}"},
        {"gb", "게임보이", "gb", {".gb", ".sgb"}, core({"gambatte", "sameboy", "mgba"}), "{path}"},
        {"gbc", "게임보이 컬러", "gbc", {".gbc"}, core({"gambatte", "sameboy", "mgba"}), "{path}"},
        {"gba", "게임보이 어드밴스", "gba", {".gba"}, core({"mgba", "gpsp", "vba_next"}), "{path}"},
        {"n64", "닌텐도 64", "n64", {".z64", ".n64", ".v64"}, core({"mupen64plus_next", "parallel_n64"}), "{path}"},
        {"nds", "닌텐도 DS", "nds", {".nds"}, core({"melonds", "melondsds", "desmume"}), "{path}"},
        {"dsi", "닌텐도 DSi", "dsi", {".nds", ".dsi"}, core({"melonds", "melondsds"}), "{path}"},
        {"vb", "버추얼보이", "vb", {".vb", ".vboy"}, core({"mednafen_vb"}), "{path}"},
        {"pokemini", "포켓몬 미니", "pokemini", {".min"}, core({"pokemini"}), "{path}"},
        {"gc", "게임큐브", "gc", {".rvz", ".iso", ".gcm", ".gcz", ".ciso", ".dol"},
         {"sdmc:/switch/dolphin/dolphin.nro"}, "--game {path}", true},
        {"wii", "Wii", "wii", {".rvz", ".iso", ".wbfs", ".wad", ".gcz", ".ciso"},
         {"sdmc:/switch/dolphin/dolphin.nro"}, "--game {path}", true},
        {"3ds", "닌텐도 3DS", "3ds", {".3ds", ".cci", ".cxi", ".cia", ".3dsx"},
         {"sdmc:/switch/dekopon/dekopon.nro"}, "{path}", true},
        {"wiiu", "Wii U", "wiiu", {".wua", ".wud", ".wux", ".rpx"},
         {"sdmc:/switch/cemu/cemu.nro", "sdmc:/switch/Cemu/cemu.nro"}, "{path}", true, true},
    };
}

} // namespace

bool FileExists(const std::string& p) {
    struct stat st{};
    return stat(plat::Path(p).c_str(), &st) == 0;
}

long FileSize(const std::string& p) {
    struct stat st{};
    return stat(plat::Path(p).c_str(), &st) == 0 ? static_cast<long>(st.st_size) : -1;
}

void MakeDirs(const std::string& p) {
    const std::string real = plat::Path(p);
    for (size_t i = 1; i <= real.size(); ++i) {
        if (i == real.size() || real[i] == '/') {
            const std::string part = real.substr(0, i);
            if (!part.empty() && part.back() != ':') mkdir(part.c_str(), 0777);
        }
    }
}

bool CopyFile(const std::string& from, const std::string& to) {
    std::FILE* in = std::fopen(plat::Path(from).c_str(), "rb");
    if (!in) return false;
    const std::string tmp = to + ".tmp";
    std::FILE* out = std::fopen(plat::Path(tmp).c_str(), "wb");
    if (!out) {
        std::fclose(in);
        return false;
    }
    std::vector<char> buf(256 * 1024);
    bool ok = true;
    size_t n;
    while ((n = std::fread(buf.data(), 1, buf.size(), in)) > 0) {
        if (std::fwrite(buf.data(), 1, n, out) != n) {
            ok = false;
            break;
        }
    }
    std::fclose(in);
    ok = std::fclose(out) == 0 && ok;
    if (!ok) {
        std::remove(plat::Path(tmp).c_str());
        return false;
    }
    std::remove(plat::Path(to).c_str());
    return std::rename(plat::Path(tmp).c_str(), plat::Path(to).c_str()) == 0;
}

// ------------------------------------------------------------------- config

void Library::WriteDefaultConfig() const {
    MakeDirs(kConfigDir);
    std::ofstream f(plat::Path(std::string(kConfigDir) + "/config.ini"));
    f << "; 게임 런처 설정. 바꾼 뒤 런처에서 '목록 새로고침' 또는 재실행.\n"
         "; emulators: '|'로 여러 후보를 적으면 처음 발견된 것을 씀\n"
         "; args: {path} 자리에 게임 경로가 들어감\n\n"
         "rom_root=sdmc:/roms\n";
    for (const System& s : systems_) {
        f << "\n[" << s.id << "]\n"
          << "name=" << s.name << "\n"
          << "folder=" << s.folder << "\n"
          << "extensions=" << Join(s.extensions, ",") << "\n"
          << "emulators=" << Join(s.emulators, "|") << "\n"
          << "args=" << s.args << "\n";
    }
}

void Library::LoadConfig() {
    systems_ = Defaults();
    const std::string path = std::string(kConfigDir) + "/config.ini";
    if (!FileExists(path)) {
        WriteDefaultConfig();
        return;
    }
    std::ifstream f(plat::Path(path));
    std::string line, section;
    std::map<std::string, std::map<std::string, std::string>> ini;
    while (std::getline(f, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = Lower(Trim(line.substr(1, line.size() - 2)));
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        ini[section][Lower(Trim(line.substr(0, eq)))] = Trim(line.substr(eq + 1));
    }
    if (!ini[""]["rom_root"].empty()) rom_root_ = ini[""]["rom_root"];
    while (!rom_root_.empty() && rom_root_.back() == '/') rom_root_.pop_back();

    for (System& s : systems_) {
        auto it = ini.find(s.id);
        if (it == ini.end()) continue;
        auto& kv = it->second;
        if (!kv["name"].empty()) s.name = kv["name"];
        if (!kv["folder"].empty()) s.folder = kv["folder"];
        if (!kv["extensions"].empty()) {
            s.extensions = Split(kv["extensions"], ',');
            for (auto& e : s.extensions) e = Lower(e);
        }
        if (!kv["emulators"].empty()) s.emulators = Split(kv["emulators"], '|');
        if (!kv["args"].empty()) s.args = kv["args"];
    }
}

// -------------------------------------------------------------------- state

void Library::LoadState() {
    favorites_.clear();
    recent_.clear();
    std::ifstream f(plat::Path(std::string(kConfigDir) + "/state.txt"));
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("fav ", 0) == 0) favorites_.insert(line.substr(4));
        else if (line.rfind("recent ", 0) == 0) recent_.push_back(line.substr(7));
    }
}

void Library::SaveState() const {
    MakeDirs(kConfigDir);
    std::ofstream f(plat::Path(std::string(kConfigDir) + "/state.txt"));
    for (const auto& p : favorites_) f << "fav " << p << "\n";
    for (const auto& p : recent_) f << "recent " << p << "\n";
}

void Library::Load() {
    LoadConfig();
    LoadState();
}

void Library::ToggleFavorite(const Game& g) {
    if (!favorites_.erase(g.path)) favorites_.insert(g.path);
    SaveState();
}

void Library::MarkPlayed(const Game& g) {
    recent_.erase(std::remove(recent_.begin(), recent_.end(), g.path), recent_.end());
    recent_.insert(recent_.begin(), g.path);
    if (recent_.size() > 30) recent_.resize(30);
    SaveState();
}

// --------------------------------------------------------------------- scan

void Library::ScanSystem(int index) {
    const System& s = systems_[index];
    const std::string dir = rom_root_ + "/" + s.folder;
    auto matches = [&](const std::string& name) {
        return std::find(s.extensions.begin(), s.extensions.end(), Extension(name)) != s.extensions.end();
    };
    auto add_files = [&](const std::string& d, const std::string& prefix) {
        for (const Entry& e : List(d)) {
            if (!e.dir && matches(e.name))
                games_.push_back({prefix + StripExtension(e.name), d + "/" + e.name, index});
        }
    };

    add_files(dir, "");
    for (const Entry& e : List(dir)) {
        if (!e.dir) continue;
        const std::string sub = dir + "/" + e.name;
        if (s.wiiu_folders && IsDir(sub + "/code")) {
            // Extracted Wii U game: launch the .rpx inside code/.
            for (const Entry& c : List(sub + "/code")) {
                if (!c.dir && Extension(c.name) == ".rpx") {
                    games_.push_back({e.name, sub + "/code/" + c.name, index});
                    break;
                }
            }
            continue;
        }
        add_files(sub, e.name + " / "); // one level of sub-folders (e.g. per-series folders)
    }
}

void Library::Scan() {
    games_.clear();
    for (int i = 0; i < static_cast<int>(systems_.size()); ++i) {
        MakeDirs(rom_root_ + "/" + systems_[i].folder);
        ScanSystem(i);
    }
    std::stable_sort(games_.begin(), games_.end(), [](const Game& a, const Game& b) {
        if (a.system != b.system) return a.system < b.system;
        return Lower(a.title) < Lower(b.title);
    });
}

std::vector<int> Library::GamesOf(int system) const {
    std::vector<int> out;
    for (int i = 0; i < static_cast<int>(games_.size()); ++i)
        if (games_[i].system == system) out.push_back(i);
    return out;
}

std::vector<int> Library::Favorites() const {
    std::vector<int> out;
    for (int i = 0; i < static_cast<int>(games_.size()); ++i)
        if (favorites_.count(games_[i].path)) out.push_back(i);
    return out;
}

std::vector<int> Library::Recent() const {
    std::vector<int> out;
    for (const auto& p : recent_) {
        for (int i = 0; i < static_cast<int>(games_.size()); ++i) {
            if (games_[i].path == p) {
                out.push_back(i);
                break;
            }
        }
    }
    return out;
}

std::string Library::ResolveEmulator(const System& s) const {
    for (const auto& e : s.emulators)
        if (FileExists(e)) return e;
    return {};
}

std::string Library::BuildArgv(const std::string& nro, const System& s, const Game& g) const {
    auto quote = [](const std::string& v) { return "\"" + v + "\""; };
    std::string args = s.args;
    const std::string token = "{path}";
    for (auto pos = args.find(token); pos != std::string::npos; pos = args.find(token, pos))
        args.replace(pos, token.size(), quote(g.path)), pos += g.path.size() + 2;
    return quote(nro) + (args.empty() ? "" : " " + args);
}
