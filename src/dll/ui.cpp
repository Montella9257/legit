#include "ui.hpp"
#include "embedded_fonts.hpp"

#include <cmath>
#include <cstdio>
#include <chrono>
#include <algorithm>
#include <vector>

namespace meow {

namespace {

uint64_t UiNowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

ImU32 ToU32(const ImVec4& c, float mulAlpha = 1.0f) {
    ImVec4 v = c;
    v.w = std::clamp(v.w * mulAlpha, 0.0f, 1.0f);
    return ImGui::ColorConvertFloat4ToU32(v);
}

ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}

const AccentPalette kPalettes[] = {
    { ImVec4(0.616f, 0.482f, 1.000f, 1.0f), ImVec4(0.957f, 0.447f, 0.714f, 1.0f), ImVec4(0.66f, 0.44f, 1.00f, 0.35f), "Sakura" },
    { ImVec4(0.220f, 0.741f, 0.973f, 1.0f), ImVec4(0.506f, 0.549f, 0.973f, 1.0f), ImVec4(0.22f, 0.74f, 0.97f, 0.35f), "Cyber"  },
    { ImVec4(0.984f, 0.443f, 0.522f, 1.0f), ImVec4(0.882f, 0.114f, 0.282f, 1.0f), ImVec4(0.98f, 0.32f, 0.42f, 0.35f), "Rose"   },
    { ImVec4(0.204f, 0.827f, 0.600f, 1.0f), ImVec4(0.024f, 0.714f, 0.831f, 1.0f), ImVec4(0.18f, 0.82f, 0.65f, 0.35f), "Mint"   }
};

} // namespace

void MeowUI::InitFonts(ImGuiIO& io) {
    injectTimeMs_ = UiNowMs();

    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = false;

    const ImWchar* cyrRanges = io.Fonts->GetGlyphRangesCyrillic();

    fBrand_ = io.Fonts->AddFontFromMemoryCompressedTTF(
        fonts::UnboundedBold_compressed_data,
        static_cast<int>(fonts::UnboundedBold_compressed_size),
        20.0f, &cfg, cyrRanges);

    fSection_ = io.Fonts->AddFontFromMemoryCompressedTTF(
        fonts::UnboundedBold_compressed_data,
        static_cast<int>(fonts::UnboundedBold_compressed_size),
        15.0f, &cfg, cyrRanges);

    fTitle_ = io.Fonts->AddFontFromMemoryCompressedTTF(
        fonts::Exo2SemiBold_compressed_data,
        static_cast<int>(fonts::Exo2SemiBold_compressed_size),
        15.5f, &cfg, cyrRanges);

    fBody_ = io.Fonts->AddFontFromMemoryCompressedTTF(
        fonts::Exo2Regular_compressed_data,
        static_cast<int>(fonts::Exo2Regular_compressed_size),
        13.5f, &cfg, cyrRanges);

    fMono_ = io.Fonts->AddFontFromMemoryCompressedTTF(
        fonts::JetBrainsMonoSemiBold_compressed_data,
        static_cast<int>(fonts::JetBrainsMonoSemiBold_compressed_size),
        12.5f, &cfg, io.Fonts->GetGlyphRangesDefault());

    io.FontDefault = fTitle_ ? fTitle_ : io.Fonts->AddFontDefault();

    // Expand TriggerBot card by default for instant access
    cardExpanded_["triggerbot"] = true;
}

const AccentPalette& MeowUI::CurrentAccent(const ClientConfig& cfg) const {
    int idx = std::clamp(cfg.accentPreset, 0, 3);
    return kPalettes[idx];
}

void MeowUI::HandleKeyCapture(int vk) {
    if (!activeBindTarget_) return;
    if (vk == 0x1B || vk == 0x2E || vk == 0x08) { // ESC, DEL, BACKSPACE -> clear
        *activeBindTarget_ = 0;
    } else if (vk != 0xA1) { // Never bind menu key R-Shift
        *activeBindTarget_ = vk;
    }
    activeBindTarget_ = nullptr;
    activeBindId_.clear();
    anyChanged_ = true;
}

void MeowUI::DrawCatLogo(ImDrawList* dl, ImVec2 c, float r, ImU32 colA, ImU32 colB) {
    // Sleek geometric vector cat silhouette with pointed ears + glowing eyes
    ImVec2 pts[7] = {
        ImVec2(c.x - r * 0.85f, c.y + r * 0.25f), // left cheek
        ImVec2(c.x - r * 0.78f, c.y - r * 0.88f), // left ear tip
        ImVec2(c.x - r * 0.28f, c.y - r * 0.42f), // left inner ear
        ImVec2(c.x + r * 0.28f, c.y - r * 0.42f), // right inner ear
        ImVec2(c.x + r * 0.78f, c.y - r * 0.88f), // right ear tip
        ImVec2(c.x + r * 0.85f, c.y + r * 0.25f), // right cheek
        ImVec2(c.x,             c.y + r * 0.82f)  // chin
    };
    dl->AddConvexPolyFilled(pts, 7, IM_COL32(20, 22, 34, 240));
    dl->AddPolyline(pts, 7, colA, ImDrawFlags_Closed, 1.8f);

    // Neon slanted feline eyes
    dl->AddLine(ImVec2(c.x - r * 0.42f, c.y - r * 0.04f),
                ImVec2(c.x - r * 0.16f, c.y + r * 0.08f), colB, 2.2f);
    dl->AddLine(ImVec2(c.x + r * 0.42f, c.y - r * 0.04f),
                ImVec2(c.x + r * 0.16f, c.y + r * 0.08f), colB, 2.2f);
    // Tiny nose triangle dot
    dl->AddCircleFilled(ImVec2(c.x, c.y + r * 0.34f), r * 0.10f, colB);
}

void MeowUI::DrawTabIcon(ImDrawList* dl, TabSection tab, ImVec2 c, float s, ImU32 col) {
    if (tab == TabSection::Visual) {
        // Stylized eye / reticle
        dl->AddBezierCubic(
            ImVec2(c.x - s, c.y),
            ImVec2(c.x - s * 0.45f, c.y - s * 0.72f),
            ImVec2(c.x + s * 0.45f, c.y - s * 0.72f),
            ImVec2(c.x + s, c.y), col, 1.7f);
        dl->AddBezierCubic(
            ImVec2(c.x - s, c.y),
            ImVec2(c.x - s * 0.45f, c.y + s * 0.72f),
            ImVec2(c.x + s * 0.45f, c.y + s * 0.72f),
            ImVec2(c.x + s, c.y), col, 1.7f);
        dl->AddCircleFilled(c, s * 0.28f, col, 16);
    } else if (tab == TabSection::PveFarm) {
        // Stylized crystal / diamond sprout
        ImVec2 d[4] = {
            ImVec2(c.x, c.y - s * 0.92f),
            ImVec2(c.x + s * 0.74f, c.y),
            ImVec2(c.x, c.y + s * 0.92f),
            ImVec2(c.x - s * 0.74f, c.y)
        };
        dl->AddPolyline(d, 4, col, ImDrawFlags_Closed, 1.7f);
        dl->AddLine(ImVec2(c.x, c.y - s * 0.42f), ImVec2(c.x, c.y + s * 0.42f), col, 1.5f);
        dl->AddLine(ImVec2(c.x - s * 0.38f, c.y), ImVec2(c.x + s * 0.38f, c.y), col, 1.5f);
    } else {
        // Stylized crossed combat blades / reticle
        dl->AddLine(ImVec2(c.x - s * 0.75f, c.y + s * 0.75f), ImVec2(c.x + s * 0.75f, c.y - s * 0.75f), col, 1.8f);
        dl->AddLine(ImVec2(c.x - s * 0.75f, c.y - s * 0.75f), ImVec2(c.x + s * 0.75f, c.y + s * 0.75f), col, 1.8f);
        dl->AddCircle(c, s * 0.38f, col, 16, 1.4f);
    }
}

