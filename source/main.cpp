// Game Launcher — one list for every Nintendo system, launching the right emulator
// straight into the chosen game through the homebrew loader.
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <ctime>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "library.h"
#include "platform.h"

namespace {

constexpr int kW = 1280, kH = 720;
constexpr int kHeaderH = 72, kFooterH = 56, kSideW = 330, kRowH = 52;
const std::string kHbmenu = "sdmc:/hbmenu.nro";
const std::string kHbmenuBackup = std::string(Library::kConfigDir) + "/hbmenu_original.nro";

struct Color { Uint8 r, g, b, a; };
constexpr Color kBg{21, 23, 28, 255}, kPanel{30, 33, 40, 255}, kHeader{38, 42, 52, 255};
constexpr Color kText{232, 234, 240, 255}, kDim{140, 146, 160, 255}, kAccent{230, 70, 60, 255};
constexpr Color kSel{58, 64, 80, 255}, kSelFocus{86, 60, 64, 255}, kStar{250, 200, 60, 255};

// ------------------------------------------------------------------ text

// Two console fonts: the standard one lacks Hangul, the Korean one lacks some Latin,
// so each run of characters is drawn with whichever font has the glyphs.
class Text {
public:
    bool Init() {
        for (int which = 0; which < 2; ++which) {
            const void* data;
            size_t size;
            if (!plat::FontData(which, &data, &size)) continue;
            data_[which] = data;
            size_[which] = size;
        }
        return data_[0] || data_[1];
    }

    void Clear() {
        for (auto& [k, v] : cache_) SDL_DestroyTexture(v.tex);
        cache_.clear();
    }

    int Draw(SDL_Renderer* r, const std::string& s, int x, int y, int pt, Color c, int max_w = 0) {
        if (s.empty()) return 0;
        const std::string key = std::to_string(pt) + ":" + std::to_string(c.r) + std::to_string(c.g) +
                                std::to_string(c.b) + ":" + std::to_string(max_w) + ":" + s;
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            if (cache_.size() > 400) Clear();
            it = cache_.emplace(key, Render(r, s, pt, c, max_w)).first;
        }
        const Entry& e = it->second;
        if (e.tex) {
            SDL_Rect dst{x, y, e.w, e.h};
            SDL_RenderCopy(r, e.tex, nullptr, &dst);
        }
        return e.w;
    }

private:
    struct Entry {
        SDL_Texture* tex = nullptr;
        int w = 0, h = 0;
    };

    TTF_Font* Font(int which, int pt) {
        auto& f = fonts_[{which, pt}];
        if (!f && data_[which])
            f = TTF_OpenFontRW(SDL_RWFromConstMem(data_[which], static_cast<int>(size_[which])), 1, pt);
        return f;
    }

    static std::vector<Uint32> Decode(const std::string& s) {
        std::vector<Uint32> out;
        for (size_t i = 0; i < s.size();) {
            const unsigned char c = s[i];
            int n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
            Uint32 cp = n == 1 ? c : c & (0x3F >> (n - 1));
            for (int k = 1; k < n && i + k < s.size(); ++k) cp = (cp << 6) | (s[i + k] & 0x3F);
            out.push_back(cp);
            i += n;
        }
        return out;
    }

