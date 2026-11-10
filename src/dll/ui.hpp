#pragma once

#include "config.hpp"
#include "jvm_bridge.hpp"
#include "modules.hpp"

#include "../../third_party/imgui/imgui.h"
#include <unordered_map>
#include <string>
#include <cstdint>

namespace meow {

struct AccentPalette {
    ImVec4 primary;
    ImVec4 secondary;
    ImVec4 glow;
    const char* name;
};

class MeowUI {
public:
    void InitFonts(ImGuiIO& io);
    void Render(ClientConfig& cfg, const FrameSnapshot& snap, const ModuleManager& mods,
                const std::string& bridgeStatus, bool menuOpen, bool& requestSave);

    // Keybind capture state (blocks WndProc toggle while binding a key)
    bool IsCapturingBind() const { return activeBindTarget_ != nullptr; }
    void HandleKeyCapture(int vk);

private:
    const AccentPalette& CurrentAccent(const ClientConfig& cfg) const;

    // Custom vector primitives & widgets
    void DrawCatLogo(ImDrawList* dl, ImVec2 center, float radius, ImU32 colA, ImU32 colB);
    void DrawTabIcon(ImDrawList* dl, TabSection tab, ImVec2 center, float size, ImU32 col);

    bool BeginModuleCard(const char* id, const char* title, const char* desc,
                         bool& enabled, int& keybind, const AccentPalette& acc,
                         bool hasSettings = true);
    void EndModuleCard();

    bool ToggleSwitch(const char* id, bool& value, const AccentPalette& acc);
    bool FancySliderFloat(const char* label, float* v, float vmin, float vmax,
                          const char* fmt, const AccentPalette& acc);
    bool FancySliderInt(const char* label, int* v, int vmin, int vmax,
                        const char* fmt, const AccentPalette& acc);
    bool SegmentedControl(const char* id, int* current, const char* const items[],
                          int count, const AccentPalette& acc);
    bool SubCheckbox(const char* label, bool* v, const AccentPalette& acc);
    void KeybindChip(const char* id, int& vk, const AccentPalette& acc);

    // Overlay elements
    void RenderOverlay(ClientConfig& cfg, const FrameSnapshot& snap,
                       const ModuleManager& mods, const AccentPalette& acc);
    void RenderESP(ImDrawList* dl, const ClientConfig& cfg, const FrameSnapshot& snap,
                   const AccentPalette& acc);
    void RenderCrosshairRing(ImDrawList* dl, const FrameSnapshot& snap,
                             const ModuleManager& mods, const AccentPalette& acc);
    void RenderTargetHud(ImDrawList* dl, const FrameSnapshot& snap,
                         const ModuleManager& mods, const AccentPalette& acc);
    void RenderWatermarkAndList(ImDrawList* dl, const ClientConfig& cfg,
                                const FrameSnapshot& snap, const AccentPalette& acc);

    // Fonts
    ImFont* fBrand_ = nullptr;
    ImFont* fSection_ = nullptr;
    ImFont* fTitle_ = nullptr;
    ImFont* fBody_ = nullptr;
    ImFont* fMono_ = nullptr;

    TabSection activeTab_ = TabSection::PvP;
    float menuAnim_ = 0.0f;
    uint64_t injectTimeMs_ = 0;

    // Animated switch & card expand state
    std::unordered_map<std::string, float> switchAnim_;
    std::unordered_map<std::string, bool> cardExpanded_;

    // Keybind capture
    int* activeBindTarget_ = nullptr;
    std::string activeBindId_;
    bool anyChanged_ = false;
};

} // namespace meow