bool MeowUI::ToggleSwitch(const char* id, bool& value, const AccentPalette& acc) {
    ImGui::PushID(id);
    const float w = 40.0f;
    const float h = 21.0f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton("##toggle", ImVec2(w, h));
    if (clicked) {
        value = !value;
        anyChanged_ = true;
    }

    float& anim = switchAnim_[id];
    float target = value ? 1.0f : 0.0f;
    float dt = ImGui::GetIO().DeltaTime;
    anim += (target - anim) * (std::min)(1.0f, dt * 14.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float alpha = ImGui::GetStyle().Alpha;

    ImVec4 trackOff(0.14f, 0.16f, 0.23f, 1.0f);
    ImVec4 trackOn = LerpColor(acc.primary, acc.secondary, 0.35f);
    ImVec4 trackCol = LerpColor(trackOff, trackOn, anim);

    if (anim > 0.05f) {
        dl->AddRectFilled(
            ImVec2(p.x - 2, p.y - 2), ImVec2(p.x + w + 2, p.y + h + 2),
            ToU32(acc.glow, alpha * anim * 0.65f), (h + 4.0f) * 0.5f);
    }
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), ToU32(trackCol, alpha), h * 0.5f);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h),
                IM_COL32(255, 255, 255, static_cast<int>(24 * alpha)), h * 0.5f, 0, 1.0f);

    float knobR = (h - 6.0f) * 0.5f;
    float knobX = p.x + 3.0f + knobR + anim * (w - 6.0f - knobR * 2.0f);
    float knobY = p.y + h * 0.5f;
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobR,
                        IM_COL32(248, 250, 255, static_cast<int>(255 * alpha)), 20);

    ImGui::PopID();
    return clicked;
}

void MeowUI::KeybindChip(const char* id, int& vk, const AccentPalette& acc) {
    ImGui::PushID(id);
    bool capturing = (activeBindTarget_ == &vk);
    std::string label = capturing ? "..." : KeyCodeToString(vk);

    if (fMono_) ImGui::PushFont(fMono_);
    ImVec2 textSz = ImGui::CalcTextSize(label.c_str());
    float w = (std::max)(44.0f, textSz.x + 14.0f);
    float h = 20.0f;

    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton("##bind", ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    if (clicked) {
        if (capturing) {
            activeBindTarget_ = nullptr;
            activeBindId_.clear();
        } else {
            activeBindTarget_ = &vk;
            activeBindId_ = id;
        }
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        vk = 0;
        if (capturing) activeBindTarget_ = nullptr;
        anyChanged_ = true;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float alpha = ImGui::GetStyle().Alpha;
    ImU32 bg = capturing
        ? ToU32(acc.primary, 0.25f * alpha)
        : (hovered ? IM_COL32(36, 41, 60, static_cast<int>(240 * alpha))
                   : IM_COL32(24, 27, 40, static_cast<int>(220 * alpha)));
    ImU32 border = capturing
        ? ToU32(acc.secondary, alpha)
        : (vk > 0 ? ToU32(acc.primary, 0.55f * alpha) : IM_COL32(55, 62, 86, static_cast<int>(180 * alpha)));

    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, 6.0f);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), border, 6.0f, 0, 1.0f);

    ImU32 txtCol = capturing || vk > 0
        ? IM_COL32(240, 244, 255, static_cast<int>(255 * alpha))
        : IM_COL32(130, 138, 165, static_cast<int>(255 * alpha));
    dl->AddText(ImVec2(p.x + (w - textSz.x) * 0.5f, p.y + (h - textSz.y) * 0.5f - 0.5f), txtCol, label.c_str());

    if (fMono_) ImGui::PopFont();
    ImGui::PopID();
}

bool MeowUI::FancySliderFloat(const char* label, float* v, float vmin, float vmax,
                              const char* fmt, const AccentPalette& acc) {
    ImGui::PushID(label);
    float fullW = ImGui::GetContentRegionAvail().x;
    float alpha = ImGui::GetStyle().Alpha;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 p = ImGui::GetCursorScreenPos();
    char valBuf[32]{};
    std::snprintf(valBuf, sizeof(valBuf), fmt, *v);

    if (fBody_) ImGui::PushFont(fBody_);
    dl->AddText(ImVec2(p.x, p.y), IM_COL32(198, 205, 226, static_cast<int>(255 * alpha)), label);
    if (fBody_) ImGui::PopFont();

    if (fMono_) ImGui::PushFont(fMono_);
    ImVec2 valSz = ImGui::CalcTextSize(valBuf);
    dl->AddText(ImVec2(p.x + fullW - valSz.x, p.y + 1.0f),
                ToU32(acc.secondary, alpha), valBuf);
    if (fMono_) ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 18.0f));
    const float trackH = 16.0f;
    ImGui::InvisibleButton("##slider", ImVec2(fullW, trackH));
    bool active = ImGui::IsItemActive();
    bool hovered = ImGui::IsItemHovered();

    bool changed = false;
    if (active) {
        float mouseX = ImGui::GetIO().MousePos.x;
        float t = std::clamp((mouseX - p.x) / (std::max)(1.0f, fullW), 0.0f, 1.0f);
        float nv = vmin + t * (vmax - vmin);
        if (std::fabs(nv - *v) > 1e-5f) {
            *v = nv;
            changed = true;
            anyChanged_ = true;
        }
    }

    float norm = std::clamp((*v - vmin) / (std::max)(1e-5f, vmax - vmin), 0.0f, 1.0f);
    float cy = p.y + 18.0f + trackH * 0.5f;
    float barH = 4.5f;
    float fillW = fullW * norm;

    dl->AddRectFilled(ImVec2(p.x, cy - barH * 0.5f), ImVec2(p.x + fullW, cy + barH * 0.5f),
                      IM_COL32(30, 34, 50, static_cast<int>(255 * alpha)), barH * 0.5f);

    if (fillW > 2.0f) {
        dl->AddRectFilledMultiColor(
            ImVec2(p.x, cy - barH * 0.5f), ImVec2(p.x + fillW, cy + barH * 0.5f),
            ToU32(acc.primary, alpha), ToU32(acc.secondary, alpha),
            ToU32(acc.secondary, alpha), ToU32(acc.primary, alpha));
    }

    float kx = p.x + fillW;
    if (hovered || active) {
        dl->AddCircleFilled(ImVec2(kx, cy), 9.0f, ToU32(acc.glow, alpha * 0.85f), 20);
    }
    dl->AddCircleFilled(ImVec2(kx, cy), 5.5f, IM_COL32(246, 248, 255, static_cast<int>(255 * alpha)), 20);
    dl->AddCircle(ImVec2(kx, cy), 5.5f, ToU32(acc.primary, alpha), 20, 1.4f);

    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    ImGui::PopID();
    return changed;
}