    static void Encode(Uint32 cp, std::string& out) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) out += static_cast<char>(0xC0 | (cp >> 6)), out += static_cast<char>(0x80 | (cp & 0x3F));
        else if (cp < 0x10000)
            out += static_cast<char>(0xE0 | (cp >> 12)), out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)),
                out += static_cast<char>(0x80 | (cp & 0x3F));
        else
            out += static_cast<char>(0xF0 | (cp >> 18)), out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)),
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)), out += static_cast<char>(0x80 | (cp & 0x3F));
    }

    Entry Render(SDL_Renderer* r, const std::string& s, int pt, Color c, int max_w) {
        TTF_Font* fonts[2] = {Font(0, pt), Font(1, pt)};
        auto pick = [&](Uint32 cp) {
            if (fonts[0] && TTF_GlyphIsProvided32(fonts[0], cp)) return 0;
            if (fonts[1] && TTF_GlyphIsProvided32(fonts[1], cp)) return 1;
            return fonts[0] ? 0 : 1;
        };
        // Split into runs that share a font.
        std::vector<std::pair<int, std::string>> runs;
        for (Uint32 cp : Decode(s)) {
            const int f = pick(cp);
            if (runs.empty() || runs.back().first != f) runs.push_back({f, {}});
            Encode(cp, runs.back().second);
        }
        std::vector<SDL_Surface*> parts;
        int w = 0, h = 0;
        for (auto& [f, str] : runs) {
            if (!fonts[f]) continue;
            SDL_Surface* surf = TTF_RenderUTF8_Blended(fonts[f], str.c_str(), SDL_Color{c.r, c.g, c.b, c.a});
            if (!surf) continue;
            parts.push_back(surf);
            w += surf->w;
            h = std::max(h, surf->h);
        }
        Entry e;
        if (parts.empty()) return e;
        SDL_Surface* all = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
        int x = 0;
        for (SDL_Surface* p : parts) {
            SDL_Rect dst{x, (h - p->h) / 2, p->w, p->h};
            SDL_SetSurfaceBlendMode(p, SDL_BLENDMODE_NONE);
            SDL_BlitSurface(p, nullptr, all, &dst);
            x += p->w;
            SDL_FreeSurface(p);
        }
        SDL_Rect src{0, 0, max_w > 0 ? std::min(w, max_w) : w, h};
        SDL_Surface* clipped = all;
        if (src.w != w) {
            clipped = SDL_CreateRGBSurfaceWithFormat(0, src.w, h, 32, SDL_PIXELFORMAT_ARGB8888);
            SDL_SetSurfaceBlendMode(all, SDL_BLENDMODE_NONE);
            SDL_BlitSurface(all, &src, clipped, nullptr);
            SDL_FreeSurface(all);
        }
        e.tex = SDL_CreateTextureFromSurface(r, clipped);
        e.w = clipped->w;
        e.h = clipped->h;
        SDL_FreeSurface(clipped);
        return e;
    }

    const void* data_[2] = {nullptr, nullptr};
    size_t size_[2] = {0, 0};
    std::map<std::pair<int, int>, TTF_Font*> fonts_;
    std::map<std::string, Entry> cache_;
};

// ------------------------------------------------------------------- app

class App {
public:
    App(SDL_Renderer* r, std::string self_path) : r_(r), self_(std::move(self_path)) {}

    bool Init() {
        if (!text_.Init()) return false;
        lib_.Load();
        Refresh();
        ip_ = plat::IpAddress();
        return true;
    }

    // Returns false when the app should exit.
    bool Frame() {
        uint32_t down, held;
        bool quit;
        plat::PollInput(down, held, quit);
        if (quit) return false;
        down |= Repeat(held);

        if (dialog_) HandleDialog(down);
        else if (settings_) HandleSettings(down);
        else HandleMain(down);

        Draw(true);
        return !exit_;
    }

    void Shutdown() { text_.Clear(); }

private:
    // Tabs: 0 = recent, 1 = favourites, then one per system.
    int TabCount() const { return 2 + static_cast<int>(lib_.Systems().size()); }

    std::string TabName(int t) const {
        if (t == 0) return "최근 실행";
        if (t == 1) return "즐겨찾기";
        return lib_.Systems()[t - 2].name;
    }

    std::vector<int> TabGames(int t) const {
        if (t == 0) return lib_.Recent();
        if (t == 1) return lib_.Favorites();
        return lib_.GamesOf(t - 2);
    }

    void Refresh() {
        lib_.Scan();
        counts_.assign(TabCount(), 0);
        for (int t = 0; t < TabCount(); ++t) counts_[t] = static_cast<int>(TabGames(t).size());
        if (tab_ >= TabCount()) tab_ = 0;
        if (tab_ == 0 && counts_[0] == 0) tab_ = FirstNonEmpty();
        SelectTab(tab_);
    }

