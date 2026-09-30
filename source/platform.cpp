#include "platform.h"

#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef __SWITCH__
#include <arpa/inet.h>
#include <switch.h>

namespace plat {
namespace {
PadState g_pad;
bool g_pl = false, g_nifm = false;

uint32_t Translate(u64 b) {
    uint32_t r = 0;
    if (b & HidNpadButton_A) r |= kA;
    if (b & HidNpadButton_B) r |= kB;
    if (b & HidNpadButton_X) r |= kX;
    if (b & HidNpadButton_Y) r |= kY;
    if (b & HidNpadButton_L) r |= kL;
    if (b & HidNpadButton_R) r |= kR;
    if (b & HidNpadButton_ZL) r |= kZL;
    if (b & HidNpadButton_ZR) r |= kZR;
    if (b & HidNpadButton_Plus) r |= kPlus;
    if (b & HidNpadButton_Minus) r |= kMinus;
    if (b & (HidNpadButton_Up | HidNpadButton_StickLUp)) r |= kUp;
    if (b & (HidNpadButton_Down | HidNpadButton_StickLDown)) r |= kDown;
    if (b & (HidNpadButton_Left | HidNpadButton_StickLLeft)) r |= kLeft;
    if (b & (HidNpadButton_Right | HidNpadButton_StickLRight)) r |= kRight;
    return r;
}
} // namespace

bool Init() {
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&g_pad);
    g_pl = R_SUCCEEDED(plInitialize(PlServiceType_User));
    g_nifm = R_SUCCEEDED(nifmInitialize(NifmServiceType_User));
    return g_pl;
}

void Exit() {
    if (g_nifm) nifmExit();
    if (g_pl) plExit();
}

std::string Path(const std::string& p) { return p; }

bool FontData(int which, const void** data, size_t* size) {
    PlFontData font;
    const PlSharedFontType type = which == 1 ? PlSharedFontType_KO : PlSharedFontType_Standard;
    if (R_FAILED(plGetSharedFontByType(&font, type))) return false;
    *data = font.address;
    *size = font.size;
    return true;
}

void PollInput(uint32_t& down, uint32_t& held, bool& quit) {
    quit = !appletMainLoop();
    padUpdate(&g_pad);
    down = Translate(padGetButtonsDown(&g_pad));
    held = Translate(padGetButtons(&g_pad));
}

bool CanChainload() { return envHasNextLoad(); }

bool Chainload(const std::string& nro, const std::string& argv) {
    return R_SUCCEEDED(envSetNextLoad(nro.c_str(), argv.c_str()));
}

bool IsAppletMode() {
    const AppletType t = appletGetAppletType();
    return t != AppletType_Application && t != AppletType_SystemApplication;
}

std::string IpAddress() {
    if (!g_nifm) return {};
    u32 ip = 0;
    if (R_FAILED(nifmGetCurrentIpAddress(&ip)) || ip == 0) return {};
    in_addr a{};
    a.s_addr = ip;
    return inet_ntoa(a);
}

} // namespace plat

#else // ------------------------------------------------------------- desktop

#include <fstream>
#include <iterator>
#include <vector>

namespace plat {
namespace {
std::vector<char> g_fonts[2];
uint32_t g_held = 0;

uint32_t KeyToButton(SDL_Keycode k) {
    switch (k) {
    case SDLK_RETURN: case SDLK_a: return kA;
    case SDLK_BACKSPACE: case SDLK_b: return kB;
    case SDLK_x: return kX;
    case SDLK_y: return kY;
    case SDLK_q: return kL;
    case SDLK_e: return kR;
    case SDLK_EQUALS: return kPlus;
    case SDLK_MINUS: return kMinus;
    case SDLK_UP: return kUp;
    case SDLK_DOWN: return kDown;
    case SDLK_LEFT: return kLeft;
    case SDLK_RIGHT: return kRight;
    default: return 0;
    }
}
} // namespace

bool Init() { return true; }
void Exit() {}

std::string Path(const std::string& p) {
    const char* root = std::getenv("LAUNCHER_SDMC");
    std::string base = root ? root : "./sdmc";
    if (p.rfind("sdmc:", 0) == 0) return base + p.substr(5);
    return p;
}

bool FontData(int which, const void** data, size_t* size) {
    const char* env = std::getenv(which == 1 ? "LAUNCHER_FONT_KO" : "LAUNCHER_FONT");
    if (!env) env = std::getenv("LAUNCHER_FONT");
    if (!env) return false;
    if (g_fonts[which].empty()) {
        std::ifstream f(env, std::ios::binary);
        g_fonts[which].assign(std::istreambuf_iterator<char>(f), {});
    }
    if (g_fonts[which].empty()) return false;
    *data = g_fonts[which].data();
    *size = g_fonts[which].size();
    return true;
}

void PollInput(uint32_t& down, uint32_t& held, bool& quit) {
    down = 0;
    quit = false;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) quit = true;
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            const uint32_t b = KeyToButton(e.key.keysym.sym);
            down |= b;
            g_held |= b;
        }
        if (e.type == SDL_KEYUP) g_held &= ~KeyToButton(e.key.keysym.sym);
    }
    held = g_held;
}

bool CanChainload() { return true; }

bool Chainload(const std::string& nro, const std::string& argv) {
    std::printf("[chainload] %s\n  argv: %s\n", nro.c_str(), argv.c_str());
    return true;
}

bool IsAppletMode() { return std::getenv("LAUNCHER_APPLET") != nullptr; }
std::string IpAddress() { return "192.168.0.23"; }

} // namespace plat
#endif