bool MeowUI::FancySliderInt(const char* label, int* v, int vmin, int vmax,
                            const char* fmt, const AccentPalette& acc) {
    float fv = static_cast<float>(*v);
    float old = fv;
    ImGui::PushID(label);
    float fullW = ImGui::GetContentRegionAvail().x;
    float alpha = ImGui::GetStyle().Alpha;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 p = ImGui::GetCursorScreenPos();
    char valBuf[32]{};
    std::snprintf(valBuf, sizeof(valBuf), fmt, *v);

    if (fBody_) ImGui::PushFont(fBody_);
    dl->AddText(ImVec2(p.x, p.y), IM_COL32(198, 205, 226, static_cast<int>(255 * alpha)), label);
    if (fBody_) ImGui::PopFont();

    if (fMono_) ImGui::PushFont(fMono_);
    ImVec2 valSz = ImGui::CalcTextSize(valBuf);
    dl->AddText(ImVec2(p.x + fullW - valSz.x, p.y + 1.0f),
                ToU32(acc.secondary, alpha), valBuf);
    if (fMono_) ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 18.0f));
    const float trackH = 16.0f;
    ImGui::InvisibleButton("##slideri", ImVec2(fullW, trackH));
    bool active = ImGui::IsItemActive();
    bool hovered = ImGui::IsItemHovered();

    bool changed = false;
    if (active) {
        float mouseX = ImGui::GetIO().MousePos.x;
        float t = std::clamp((mouseX - p.x) / (std::max)(1.0f, fullW), 0.0f, 1.0f);
        int nv = static_cast<int>(std::lround(vmin + t * (vmax - vmin)));
        if (nv != *v) {
            *v = nv;
            changed = true;
            anyChanged_ = true;
        }
    }
    (void)old;

    float norm = std::clamp(static_cast<float>(*v - vmin) / (std::max)(1, vmax - vmin), 0.0f, 1.0f);
    float cy = p.y + 18.0f + trackH * 0.5f;
    float barH = 4.5f;
    float fillW = fullW * norm;

    dl->AddRectFilled(ImVec2(p.x, cy - barH * 0.5f), ImVec2(p.x + fullW, cy + barH * 0.5f),
                      IM_COL32(30, 34, 50, static_cast<int>(255 * alpha)), barH * 0.5f);
    if (fillW > 2.0f) {
        dl->AddRectFilledMultiColor(
            ImVec2(p.x, cy - barH * 0.5f), ImVec2(p.x + fillW, cy + barH * 0.5f),
            ToU32(acc.primary, alpha), ToU32(acc.secondary, alpha),
            ToU32(acc.secondary, alpha), ToU32(acc.primary, alpha));
    }
    float kx = p.x + fillW;
    if (hovered || active) {
        dl->AddCircleFilled(ImVec2(kx, cy), 9.0f, ToU32(acc.glow, alpha * 0.85f), 20);
    }
    dl->AddCircleFilled(ImVec2(kx, cy), 5.5f, IM_COL32(246, 248, 255, static_cast<int>(255 * alpha)), 20);
    dl->AddCircle(ImVec2(kx, cy), 5.5f, ToU32(acc.primary, alpha), 20, 1.4f);

    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    ImGui::PopID();
    return changed;
}

bool MeowUI::SegmentedControl(const char* id, int* current, const char* const items[],
                              int count, const AccentPalette& acc) {
    ImGui::PushID(id);
    float fullW = ImGui::GetContentRegionAvail().x;
    float h = 28.0f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float alpha = ImGui::GetStyle().Alpha;

    dl->AddRectFilled(p, ImVec2(p.x + fullW, p.y + h),
                      IM_COL32(18, 21, 32, static_cast<int>(255 * alpha)), 8.0f);
    dl->AddRect(p, ImVec2(p.x + fullW, p.y + h),
                IM_COL32(42, 48, 70, static_cast<int>(200 * alpha)), 8.0f, 0, 1.0f);

    float segW = fullW / static_cast<float>(count);
    bool changed = false;
    if (fBody_) ImGui::PushFont(fBody_);

    for (int i = 0; i < count; ++i) {
        ImVec2 sp(p.x + segW * i + 2.0f, p.y + 2.0f);
        ImVec2 ep(p.x + segW * (i + 1) - 2.0f, p.y + h - 2.0f);
        ImGui::SetCursorScreenPos(sp);
        if (ImGui::InvisibleButton(items[i], ImVec2(ep.x - sp.x, ep.y - sp.y))) {
            *current = i;
            changed = true;
            anyChanged_ = true;
        }
        bool sel = (*current == i);
        bool hov = ImGui::IsItemHovered();
        if (sel) {
            dl->AddRectFilledMultiColor(sp, ep,
                ToU32(acc.primary, 0.85f * alpha), ToU32(acc.secondary, 0.85f * alpha),
                ToU32(acc.secondary, 0.72f * alpha), ToU32(acc.primary, 0.72f * alpha));
        } else if (hov) {
            dl->AddRectFilled(sp, ep, IM_COL32(34, 38, 54, static_cast<int>(180 * alpha)), 6.0f);
        }
        ImVec2 tsz = ImGui::CalcTextSize(items[i]);
        dl->AddText(ImVec2(sp.x + ((ep.x - sp.x) - tsz.x) * 0.5f,
                           sp.y + ((ep.y - sp.y) - tsz.y) * 0.5f - 0.5f),
                    sel ? IM_COL32(255, 255, 255, static_cast<int>(255 * alpha))
                        : IM_COL32(150, 158, 185, static_cast<int>(255 * alpha)),
                    items[i]);
    }

    if (fBody_) ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + 6.0f));
    ImGui::PopID();
    return changed;
}

bool MeowUI::SubCheckbox(const char* label, bool* v, const AccentPalette& acc) {
    ImGui::PushID(label);
    float fullW = ImGui::GetContentRegionAvail().x;
    float h = 22.0f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton("##chk", ImVec2(fullW, h));
    if (clicked) {
        *v = !*v;
        anyChanged_ = true;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float alpha = ImGui::GetStyle().Alpha;
    if (fBody_) ImGui::PushFont(fBody_);
    dl->AddText(ImVec2(p.x, p.y + 3.0f),
                *v ? IM_COL32(228, 234, 250, static_cast<int>(255 * alpha))
                   : IM_COL32(155, 163, 188, static_cast<int>(255 * alpha)),
                label);
    if (fBody_) ImGui::PopFont();

    float boxSz = 15.0f;
    ImVec2 bp(p.x + fullW - boxSz, p.y + (h - boxSz) * 0.5f);
    if (*v) {
        dl->AddRectFilled(bp, ImVec2(bp.x + boxSz, bp.y + boxSz), ToU32(acc.primary, alpha), 4.0f);
        dl->AddLine(ImVec2(bp.x + 3.5f, bp.y + 7.5f), ImVec2(bp.x + 6.5f, bp.y + 10.8f),
                    IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), 1.7f);
        dl->AddLine(ImVec2(bp.x + 6.5f, bp.y + 10.8f), ImVec2(bp.x + 11.5f, bp.y + 4.2f),
                    IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), 1.7f);
    } else {
        dl->AddRectFilled(bp, ImVec2(bp.x + boxSz, bp.y + boxSz),
                          IM_COL32(22, 25, 36, static_cast<int>(255 * alpha)), 4.0f);
        dl->AddRect(bp, ImVec2(bp.x + boxSz, bp.y + boxSz),
                    IM_COL32(60, 68, 94, static_cast<int>(220 * alpha)), 4.0f, 0, 1.0f);
    }
    ImGui::PopID();
    return clicked;
}