    int FirstNonEmpty() const {
        for (int t = 2; t < TabCount(); ++t)
            if (counts_[t]) return t;
        return 2;
    }

    void SelectTab(int t) {
        tab_ = (t + TabCount()) % TabCount();
        list_ = TabGames(tab_);
        sel_ = std::min(sel_by_tab_[tab_], std::max(0, static_cast<int>(list_.size()) - 1));
    }

    // Held up/down repeats after a short delay.
    uint32_t Repeat(uint32_t held) {
        const uint32_t dirs = held & (plat::kUp | plat::kDown);
        const Uint32 now = SDL_GetTicks();
        if (!dirs) {
            repeat_since_ = 0;
            return 0;
        }
        if (!repeat_since_) {
            repeat_since_ = now;
            repeat_last_ = now;
            return 0;
        }
        if (now - repeat_since_ > 350 && now - repeat_last_ > 60) {
            repeat_last_ = now;
            return dirs;
        }
        return 0;
    }

    // ---------------------------------------------------------- dialogs

    struct Dialog {
        std::string title, body;
        std::function<void()> on_yes; // null = message only
    };

    void Message(std::string title, std::string body) { dialog_ = Dialog{std::move(title), std::move(body), nullptr}; }
    void Confirm(std::string title, std::string body, std::function<void()> yes) {
        dialog_ = Dialog{std::move(title), std::move(body), std::move(yes)};
    }

    void HandleDialog(uint32_t down) {
        if (down & plat::kA) {
            auto yes = dialog_->on_yes;
            dialog_.reset();
            if (yes) yes();
        } else if (down & plat::kB) {
            dialog_.reset();
        }
    }

    // ------------------------------------------------------------- main

    void HandleMain(uint32_t down) {
        if (down & (plat::kR | plat::kRight)) SwitchTab(+1);
        if (down & (plat::kL | plat::kLeft)) SwitchTab(-1);
        if (!list_.empty()) {
            if (down & plat::kDown) sel_ = (sel_ + 1) % static_cast<int>(list_.size());
            if (down & plat::kUp) sel_ = (sel_ - 1 + static_cast<int>(list_.size())) % static_cast<int>(list_.size());
            if (down & plat::kZR) sel_ = std::min(sel_ + 10, static_cast<int>(list_.size()) - 1);
            if (down & plat::kZL) sel_ = std::max(sel_ - 10, 0);
            sel_by_tab_[tab_] = sel_;
            if (down & plat::kA) Launch(lib_.Games()[list_[sel_]]);
            if (down & plat::kY) {
                lib_.ToggleFavorite(lib_.Games()[list_[sel_]]);
                counts_[1] = static_cast<int>(lib_.Favorites().size());
                if (tab_ == 1) SelectTab(1);
            }
        }
        if (down & plat::kMinus) {
            settings_ = true;
            setting_sel_ = 0;
        }
        if (down & plat::kPlus) exit_ = true;
    }

    void SwitchTab(int dir) {
        sel_by_tab_[tab_] = sel_;
        SelectTab(tab_ + dir);
    }

    void Launch(const Game& g) {
        const System& s = lib_.Systems()[g.system];
        const std::string emu = lib_.ResolveEmulator(s);
        if (emu.empty()) {
            Message("에뮬레이터가 없어요",
                    s.name + "용 에뮬레이터를 찾지 못했어요.\n찾아본 위치: " + (s.emulators.empty() ? "" : s.emulators[0]) +
                        "\n\n설정 파일에서 경로를 바꿀 수 있어요:\n" + std::string(Library::kConfigDir) + "/config.ini");
            return;
        }
        if (!plat::CanChainload()) {
            Message("실행할 수 없어요", "지금 환경에서는 다른 앱으로 넘어갈 수 없어요.\nhbmenu에서 런처를 실행해주세요.");
            return;
        }
        auto go = [this, emu, &s, g] {
            lib_.MarkPlayed(g);
            if (plat::Chainload(emu, lib_.BuildArgv(emu, s, g))) exit_ = true;
            else Message("실행 실패", "에뮬레이터를 넘겨주지 못했어요.");
        };
        if (s.heavy && plat::IsAppletMode()) {
            Confirm("메모리가 부족할 수 있어요",
                    "앨범에서 실행해서 메모리가 적은 모드예요.\n" + s.name +
                        " 게임은 게임을 R 누른 채 실행해서 띄운\nhbmenu에서 여는 걸 추천해요.\n\nA: 그래도 실행   B: 취소",
                    go);
            return;
        }
        go();
    }