bool MeowUI::BeginModuleCard(const char* id, const char* title, const char* desc,
                             bool& enabled, int& keybind, const AccentPalette& acc,
                             bool hasSettings) {
    ImGui::PushID(id);
    float w = ImGui::GetContentRegionAvail().x;
    float alpha = ImGui::GetStyle().Alpha;
    bool expanded = hasSettings ? cardExpanded_[id] : false;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 11.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.078f, 0.088f, 0.129f, 0.96f * alpha));
    ImGui::PushStyleColor(ImGuiCol_Border, enabled
        ? ImVec4(acc.primary.x, acc.primary.y, acc.primary.z, 0.55f * alpha)
        : ImVec4(0.145f, 0.165f, 0.235f, 0.90f * alpha));

    ImGuiChildFlags flags = ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY;
    ImGui::BeginChild(id, ImVec2(w, 0.0f), flags);

    // Header row
    ImVec2 hp = ImGui::GetCursorScreenPos();
    float availW = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Accent status indicator bar on left
    if (enabled) {
        dl->AddRectFilled(ImVec2(hp.x - 14.0f, hp.y + 2.0f),
                          ImVec2(hp.x - 10.5f, hp.y + 34.0f),
                          ToU32(acc.secondary, alpha), 2.0f);
    }

    if (fTitle_) ImGui::PushFont(fTitle_);
    dl->AddText(ImVec2(hp.x, hp.y),
                enabled ? IM_COL32(246, 248, 255, static_cast<int>(255 * alpha))
                        : IM_COL32(204, 210, 232, static_cast<int>(255 * alpha)),
                title);
    if (fTitle_) ImGui::PopFont();

    if (fBody_) ImGui::PushFont(fBody_);
    dl->AddText(ImVec2(hp.x, hp.y + 19.0f),
                IM_COL32(122, 130, 156, static_cast<int>(255 * alpha)), desc);
    if (fBody_) ImGui::PopFont();

    // Controls on right: [Expand] [Bind] [Toggle]
    float rightX = hp.x + availW;
    float toggleW = 40.0f;
    float bindW = (std::max)(44.0f, ImGui::CalcTextSize(
        (activeBindTarget_ == &keybind) ? "..." : KeyCodeToString(keybind).c_str()).x + 14.0f);
    float expW = hasSettings ? 22.0f : 0.0f;

    ImGui::SetCursorScreenPos(ImVec2(rightX - toggleW, hp.y + 7.0f));
    ToggleSwitch((std::string(id) + "_sw").c_str(), enabled, acc);

    ImGui::SetCursorScreenPos(ImVec2(rightX - toggleW - 8.0f - bindW, hp.y + 7.5f));
    KeybindChip((std::string(id) + "_kb").c_str(), keybind, acc);

    if (hasSettings) {
        ImVec2 ep(rightX - toggleW - 8.0f - bindW - 6.0f - expW, hp.y + 7.0f);
        ImGui::SetCursorScreenPos(ep);
        if (ImGui::InvisibleButton("##exp", ImVec2(expW, 21.0f))) {
            cardExpanded_[id] = !cardExpanded_[id];
            expanded = cardExpanded_[id];
        }
        bool eHov = ImGui::IsItemHovered();
        ImU32 chevCol = (expanded || eHov)
            ? ToU32(acc.secondary, alpha)
            : IM_COL32(128, 136, 162, static_cast<int>(255 * alpha));
        ImVec2 cc(ep.x + expW * 0.5f, ep.y + 10.5f);
        if (expanded) {
            dl->AddLine(ImVec2(cc.x - 4.0f, cc.y + 2.0f), ImVec2(cc.x, cc.y - 2.5f), chevCol, 1.7f);
            dl->AddLine(ImVec2(cc.x, cc.y - 2.5f), ImVec2(cc.x + 4.0f, cc.y + 2.0f), chevCol, 1.7f);
        } else {
            dl->AddLine(ImVec2(cc.x - 4.0f, cc.y - 2.0f), ImVec2(cc.x, cc.y + 2.5f), chevCol, 1.7f);
            dl->AddLine(ImVec2(cc.x, cc.y + 2.5f), ImVec2(cc.x + 4.0f, cc.y - 2.0f), chevCol, 1.7f);
        }
    }

    // Also allow clicking title area to expand/collapse settings
    if (hasSettings) {
        float headerTouchW = availW - toggleW - bindW - expW - 22.0f;
        if (headerTouchW > 20.0f) {
            ImGui::SetCursorScreenPos(hp);
            if (ImGui::InvisibleButton("##hdr_click", ImVec2(headerTouchW, 36.0f))) {
                cardExpanded_[id] = !cardExpanded_[id];
                expanded = cardExpanded_[id];
            }
        }
    }

    ImGui::SetCursorScreenPos(ImVec2(hp.x, hp.y + 40.0f));

    if (expanded) {
        dl->AddLine(ImVec2(hp.x, hp.y + 41.0f), ImVec2(hp.x + availW, hp.y + 41.0f),
                    IM_COL32(34, 38, 56, static_cast<int>(220 * alpha)), 1.0f);
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        return true;
    }

    return false;
}

void MeowUI::EndModuleCard() {
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PopID();
}

void MeowUI::RenderESP(ImDrawList* dl, const ClientConfig& cfg, const FrameSnapshot& snap,
                       const AccentPalette& acc) {
    if ((!cfg.espEnabled && !cfg.tracersEnabled) || !snap.inWorld || snap.screenW <= 0 || snap.screenH <= 0)
        return;

    ImVec2 screenCenterBottom(snap.screenW * 0.5f, snap.screenH * 0.55f);

    for (const auto& p : snap.players) {
        if (!p.alive || p.spectator) continue;
        if (cfg.espIgnoreInvis && p.invisible) continue;

        if (cfg.tracersEnabled && p.distance <= cfg.tracersMaxDist) {
            Vec3 chest {
                (p.box.minX + p.box.maxX) * 0.5,
                p.box.minY + (p.box.maxY - p.box.minY) * 0.55,
                (p.box.minZ + p.box.maxZ) * 0.5
            };
            ScreenPos sp;
            if (WorldToScreen(chest, snap.camPos, snap.camYaw, snap.camPitch,
                              snap.fovDeg, snap.screenW, snap.screenH, sp)) {
                float distFade = std::clamp(1.0f - (p.distance / cfg.tracersMaxDist), 0.2f, 0.95f);
                ImU32 lineCol = ToU32(p.sameTeam ? ImVec4(0.32f, 0.85f, 0.55f, 1.0f) : acc.secondary, distFade);
                dl->AddLine(screenCenterBottom, ImVec2(sp.x, sp.y), lineCol, 1.25f);
            }
        }

        if (!cfg.espEnabled) continue;

        Vec3 corners[8] = {
            { p.box.minX, p.box.minY, p.box.minZ },
            { p.box.maxX, p.box.minY, p.box.minZ },
            { p.box.minX, p.box.maxY, p.box.minZ },
            { p.box.maxX, p.box.maxY, p.box.minZ },
            { p.box.minX, p.box.minY, p.box.maxZ },
            { p.box.maxX, p.box.minY, p.box.maxZ },
            { p.box.minX, p.box.maxY, p.box.maxZ },
            { p.box.maxX, p.box.maxY, p.box.maxZ }
        };

        float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
        int visibleCount = 0;
        for (const auto& c : corners) {
            ScreenPos sp;
            if (WorldToScreen(c, snap.camPos, snap.camYaw, snap.camPitch,
                              snap.fovDeg, snap.screenW, snap.screenH, sp)) {
                minX = (std::min)(minX, sp.x);
                minY = (std::min)(minY, sp.y);
                maxX = (std::max)(maxX, sp.x);
                maxY = (std::max)(maxY, sp.y);
                ++visibleCount;
            }
        }
        if (visibleCount < 2 || maxX - minX < 2.0f || maxY - minY < 2.0f) continue;

        ImVec4 boxCol(cfg.espColor[0], cfg.espColor[1], cfg.espColor[2], cfg.espColor[3]);
        if (p.hurtTime > 0) boxCol = ImVec4(1.0f, 0.30f, 0.42f, 1.0f);
        else if (p.sameTeam) boxCol = ImVec4(0.28f, 0.88f, 0.56f, 1.0f);

        if (cfg.espFill) {
            dl->AddRectFilled(ImVec2(minX, minY), ImVec2(maxX, maxY), ToU32(boxCol, 0.12f), 2.0f);
        }

        ImU32 uBox = ToU32(boxCol, 0.95f);
        if (cfg.espCorners) {
            float cw = (maxX - minX) * 0.24f;
            float ch = (maxY - minY) * 0.22f;
            // Top-Left
            dl->AddLine(ImVec2(minX, minY), ImVec2(minX + cw, minY), uBox, 1.8f);
            dl->AddLine(ImVec2(minX, minY), ImVec2(minX, minY + ch), uBox, 1.8f);
            // Top-Right
            dl->AddLine(ImVec2(maxX - cw, minY), ImVec2(maxX, minY), uBox, 1.8f);
            dl->AddLine(ImVec2(maxX, minY), ImVec2(maxX, minY + ch), uBox, 1.8f);
            // Bottom-Left
            dl->AddLine(ImVec2(minX, maxY - ch), ImVec2(minX, maxY), uBox, 1.8f);
            dl->AddLine(ImVec2(minX, maxY), ImVec2(minX + cw, maxY), uBox, 1.8f);
            // Bottom-Right
            dl->AddLine(ImVec2(maxX, maxY - ch), ImVec2(maxX, maxY), uBox, 1.8f);
            dl->AddLine(ImVec2(maxX - cw, maxY), ImVec2(maxX, maxY), uBox, 1.8f);
        } else {
            dl->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), uBox, 2.0f, 0, 1.5f);
        }

        if (cfg.espHealthBar) {
            float hpRatio = std::clamp(p.health / (std::max)(1.0f, p.maxHealth), 0.0f, 1.0f);
            float barX = minX - 6.0f;
            float barH = (maxY - minY) * hpRatio;
            dl->AddRectFilled(ImVec2(barX, minY), ImVec2(barX + 3.0f, maxY), IM_COL32(10, 12, 18, 200), 1.5f);
            ImU32 hpCol = ToU32(LerpColor(ImVec4(0.98f, 0.28f, 0.42f, 1.0f), ImVec4(0.25f, 0.92f, 0.62f, 1.0f), hpRatio));
            dl->AddRectFilled(ImVec2(barX, maxY - barH), ImVec2(barX + 3.0f, maxY), hpCol, 1.5f);
        }

        if (cfg.espNames || cfg.espDistance) {
            char tag[128]{};
            if (cfg.espNames && cfg.espDistance) {
                std::snprintf(tag, sizeof(tag), "%s  %.1fm", p.name.c_str(), p.distance);
            } else if (cfg.espNames) {
                std::snprintf(tag, sizeof(tag), "%s", p.name.c_str());
            } else {
                std::snprintf(tag, sizeof(tag), "%.1fm", p.distance);
            }
            if (fTitle_) ImGui::PushFont(fTitle_);
            ImVec2 tsz = ImGui::CalcTextSize(tag);
            float tx = (minX + maxX) * 0.5f - tsz.x * 0.5f;
            float ty = minY - tsz.y - 7.0f;
            dl->AddRectFilled(ImVec2(tx - 6.0f, ty - 2.0f), ImVec2(tx + tsz.x + 6.0f, ty + tsz.y + 2.0f),
                              IM_COL32(10, 12, 18, 195), 5.0f);
            dl->AddText(ImVec2(tx, ty), IM_COL32(244, 246, 255, 255), tag);
            if (fTitle_) ImGui::PopFont();
        }
    }
}

void MeowUI::RenderCrosshairRing(ImDrawList* dl, const FrameSnapshot& snap,
                                 const ModuleManager& mods, const AccentPalette& acc) {
    if (!snap.inWorld || snap.inScreen || snap.screenW <= 0 || snap.screenH <= 0) return;

    ImVec2 c(snap.screenW * 0.5f, snap.screenH * 0.5f);
    float r = 15.0f;

    // Background faint ring
    dl->AddCircle(c, r, IM_COL32(14, 16, 24, 130), 32, 2.2f);

    float cd = std::clamp(snap.attackCooldown, 0.0f, 1.0f);
    if (cd > 0.01f) {
        float start = -3.1415926f * 0.5f;
        float end = start + 2.0f * 3.1415926f * cd;
        dl->PathClear();
        dl->PathArcTo(c, r, start, end, 28);
        ImU32 ringCol = snap.canCritNow ? ToU32(acc.secondary, 0.95f)
                        : (cd >= 0.90f ? ToU32(acc.primary, 0.92f) : IM_COL32(160, 170, 200, 190));
        dl->PathStroke(ringCol, 0, 2.2f);
    }

    if (snap.canCritNow && snap.attackCooldown >= 0.88f) {
        if (fMono_) ImGui::PushFont(fMono_);
        const char* txt = "CRIT";
        ImVec2 tsz = ImGui::CalcTextSize(txt);
        dl->AddText(ImVec2(c.x - tsz.x * 0.5f, c.y + r + 5.0f), ToU32(acc.secondary, 0.95f), txt);
        if (fMono_) ImGui::PopFont();
    }

    uint64_t now = UiNowMs();
    if (now - mods.LastCritHitMs() < 260) {
        float k = 1.0f - static_cast<float>(now - mods.LastCritHitMs()) / 260.0f;
        dl->AddCircle(c, r + (1.0f - k) * 9.0f, ToU32(acc.secondary, k * 0.85f), 28, 1.8f);
    }
}

void MeowUI::RenderTargetHud(ImDrawList* dl, const FrameSnapshot& snap,
                             const ModuleManager& mods, const AccentPalette& acc) {
    const auto& th = mods.GetTargetHud();
    if (!th.active || snap.screenW <= 0 || snap.screenH <= 0) return;

    float w = 228.0f;
    float h = 64.0f;
    float x = snap.screenW * 0.5f + 28.0f;
    float y = snap.screenH * 0.5f + 22.0f;

    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(11, 13, 20, 228), 11.0f);
    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h),
                th.hurtTime > 0 ? IM_COL32(255, 85, 115, 220) : ToU32(acc.primary, 0.65f), 11.0f, 0, 1.2f);

    if (fTitle_) ImGui::PushFont(fTitle_);
    dl->AddText(ImVec2(x + 12.0f, y + 9.0f), IM_COL32(244, 246, 255, 255), th.name.c_str());
    if (fTitle_) ImGui::PopFont();

    char meta[48]{};
    std::snprintf(meta, sizeof(meta), "%.1f HP • %.1fm", th.health, th.distance);
    if (fMono_) ImGui::PushFont(fMono_);
    ImVec2 msz = ImGui::CalcTextSize(meta);
    dl->AddText(ImVec2(x + w - msz.x - 12.0f, y + 11.0f), ToU32(acc.secondary, 0.95f), meta);
    if (fMono_) ImGui::PopFont();

    // Smooth HP bar
    float barX = x + 12.0f;
    float barY = y + 36.0f;
    float barW = w - 24.0f;
    float barH = 8.0f;
    float ratio = std::clamp(th.animatedHealth / (std::max)(1.0f, th.maxHealth), 0.0f, 1.0f);

    dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(24, 28, 42, 255), 4.0f);
    if (barW * ratio > 2.0f) {
        dl->AddRectFilledMultiColor(
            ImVec2(barX, barY), ImVec2(barX + barW * ratio, barY + barH),
            ToU32(acc.primary, 1.0f), ToU32(acc.secondary, 1.0f),
            ToU32(acc.secondary, 1.0f), ToU32(acc.primary, 1.0f));
    }

    if (th.canCrit) {
        if (fMono_) ImGui::PushFont(fMono_);
        dl->AddText(ImVec2(x + 12.0f, y + 48.0f), ToU32(acc.secondary, 0.95f), "CRIT WINDOW");
        if (fMono_) ImGui::PopFont();
    }
}