    // --------------------------------------------------------- settings

    bool IsDefaultMenu() const {
        return FileExists(kHbmenuBackup) && FileSize(kHbmenu) == FileSize(self_) && self_ != kHbmenuBackup;
    }

    std::vector<std::string> SettingItems() const {
        return {
            std::string("게임 끝나면 런처로 돌아오기: ") + (IsDefaultMenu() ? "켜짐" : "꺼짐"),
            "원래 홈브루 메뉴 열기",
            "게임 목록 새로고침",
            "설정 파일 위치 보기",
            "닫기",
        };
    }

    void HandleSettings(uint32_t down) {
        const int n = static_cast<int>(SettingItems().size());
        if (down & plat::kDown) setting_sel_ = (setting_sel_ + 1) % n;
        if (down & plat::kUp) setting_sel_ = (setting_sel_ - 1 + n) % n;
        if (down & (plat::kB | plat::kMinus)) settings_ = false;
        if (!(down & plat::kA)) return;
        switch (setting_sel_) {
        case 0: ToggleDefaultMenu(); break;
        case 1: OpenHbmenu(); break;
        case 2:
            Refresh();
            Message("새로고침 완료", "게임 " + std::to_string(lib_.Games().size()) + "개를 찾았어요.");
            break;
        case 3:
            Message("설정 파일", std::string(Library::kConfigDir) +
                                     "/config.ini\n\n롬 폴더, 기종별 에뮬레이터 경로, 확장자, 실행 인자를\n바꿀 수 있어요.");
            break;
        default: settings_ = false;
        }
    }

    // Replaces sdmc:/hbmenu.nro with this launcher so the loader comes back here after a game.
    void ToggleDefaultMenu() {
        if (IsDefaultMenu()) {
            Confirm("원래대로 되돌릴까요?", "홈브루 메뉴를 원래 hbmenu로 되돌려요.\n\nA: 되돌리기   B: 취소", [this] {
                if (CopyFile(kHbmenuBackup, kHbmenu)) {
                    std::remove(plat::Path(kHbmenuBackup).c_str());
                    Message("되돌렸어요", "이제 게임을 끝내면 원래 hbmenu로 돌아가요.");
                } else {
                    Message("실패", "hbmenu.nro를 되돌리지 못했어요.");
                }
            });
            return;
        }
        Confirm("런처를 기본 메뉴로 쓸까요?",
                "sdmc:/hbmenu.nro 자리에 이 런처를 넣어요.\n게임을 끝내면 런처로 돌아오고, 원래 hbmenu는 설정에서 열 수 있어요.\n"
                "원래 파일은 " + kHbmenuBackup + "에 보관돼요.\n\nA: 켜기   B: 취소",
                [this] {
                    MakeDirs(Library::kConfigDir);
                    if (!FileExists(kHbmenuBackup) && !CopyFile(kHbmenu, kHbmenuBackup)) {
                        Message("실패", "원래 hbmenu를 백업하지 못해서 중단했어요.");
                        return;
                    }
                    if (self_ == kHbmenu || CopyFile(self_, kHbmenu)) {
                        Message("켰어요", "이제 홈브루를 끝내면 이 런처로 돌아와요.");
                    } else {
                        Message("실패", "런처를 복사하지 못했어요.\n" + self_);
                    }
                });
    }

    void OpenHbmenu() {
        const std::string target = FileExists(kHbmenuBackup) ? kHbmenuBackup : kHbmenu;
        if (target == self_ || !FileExists(target)) {
            Message("hbmenu가 없어요", "원래 홈브루 메뉴 파일을 찾지 못했어요.");
            return;
        }
        if (plat::Chainload(target, "\"" + target + "\"")) exit_ = true;
    }

    // ------------------------------------------------------------- draw

    void Fill(int x, int y, int w, int h, Color c) {
        SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, c.a);
        SDL_Rect rc{x, y, w, h};
        SDL_RenderFillRect(r_, &rc);
    }

public:
    void Draw(bool present) {
        Fill(0, 0, kW, kH, kBg);
        DrawHeader();
        DrawTabs();
        DrawList();
        DrawFooter();
        if (settings_) DrawSettings();
        if (dialog_) DrawDialog();
        if (present) SDL_RenderPresent(r_);
    }

private:

    void DrawHeader() {
        Fill(0, 0, kW, kHeaderH, kHeader);
        Fill(0, kHeaderH - 3, kW, 3, kAccent);
        text_.Draw(r_, "게임 런처", 32, 18, 30, kText);
        std::time_t t = std::time(nullptr);
        char clock[16];
        std::strftime(clock, sizeof clock, "%H:%M", std::localtime(&t));
        std::string right = clock;
        if (!ip_.empty()) right = "폰 화면: " + ip_ + ":8080    " + right;
        text_.Draw(r_, right, kW - 32 - 520, 24, 20, kDim);
    }

    void DrawTabs() {
        Fill(0, kHeaderH, kSideW, kH - kHeaderH - kFooterH, kPanel);
        const int visible = (kH - kHeaderH - kFooterH - 16) / 42;
        const int first = std::clamp(tab_ - visible / 2, 0, std::max(0, TabCount() - visible));
        for (int i = 0; i < visible && first + i < TabCount(); ++i) {
            const int t = first + i;
            const int y = kHeaderH + 8 + i * 42;
            if (t == tab_) Fill(0, y, kSideW, 40, kSelFocus), Fill(0, y, 5, 40, kAccent);
            const Color c = counts_[t] ? kText : kDim;
            text_.Draw(r_, TabName(t), 24, y + 8, 21, c);
            const std::string n = std::to_string(counts_[t]);
            text_.Draw(r_, n, kSideW - 24 - 12 * static_cast<int>(n.size()), y + 10, 18, kDim);
        }
    }

    void DrawList() {
        const int x0 = kSideW + 24, top = kHeaderH + 12, h = kH - kHeaderH - kFooterH - 24;
        if (list_.empty()) {
            std::string hint = tab_ == 0   ? "아직 실행한 게임이 없어요."
                               : tab_ == 1 ? "Y 버튼으로 즐겨찾기를 추가해보세요."
                                           : "게임이 없어요.\n" + lib_.RomRoot() + "/" + lib_.Systems()[tab_ - 2].folder + " 에 넣어주세요.";
            int y = top + 20;
            size_t start = 0;
            while (start <= hint.size()) {
                const size_t nl = hint.find('\n', start);
                text_.Draw(r_, hint.substr(start, nl - start), x0 + 8, y, 22, kDim);
                y += 34;
                if (nl == std::string::npos) break;
                start = nl + 1;
            }
            return;
        }
        const int visible = h / kRowH;
        const int n = static_cast<int>(list_.size());
        const int first = std::clamp(sel_ - visible / 2, 0, std::max(0, n - visible));
        for (int i = 0; i < visible && first + i < n; ++i) {
            const int idx = first + i;
            const Game& g = lib_.Games()[list_[idx]];
            const int y = top + i * kRowH;
            if (idx == sel_) Fill(x0 - 8, y, kW - x0 - 16, kRowH - 4, kSel);
            int x = x0 + 8;
            if (lib_.IsFavorite(g)) x += text_.Draw(r_, "★ ", x, y + 12, 22, kStar);
            text_.Draw(r_, g.title, x, y + 11, 23, kText, kW - x - 220);
            if (tab_ < 2) text_.Draw(r_, lib_.Systems()[g.system].name, kW - 200, y + 14, 18, kDim, 180);
        }
        // scrollbar
        if (n > visible) {
            const int bar_h = std::max(30, h * visible / n);
            const int bar_y = top + (h - bar_h) * first / std::max(1, n - visible);
            Fill(kW - 10, bar_y, 4, bar_h, kDim);
        }
    }

    void DrawFooter() {
        Fill(0, kH - kFooterH, kW, kFooterH, kHeader);
        text_.Draw(r_, "A 실행    Y 즐겨찾기    L/R 기종 바꾸기    ZL/ZR 빠르게 넘기기    - 설정    + 종료", 32, kH - kFooterH + 15, 20, kDim);
    }

    void DrawPanel(int w, int h, const std::string& title) {
        SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
        Fill(0, 0, kW, kH, Color{0, 0, 0, 160});
        SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_NONE);
        const int x = (kW - w) / 2, y = (kH - h) / 2;
        Fill(x, y, w, h, kPanel);
        Fill(x, y, w, 4, kAccent);
        text_.Draw(r_, title, x + 28, y + 22, 26, kText);
    }

    void DrawSettings() {
        const auto items = SettingItems();
        const int w = 720, h = 110 + 52 * static_cast<int>(items.size());
        DrawPanel(w, h, "설정");
        const int x = (kW - w) / 2, y = (kH - h) / 2 + 76;
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            if (i == setting_sel_) Fill(x + 16, y + i * 52, w - 32, 46, kSel);
            text_.Draw(r_, items[i], x + 32, y + i * 52 + 10, 22, kText);
        }
    }

    void DrawDialog() {
        std::vector<std::string> lines;
        size_t start = 0;
        const std::string& b = dialog_->body;
        while (true) {
            const size_t nl = b.find('\n', start);
            lines.push_back(b.substr(start, nl - start));
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
        const int w = 820, h = 120 + 34 * static_cast<int>(lines.size());
        DrawPanel(w, h, dialog_->title);
        const int x = (kW - w) / 2, y = (kH - h) / 2 + 76;
        for (size_t i = 0; i < lines.size(); ++i) text_.Draw(r_, lines[i], x + 28, y + static_cast<int>(i) * 34, 21, kText, w - 56);
        if (!dialog_->on_yes) text_.Draw(r_, "A 확인", x + w - 120, y + h - 110, 20, kDim);
    }

    SDL_Renderer* r_;
    std::string self_;
    Text text_;
    Library lib_;
    std::string ip_;

    int tab_ = 0, sel_ = 0;
    std::map<int, int> sel_by_tab_;
    std::vector<int> list_, counts_;
    bool settings_ = false, exit_ = false;
    int setting_sel_ = 0;
    std::optional<Dialog> dialog_;
    Uint32 repeat_since_ = 0, repeat_last_ = 0;
};

} // namespace

int main(int argc, char* argv[]) {
    plat::Init();
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_Window* win = SDL_CreateWindow("Game Launcher", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kW, kH, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);

    std::string self = argc > 0 && argv[0] && argv[0][0] ? argv[0] : "sdmc:/switch/nxlauncher/nxlauncher.nro";
    App app(ren, self);
    int frames_left = -1;
    if (const char* f = SDL_getenv("LAUNCHER_FRAMES")) frames_left = std::atoi(f); // desktop testing
    if (app.Init()) {
        while (app.Frame() && frames_left-- != 0) {}
    }
    if (const char* shot = SDL_getenv("LAUNCHER_SCREENSHOT")) { // desktop testing
        app.Draw(false);
        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, kW, kH, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
        SDL_SaveBMP(s, shot);
        SDL_FreeSurface(s);
    }
    app.Shutdown();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    plat::Exit();
    return 0;
}