void MeowUI::RenderWatermarkAndList(ImDrawList* dl, const ClientConfig& cfg,
                                    const FrameSnapshot& snap, const AccentPalette& acc) {
    if (snap.screenW <= 0 || snap.screenH <= 0) return;

    if (cfg.showWatermark) {
        char wm[96]{};
        std::snprintf(wm, sizeof(wm), "MEOW.DLL  |  1.21.4 FABRIC  |  %d FPS", snap.fps);
        if (fMono_) ImGui::PushFont(fMono_);
        ImVec2 tsz = ImGui::CalcTextSize(wm);
        float px = 14.0f, py = 14.0f;
        float pw = tsz.x + 34.0f, ph = 28.0f;

        dl->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + ph), IM_COL32(10, 12, 19, 215), 8.0f);
        dl->AddRect(ImVec2(px, py), ImVec2(px + pw, py + ph), ToU32(acc.primary, 0.48f), 8.0f, 0, 1.0f);
        dl->AddRectFilledMultiColor(
            ImVec2(px + 2.0f, py + 5.0f), ImVec2(px + 5.0f, py + ph - 5.0f),
            ToU32(acc.primary, 1.0f), ToU32(acc.primary, 1.0f),
            ToU32(acc.secondary, 1.0f), ToU32(acc.secondary, 1.0f));
        dl->AddText(ImVec2(px + 14.0f, py + (ph - tsz.y) * 0.5f), IM_COL32(238, 242, 255, 255), wm);
        if (fMono_) ImGui::PopFont();
    }

    if (cfg.showArrayList) {
        std::vector<std::string> active;
        if (cfg.triggerBotEnabled) active.push_back("TriggerBot");
        if (cfg.aimAssistEnabled)  active.push_back("AimAssist");
        if (cfg.sprintResetEnabled)active.push_back("SprintReset");
        if (cfg.jumpResetEnabled)  active.push_back("JumpReset");
        if (cfg.mobTriggerEnabled) active.push_back("MobTrigger");
        if (cfg.autoFishEnabled)   active.push_back("AutoFish");
        if (cfg.autoSprintEnabled) active.push_back("AutoSprint");
        if (cfg.autoEatEnabled)    active.push_back("AutoEat");
        if (cfg.espEnabled)        active.push_back("PlayerESP");
        if (cfg.tracersEnabled)    active.push_back("Tracers");
        if (cfg.fullbrightEnabled) active.push_back("Fullbright");

        if (fTitle_) ImGui::PushFont(fTitle_);
        std::sort(active.begin(), active.end(), [](const std::string& a, const std::string& b) {
            return ImGui::CalcTextSize(a.c_str()).x > ImGui::CalcTextSize(b.c_str()).x;
        });

        float y = 14.0f;
        for (size_t i = 0; i < active.size(); ++i) {
            ImVec2 tsz = ImGui::CalcTextSize(active[i].c_str());
            float pw = tsz.x + 18.0f;
            float ph = 22.0f;
            float px = static_cast<float>(snap.screenW) - pw - 14.0f;
            ImVec4 col = LerpColor(acc.primary, acc.secondary,
                active.size() > 1 ? static_cast<float>(i) / static_cast<float>(active.size() - 1) : 0.0f);

            dl->AddRectFilled(ImVec2(px, y), ImVec2(px + pw, y + ph), IM_COL32(10, 12, 19, 200), 6.0f);
            dl->AddRectFilled(ImVec2(px + pw - 3.0f, y + 3.0f), ImVec2(px + pw, y + ph - 3.0f), ToU32(col, 1.0f), 1.5f);
            dl->AddText(ImVec2(px + 8.0f, y + (ph - tsz.y) * 0.5f - 0.5f), IM_COL32(240, 244, 255, 255), active[i].c_str());
            y += ph + 4.0f;
        }
        if (fTitle_) ImGui::PopFont();
    }
}

void MeowUI::RenderOverlay(ClientConfig& cfg, const FrameSnapshot& snap,
                           const ModuleManager& mods, const AccentPalette& acc) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    RenderESP(dl, cfg, snap, acc);
    if (cfg.crosshairRingEnabled) RenderCrosshairRing(dl, snap, mods, acc);
    if (cfg.targetHudEnabled) RenderTargetHud(dl, snap, mods, acc);
    RenderWatermarkAndList(dl, cfg, snap, acc);

    // 5.5s Injection confirmation toast (bottom-right)
    uint64_t age = UiNowMs() - injectTimeMs_;
    if (age < 5500 && snap.screenW > 0 && snap.screenH > 0) {
        float fade = 1.0f;
        if (age < 350) fade = static_cast<float>(age) / 350.0f;
        else if (age > 4700) fade = static_cast<float>(5500 - age) / 800.0f;
        fade = std::clamp(fade, 0.0f, 1.0f);

        const char* msg = "MEOW.DLL INJECTED  •  PRESS R-SHIFT";
        if (fMono_) ImGui::PushFont(fMono_);
        ImVec2 tsz = ImGui::CalcTextSize(msg);
        float pw = tsz.x + 34.0f, ph = 34.0f;
        float px = snap.screenW - pw - 18.0f;
        float py = snap.screenH - ph - 18.0f + (1.0f - fade) * 14.0f;

        dl->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + ph),
                          IM_COL32(11, 13, 21, static_cast<int>(235 * fade)), 9.0f);
        dl->AddRect(ImVec2(px, py), ImVec2(px + pw, py + ph),
                    ToU32(acc.secondary, 0.85f * fade), 9.0f, 0, 1.3f);
        dl->AddCircleFilled(ImVec2(px + 14.0f, py + ph * 0.5f), 4.0f,
                            ToU32(acc.primary, fade), 12);
        dl->AddText(ImVec2(px + 24.0f, py + (ph - tsz.y) * 0.5f),
                    IM_COL32(246, 248, 255, static_cast<int>(255 * fade)), msg);
        if (fMono_) ImGui::PopFont();
    }
}

void MeowUI::Render(ClientConfig& cfg, const FrameSnapshot& snap, const ModuleManager& mods,
                    const std::string& bridgeStatus, bool menuOpen, bool& requestSave) {
    anyChanged_ = false;
    const AccentPalette& acc = CurrentAccent(cfg);

    RenderOverlay(cfg, snap, mods, acc);

    // Smooth menu fade/scale transition
    float dt = ImGui::GetIO().DeltaTime;
    float targetAnim = menuOpen ? 1.0f : 0.0f;
    menuAnim_ += (targetAnim - menuAnim_) * (std::min)(1.0f, dt * 12.0f);
    if (menuAnim_ < 0.01f) {
        menuAnim_ = 0.0f;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const float winW = 790.0f;
    const float winH = 520.0f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, menuAnim_);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.2f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.039f, 0.043f, 0.067f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(acc.primary.x, acc.primary.y, acc.primary.z, 0.42f));

    ImGuiWindowFlags wFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("##MeowClientMenu", nullptr, wFlags)) {
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float alpha = ImGui::GetStyle().Alpha;

        const float sidebarW = 196.0f;

        // Sidebar dark surface + subtle top-left radial glow
        dl->AddRectFilled(wp, ImVec2(wp.x + sidebarW, wp.y + ws.y),
                          IM_COL32(12, 14, 22, static_cast<int>(250 * alpha)), 16.0f, ImDrawFlags_RoundCornersLeft);
        dl->AddRectFilledMultiColor(
            wp, ImVec2(wp.x + sidebarW, wp.y + 110.0f),
            ToU32(acc.primary, 0.14f * alpha), ToU32(acc.secondary, 0.06f * alpha),
            IM_COL32(12, 14, 22, 0), IM_COL32(12, 14, 22, 0));
        dl->AddLine(ImVec2(wp.x + sidebarW, wp.y + 12.0f),
                    ImVec2(wp.x + sidebarW, wp.y + ws.y - 12.0f),
                    IM_COL32(34, 38, 56, static_cast<int>(220 * alpha)), 1.0f);

        // Brand Logo + MEOW.DLL
        DrawCatLogo(dl, ImVec2(wp.x + 32.0f, wp.y + 36.0f), 14.5f,
                    ToU32(acc.primary, alpha), ToU32(acc.secondary, alpha));
        if (fBrand_) ImGui::PushFont(fBrand_);
        dl->AddText(ImVec2(wp.x + 54.0f, wp.y + 21.0f),
                    IM_COL32(248, 250, 255, static_cast<int>(255 * alpha)), "MEOW");
        if (fBrand_) ImGui::PopFont();

        if (fMono_) ImGui::PushFont(fMono_);
        dl->AddText(ImVec2(wp.x + 55.0f, wp.y + 44.0f),
                    ToU32(acc.secondary, alpha), "DLL // 1.21.4");
        if (fMono_) ImGui::PopFont();

        // Navigation Sections: Visual, PveFarm, PvP
        struct NavItem { TabSection sec; const char* title; const char* sub; };
        const NavItem navItems[3] = {
            { TabSection::Visual,  "Visual",  "ESP & HUD" },
            { TabSection::PveFarm, "PveFarm", "Grind & Auto" },
            { TabSection::PvP,     "PvP",     "Combat & Crits" }
        };

        float navY = wp.y + 88.0f;
        for (int i = 0; i < 3; ++i) {
            ImVec2 btnPos(wp.x + 14.0f, navY + i * 54.0f);
            ImVec2 btnSz(sidebarW - 28.0f, 44.0f);
            ImGui::SetCursorScreenPos(btnPos);
            if (ImGui::InvisibleButton(navItems[i].title, btnSz)) {
                activeTab_ = navItems[i].sec;
            }
            bool sel = (activeTab_ == navItems[i].sec);
            bool hov = ImGui::IsItemHovered();

            if (sel) {
                dl->AddRectFilledMultiColor(
                    btnPos, ImVec2(btnPos.x + btnSz.x, btnPos.y + btnSz.y),
                    ToU32(acc.primary, 0.22f * alpha), ToU32(acc.secondary, 0.14f * alpha),
                    ToU32(acc.secondary, 0.10f * alpha), ToU32(acc.primary, 0.18f * alpha));
                dl->AddRect(btnPos, ImVec2(btnPos.x + btnSz.x, btnPos.y + btnSz.y),
                            ToU32(acc.primary, 0.55f * alpha), 10.0f, 0, 1.0f);
                dl->AddRectFilled(ImVec2(btnPos.x, btnPos.y + 9.0f),
                                  ImVec2(btnPos.x + 3.5f, btnPos.y + btnSz.y - 9.0f),
                                  ToU32(acc.secondary, alpha), 2.0f);
            } else if (hov) {
                dl->AddRectFilled(btnPos, ImVec2(btnPos.x + btnSz.x, btnPos.y + btnSz.y),
                                  IM_COL32(28, 32, 48, static_cast<int>(180 * alpha)), 10.0f);
            }

            ImU32 iconCol = sel ? ToU32(acc.secondary, alpha)
                                : IM_COL32(138, 146, 174, static_cast<int>(255 * alpha));
            DrawTabIcon(dl, navItems[i].sec, ImVec2(btnPos.x + 22.0f, btnPos.y + btnSz.y * 0.5f), 8.5f, iconCol);

            if (fTitle_) ImGui::PushFont(fTitle_);
            dl->AddText(ImVec2(btnPos.x + 40.0f, btnPos.y + 7.0f),
                        sel ? IM_COL32(248, 250, 255, static_cast<int>(255 * alpha))
                            : IM_COL32(184, 192, 218, static_cast<int>(255 * alpha)),
                        navItems[i].title);
            if (fTitle_) ImGui::PopFont();

            if (fBody_) ImGui::PushFont(fBody_);
            dl->AddText(ImVec2(btnPos.x + 40.0f, btnPos.y + 23.0f),
                        IM_COL32(106, 114, 140, static_cast<int>(255 * alpha)),
                        navItems[i].sub);
            if (fBody_) ImGui::PopFont();
        }

        // Accent Color Swatches + Status Footer inside Sidebar
        float footY = wp.y + ws.y - 118.0f;
        dl->AddLine(ImVec2(wp.x + 16.0f, footY), ImVec2(wp.x + sidebarW - 16.0f, footY),
                    IM_COL32(34, 38, 56, static_cast<int>(200 * alpha)), 1.0f);

        if (fBody_) ImGui::PushFont(fBody_);
        dl->AddText(ImVec2(wp.x + 18.0f, footY + 10.0f),
                    IM_COL32(128, 136, 162, static_cast<int>(255 * alpha)), "Theme Accent");
        if (fBody_) ImGui::PopFont();

        for (int i = 0; i < 4; ++i) {
            ImVec2 sc(wp.x + 28.0f + i * 28.0f, footY + 38.0f);
            ImGui::SetCursorScreenPos(ImVec2(sc.x - 10.0f, sc.y - 10.0f));
            char sid[16]{};
            std::snprintf(sid, sizeof(sid), "##sw_%d", i);
            if (ImGui::InvisibleButton(sid, ImVec2(20.0f, 20.0f))) {
                cfg.accentPreset = i;
                anyChanged_ = true;
            }
            dl->AddCircleFilled(sc, 7.5f, ToU32(kPalettes[i].primary, alpha), 18);
            if (cfg.accentPreset == i) {
                dl->AddCircle(sc, 10.0f, IM_COL32(255, 255, 255, static_cast<int>(230 * alpha)), 20, 1.5f);
            }
        }

        // Live JVM / Fabric status pill
        ImVec2 stPos(wp.x + 14.0f, wp.y + ws.y - 54.0f);
        dl->AddRectFilled(stPos, ImVec2(wp.x + sidebarW - 14.0f, stPos.y + 40.0f),
                          IM_COL32(18, 21, 32, static_cast<int>(240 * alpha)), 9.0f);
        ImU32 dotCol = snap.ready
            ? (snap.inWorld ? IM_COL32(52, 211, 153, static_cast<int>(255 * alpha))
                            : IM_COL32(251, 191, 36, static_cast<int>(255 * alpha)))
            : IM_COL32(248, 113, 113, static_cast<int>(255 * alpha));
        dl->AddCircleFilled(ImVec2(stPos.x + 12.0f, stPos.y + 14.0f), 4.0f, dotCol, 12);

        if (fMono_) ImGui::PushFont(fMono_);
        dl->AddText(ImVec2(stPos.x + 22.0f, stPos.y + 7.5f),
                    IM_COL32(226, 232, 248, static_cast<int>(255 * alpha)),
                    snap.ready ? (snap.inWorld ? "IN WORLD" : "IN MENU") : "NO JVM");
        dl->AddText(ImVec2(stPos.x + 11.0f, stPos.y + 22.5f),
                    IM_COL32(115, 124, 150, static_cast<int>(255 * alpha)),
                    "TOGGLE: R-SHIFT");
        if (fMono_) ImGui::PopFont();

        // Main Content Pane
        ImVec2 contentPos(wp.x + sidebarW + 20.0f, wp.y + 18.0f);
        ImVec2 contentSz(ws.x - sidebarW - 36.0f, ws.y - 34.0f);

        // Section Header
        const char* secHead = activeTab_ == TabSection::Visual ? "VISUAL OVERLAY"
                            : activeTab_ == TabSection::PveFarm ? "PVE FARMING"
                                                                : "PVP COMBAT";
        const char* secSub  = activeTab_ == TabSection::Visual ? "Anti-Minecraft OpenGL overlay & ESP"
                            : activeTab_ == TabSection::PveFarm ? "Legit PvE grinding & quality-of-life"
                                                                : "Fast hits, smart crits & legit combat";

        if (fSection_) ImGui::PushFont(fSection_);
        dl->AddText(contentPos, IM_COL32(248, 250, 255, static_cast<int>(255 * alpha)), secHead);
        if (fSection_) ImGui::PopFont();

        if (fBody_) ImGui::PushFont(fBody_);
        dl->AddText(ImVec2(contentPos.x, contentPos.y + 22.0f),
                    IM_COL32(124, 132, 158, static_cast<int>(255 * alpha)), secSub);
        if (fBody_) ImGui::PopFont();

        // Top-right HUD toggles quick bar
        ImGui::SetCursorScreenPos(ImVec2(wp.x + ws.x - 186.0f, contentPos.y + 4.0f));
        ImGui::BeginGroup();
        if (fMono_) ImGui::PushFont(fMono_);
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(24, 28, 42, 230));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(36, 42, 62, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        if (ImGui::Button("SAVE CFG", ImVec2(82.0f, 26.0f))) requestSave = true;
        ImGui::SameLine();
        if (ImGui::Button(cfg.showWatermark ? "HUD: ON" : "HUD: OFF", ImVec2(78.0f, 26.0f))) {
            cfg.showWatermark = !cfg.showWatermark;
            cfg.showArrayList = cfg.showWatermark;
            anyChanged_ = true;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        if (fMono_) ImGui::PopFont();
        ImGui::EndGroup();

        // Scrollable modules region
        ImGui::SetCursorScreenPos(ImVec2(contentPos.x, contentPos.y + 50.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(acc.primary.x, acc.primary.y, acc.primary.z, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(acc.secondary.x, acc.secondary.y, acc.secondary.z, 0.6f));
        ImGui::BeginChild("##ModulesScroll", ImVec2(contentSz.x, contentSz.y - 50.0f), false);

        if (activeTab_ == TabSection::Visual) {
            if (BeginModuleCard("player_esp", "Player ESP", "2D corner brackets, HP bar & nametags",
                                cfg.espEnabled, cfg.espBind, acc)) {
                SubCheckbox("Corner Brackets", &cfg.espCorners, acc);
                SubCheckbox("Translucent Fill", &cfg.espFill, acc);
                SubCheckbox("Health Bar", &cfg.espHealthBar, acc);
                SubCheckbox("Player Names", &cfg.espNames, acc);
                SubCheckbox("Distance", &cfg.espDistance, acc);
                SubCheckbox("Ignore Invisible", &cfg.espIgnoreInvis, acc);
            }
            EndModuleCard();

            if (BeginModuleCard("tracers", "Tracers", "Smooth directional lines to nearby players",
                                cfg.tracersEnabled, cfg.tracersBind, acc)) {
                FancySliderFloat("Max Distance", &cfg.tracersMaxDist, 16.0f, 128.0f, "%.0fm", acc);
            }
            EndModuleCard();

            if (BeginModuleCard("target_hud", "Target HUD", "Floating opponent health & crit card",
                                cfg.targetHudEnabled, cfg.targetHudBind, acc, false)) {
            }
            EndModuleCard();

            if (BeginModuleCard("crosshair_ring", "Crit & Cooldown Ring", "Crosshair attack cooldown & crit readiness arc",
                                cfg.crosshairRingEnabled, cfg.crosshairRingBind, acc, false)) {
            }
            EndModuleCard();

            if (BeginModuleCard("fullbright", "Fullbright", "Boosts lightmap gamma in dark caves & night",
                                cfg.fullbrightEnabled, cfg.fullbrightBind, acc, false)) {
            }
            EndModuleCard();
        } else if (activeTab_ == TabSection::PveFarm) {
            if (BeginModuleCard("mob_trigger", "Mob Trigger", "Auto-attacks aimed mobs at optimal cooldown",
                                cfg.mobTriggerEnabled, cfg.mobTriggerBind, acc)) {
                SubCheckbox("Hostile Mobs Only", &cfg.mobHostileOnly, acc);
                SubCheckbox("Smart Jump Crits", &cfg.mobUseCrits, acc);
                FancySliderFloat("Attack Cooldown", &cfg.mobCooldown, 0.75f, 1.00f, "%.2f", acc);
                FancySliderFloat("Max Range", &cfg.mobRange, 2.4f, 4.0f, "%.1fm", acc);
            }
            EndModuleCard();

            if (BeginModuleCard("autofish", "Auto Fish", "Detects bobber bite, reels in & recasts naturally",
                                cfg.autoFishEnabled, cfg.autoFishBind, acc)) {
                FancySliderInt("Recast Delay", &cfg.autoFishRecastDelayMs, 150, 900, "%d ms", acc);
                FancySliderInt("Random Jitter", &cfg.autoFishJitterMs, 0, 350, "+/- %d ms", acc);
            }
            EndModuleCard();

            if (BeginModuleCard("autosprint", "Auto Sprint", "Automatically holds sprint (pauses cleanly for crits)",
                                cfg.autoSprintEnabled, cfg.autoSprintBind, acc, false)) {
            }
            EndModuleCard();

            if (BeginModuleCard("autoeat", "Auto Eat", "Switches to food in hotbar when hunger or HP drops",
                                cfg.autoEatEnabled, cfg.autoEatBind, acc)) {
                FancySliderInt("Hunger Threshold", &cfg.autoEatHunger, 4, 19, "%d / 20", acc);
                FancySliderFloat("Health Threshold", &cfg.autoEatHealth, 4.0f, 18.0f, "%.1f HP", acc);
            }
            EndModuleCard();
        } else { // PvP
            if (BeginModuleCard("triggerbot", "TriggerBot", "Fast crosshair attacks on players with 1.5x critical hits",
                                cfg.triggerBotEnabled, cfg.triggerBotBind, acc)) {
                if (fBody_) ImGui::PushFont(fBody_);
                ImGui::TextColored(ImVec4(0.68f, 0.72f, 0.85f, 1.0f), "Crit Mode");
                if (fBody_) ImGui::PopFont();
                const char* critLabels[3] = { "Smart", "Only Crits", "AutoCrit" };
                SegmentedControl("##critmode", &cfg.triggerCritMode, critLabels, 3, acc);

                FancySliderFloat("Attack Cooldown", &cfg.triggerCooldown, 0.85f, 1.00f, "%.2f", acc);
                FancySliderInt("Min Reaction Jitter", &cfg.triggerMinDelayMs, 0, 80, "%d ms", acc);
                FancySliderInt("Max Reaction Jitter", &cfg.triggerMaxDelayMs, 0, 140, "%d ms", acc);
                if (cfg.triggerMaxDelayMs < cfg.triggerMinDelayMs) {
                    cfg.triggerMaxDelayMs = cfg.triggerMinDelayMs;
                }
                FancySliderFloat("Reach Limit", &cfg.triggerRange, 2.4f, 3.3f, "%.2fm", acc);

                SubCheckbox("Sprint Reset for Crits (Server Sync)", &cfg.triggerSprintReset, acc);
                SubCheckbox("Weapon Only (Sword / Axe / Mace)", &cfg.triggerWeaponOnly, acc);
                SubCheckbox("Pause While Eating / Blocking", &cfg.triggerPauseWhileUsing, acc);
                SubCheckbox("Ignore Invisible Players", &cfg.triggerIgnoreInvis, acc);
                SubCheckbox("Ignore Same Team", &cfg.triggerIgnoreTeams, acc);
                SubCheckbox("Ignore Creative Mode", &cfg.triggerIgnoreCreative, acc);
            }
            EndModuleCard();

            if (BeginModuleCard("aimassist", "Aim Assist", "Subtle human-like angular smoothing toward target",
                                cfg.aimAssistEnabled, cfg.aimAssistBind, acc)) {
                FancySliderFloat("FOV Cone", &cfg.aimFov, 4.0f, 45.0f, "%.1f deg", acc);
                FancySliderFloat("Range", &cfg.aimRange, 2.5f, 5.0f, "%.1fm", acc);
                FancySliderFloat("Horizontal Speed", &cfg.aimSpeedH, 1.0f, 15.0f, "%.1f", acc);
                FancySliderFloat("Vertical Speed", &cfg.aimSpeedV, 0.0f, 10.0f, "%.1f", acc);
                SubCheckbox("Only While Holding Attack", &cfg.aimOnlyOnClick, acc);
                SubCheckbox("Weapon Only", &cfg.aimWeaponOnly, acc);
            }
            EndModuleCard();

            if (BeginModuleCard("sprintreset", "Sprint Reset (W-Tap)", "Briefly resets sprint after hit for combo knockback",
                                cfg.sprintResetEnabled, cfg.sprintResetBind, acc)) {
                FancySliderInt("Reset Pulse", &cfg.sprintResetPulseMs, 20, 100, "%d ms", acc);
            }
            EndModuleCard();

            if (BeginModuleCard("jumpreset", "Jump Reset", "Legit jump on hurt tick to reduce knockback",
                                cfg.jumpResetEnabled, cfg.jumpResetBind, acc, false)) {
            }
            EndModuleCard();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(3);
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);

    if (anyChanged_) {
        requestSave = true;
    }
}

} // namespace meow
