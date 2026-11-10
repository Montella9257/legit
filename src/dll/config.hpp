#pragma once

#include <string>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include "../../third_party/json/json.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace meow {

enum class TabSection {
    Visual = 0,
    PveFarm = 1,
    PvP = 2
};

enum class CritMode {
    Smart = 0,     // Fast hits on ground; waits for fallDistance > 0 while jumping
    OnlyCrits = 1, // Strictly attacks only when critical hit conditions are met
    AutoCrit = 2   // Automatically jumps and strikes on fall apex for chained crits
};

enum class AccentPreset {
    SakuraViolet = 0, // #9D7BFF -> #F472B6
    CyberCyan    = 1, // #38BDF8 -> #818CF8
    CrimsonRose  = 2, // #FB7185 -> #F43F5E
    MintPulse    = 3  // #34D399 -> #2DD4BF
};

struct ClientConfig {
    // UI / General
    int accentPreset = static_cast<int>(AccentPreset::SakuraViolet);
    bool showWatermark = true;
    bool showArrayList = true;

    // Visual
    bool espEnabled = false;
    int  espBind = 0;
    bool espCorners = true;
    bool espFill = true;
    bool espHealthBar = true;
    bool espNames = true;
    bool espDistance = true;
    bool espIgnoreInvis = true;
    float espColor[4] = { 0.68f, 0.45f, 1.00f, 1.00f };

    bool tracersEnabled = false;
    int  tracersBind = 0;
    float tracersMaxDist = 64.0f;

    bool targetHudEnabled = true;
    int  targetHudBind = 0;

    bool crosshairRingEnabled = true;
    int  crosshairRingBind = 0;

    bool fullbrightEnabled = false;
    int  fullbrightBind = 0;

    // PveFarm
    bool mobTriggerEnabled = false;
    int  mobTriggerBind = 0;
    bool mobHostileOnly = true;
    bool mobUseCrits = true;
    float mobCooldown = 0.93f;
    float mobRange = 3.2f;

    bool autoFishEnabled = false;
    int  autoFishBind = 0;
    int  autoFishRecastDelayMs = 340;
    int  autoFishJitterMs = 140;

    bool autoSprintEnabled = false;
    int  autoSprintBind = 0;

    bool autoEatEnabled = false;
    int  autoEatBind = 0;
    int  autoEatHunger = 14;
    float autoEatHealth = 10.0f;

    // PvP
    bool triggerBotEnabled = false;
    int  triggerBotBind = 0;
    int  triggerCritMode = static_cast<int>(CritMode::Smart);
    float triggerCooldown = 0.92f;
    int  triggerMinDelayMs = 12;
    int  triggerMaxDelayMs = 36;
    float triggerRange = 3.0f;
    bool triggerSprintReset = true;
    bool triggerWeaponOnly = false;
    bool triggerPauseWhileUsing = true;
    bool triggerIgnoreInvis = true;
    bool triggerIgnoreTeams = true;
    bool triggerIgnoreCreative = true;

    bool aimAssistEnabled = false;
    int  aimAssistBind = 0;
    float aimFov = 18.0f;
    float aimRange = 4.0f;
    float aimSpeedH = 5.5f;
    float aimSpeedV = 2.5f;
    bool aimOnlyOnClick = true;
    bool aimWeaponOnly = false;

    bool sprintResetEnabled = false;
    int  sprintResetBind = 0;
    int  sprintResetPulseMs = 45;

    bool jumpResetEnabled = false;
    int  jumpResetBind = 0;

    nlohmann::json ToJson() const {
        nlohmann::json j;
        j["accentPreset"] = accentPreset;
        j["showWatermark"] = showWatermark;
        j["showArrayList"] = showArrayList;

        j["espEnabled"] = espEnabled;
        j["espBind"] = espBind;
        j["espCorners"] = espCorners;
        j["espFill"] = espFill;
        j["espHealthBar"] = espHealthBar;
        j["espNames"] = espNames;
        j["espDistance"] = espDistance;
        j["espIgnoreInvis"] = espIgnoreInvis;
        j["espColor"] = { espColor[0], espColor[1], espColor[2], espColor[3] };

        j["tracersEnabled"] = tracersEnabled;
        j["tracersBind"] = tracersBind;
        j["tracersMaxDist"] = tracersMaxDist;

        j["targetHudEnabled"] = targetHudEnabled;
        j["targetHudBind"] = targetHudBind;
        j["crosshairRingEnabled"] = crosshairRingEnabled;
        j["crosshairRingBind"] = crosshairRingBind;
        j["fullbrightEnabled"] = fullbrightEnabled;
        j["fullbrightBind"] = fullbrightBind;

        j["mobTriggerEnabled"] = mobTriggerEnabled;
        j["mobTriggerBind"] = mobTriggerBind;
        j["mobHostileOnly"] = mobHostileOnly;
        j["mobUseCrits"] = mobUseCrits;
        j["mobCooldown"] = mobCooldown;
        j["mobRange"] = mobRange;

        j["autoFishEnabled"] = autoFishEnabled;
        j["autoFishBind"] = autoFishBind;
        j["autoFishRecastDelayMs"] = autoFishRecastDelayMs;
        j["autoFishJitterMs"] = autoFishJitterMs;

        j["autoSprintEnabled"] = autoSprintEnabled;
        j["autoSprintBind"] = autoSprintBind;

        j["autoEatEnabled"] = autoEatEnabled;
        j["autoEatBind"] = autoEatBind;
        j["autoEatHunger"] = autoEatHunger;
        j["autoEatHealth"] = autoEatHealth;

        j["triggerBotEnabled"] = triggerBotEnabled;
        j["triggerBotBind"] = triggerBotBind;
        j["triggerCritMode"] = triggerCritMode;
        j["triggerCooldown"] = triggerCooldown;
        j["triggerMinDelayMs"] = triggerMinDelayMs;
        j["triggerMaxDelayMs"] = triggerMaxDelayMs;
        j["triggerRange"] = triggerRange;
        j["triggerSprintReset"] = triggerSprintReset;
        j["triggerWeaponOnly"] = triggerWeaponOnly;
        j["triggerPauseWhileUsing"] = triggerPauseWhileUsing;
        j["triggerIgnoreInvis"] = triggerIgnoreInvis;
        j["triggerIgnoreTeams"] = triggerIgnoreTeams;
        j["triggerIgnoreCreative"] = triggerIgnoreCreative;

        j["aimAssistEnabled"] = aimAssistEnabled;
        j["aimAssistBind"] = aimAssistBind;
        j["aimFov"] = aimFov;
        j["aimRange"] = aimRange;
        j["aimSpeedH"] = aimSpeedH;
        j["aimSpeedV"] = aimSpeedV;
        j["aimOnlyOnClick"] = aimOnlyOnClick;
        j["aimWeaponOnly"] = aimWeaponOnly;

        j["sprintResetEnabled"] = sprintResetEnabled;
        j["sprintResetBind"] = sprintResetBind;
        j["sprintResetPulseMs"] = sprintResetPulseMs;

        j["jumpResetEnabled"] = jumpResetEnabled;
        j["jumpResetBind"] = jumpResetBind;
        return j;
    }

    void FromJson(const nlohmann::json& j) {
        auto getb = [&](const char* k, bool& v) { if (j.contains(k) && j[k].is_boolean()) v = j[k].get<bool>(); };
        auto geti = [&](const char* k, int& v) { if (j.contains(k) && j[k].is_number_integer()) v = j[k].get<int>(); };
        auto getf = [&](const char* k, float& v) { if (j.contains(k) && j[k].is_number()) v = j[k].get<float>(); };

        geti("accentPreset", accentPreset);
        accentPreset = std::clamp(accentPreset, 0, 3);
        getb("showWatermark", showWatermark);
        getb("showArrayList", showArrayList);

        getb("espEnabled", espEnabled);
        geti("espBind", espBind);
        getb("espCorners", espCorners);
        getb("espFill", espFill);
        getb("espHealthBar", espHealthBar);
        getb("espNames", espNames);
        getb("espDistance", espDistance);
        getb("espIgnoreInvis", espIgnoreInvis);
        if (j.contains("espColor") && j["espColor"].is_array() && j["espColor"].size() == 4) {
            for (int i = 0; i < 4; ++i) {
                if (j["espColor"][i].is_number()) espColor[i] = std::clamp(j["espColor"][i].get<float>(), 0.0f, 1.0f);
            }
        }

        getb("tracersEnabled", tracersEnabled);
        geti("tracersBind", tracersBind);
        getf("tracersMaxDist", tracersMaxDist);
        tracersMaxDist = std::clamp(tracersMaxDist, 8.0f, 128.0f);

        getb("targetHudEnabled", targetHudEnabled);
        geti("targetHudBind", targetHudBind);
        getb("crosshairRingEnabled", crosshairRingEnabled);
        geti("crosshairRingBind", crosshairRingBind);
        getb("fullbrightEnabled", fullbrightEnabled);
        geti("fullbrightBind", fullbrightBind);

        getb("mobTriggerEnabled", mobTriggerEnabled);
        geti("mobTriggerBind", mobTriggerBind);
        getb("mobHostileOnly", mobHostileOnly);
        getb("mobUseCrits", mobUseCrits);
        getf("mobCooldown", mobCooldown);
        mobCooldown = std::clamp(mobCooldown, 0.70f, 1.00f);
        getf("mobRange", mobRange);
        mobRange = std::clamp(mobRange, 2.0f, 4.5f);

        getb("autoFishEnabled", autoFishEnabled);
        geti("autoFishBind", autoFishBind);
        geti("autoFishRecastDelayMs", autoFishRecastDelayMs);
        autoFishRecastDelayMs = std::clamp(autoFishRecastDelayMs, 100, 1500);
        geti("autoFishJitterMs", autoFishJitterMs);
        autoFishJitterMs = std::clamp(autoFishJitterMs, 0, 500);

        getb("autoSprintEnabled", autoSprintEnabled);
        geti("autoSprintBind", autoSprintBind);

        getb("autoEatEnabled", autoEatEnabled);
        geti("autoEatBind", autoEatBind);
        geti("autoEatHunger", autoEatHunger);
        autoEatHunger = std::clamp(autoEatHunger, 2, 19);
        getf("autoEatHealth", autoEatHealth);
        autoEatHealth = std::clamp(autoEatHealth, 2.0f, 20.0f);

        getb("triggerBotEnabled", triggerBotEnabled);
        geti("triggerBotBind", triggerBotBind);
        geti("triggerCritMode", triggerCritMode);
        triggerCritMode = std::clamp(triggerCritMode, 0, 2);
        getf("triggerCooldown", triggerCooldown);
        triggerCooldown = std::clamp(triggerCooldown, 0.80f, 1.00f);
        geti("triggerMinDelayMs", triggerMinDelayMs);
        geti("triggerMaxDelayMs", triggerMaxDelayMs);
        triggerMinDelayMs = std::clamp(triggerMinDelayMs, 0, 150);
        triggerMaxDelayMs = std::clamp(triggerMaxDelayMs, triggerMinDelayMs, 200);
        getf("triggerRange", triggerRange);
        triggerRange = std::clamp(triggerRange, 2.0f, 3.5f);
        getb("triggerSprintReset", triggerSprintReset);
        getb("triggerWeaponOnly", triggerWeaponOnly);
        getb("triggerPauseWhileUsing", triggerPauseWhileUsing);
        getb("triggerIgnoreInvis", triggerIgnoreInvis);
        getb("triggerIgnoreTeams", triggerIgnoreTeams);
        getb("triggerIgnoreCreative", triggerIgnoreCreative);

        getb("aimAssistEnabled", aimAssistEnabled);
        geti("aimAssistBind", aimAssistBind);
        getf("aimFov", aimFov);
        aimFov = std::clamp(aimFov, 3.0f, 60.0f);
        getf("aimRange", aimRange);
        aimRange = std::clamp(aimRange, 2.0f, 6.0f);
        getf("aimSpeedH", aimSpeedH);
        aimSpeedH = std::clamp(aimSpeedH, 0.5f, 20.0f);
        getf("aimSpeedV", aimSpeedV);
        aimSpeedV = std::clamp(aimSpeedV, 0.0f, 15.0f);
        getb("aimOnlyOnClick", aimOnlyOnClick);
        getb("aimWeaponOnly", aimWeaponOnly);

        getb("sprintResetEnabled", sprintResetEnabled);
        geti("sprintResetBind", sprintResetBind);
        geti("sprintResetPulseMs", sprintResetPulseMs);
        sprintResetPulseMs = std::clamp(sprintResetPulseMs, 15, 120);

        getb("jumpResetEnabled", jumpResetEnabled);
        geti("jumpResetBind", jumpResetBind);
    }
};

inline std::filesystem::path GetConfigFilePath() {
#ifdef _WIN32
    wchar_t appdata[MAX_PATH]{};
    DWORD len = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return std::filesystem::path(appdata) / L"MeowClient" / L"config.json";
    }
    return std::filesystem::path(L"meow_config.json");
#else
    return std::filesystem::path("meow_config.json");
#endif
}

inline bool SaveConfigToFile(const ClientConfig& cfg, const std::filesystem::path& path = GetConfigFilePath()) {
    try {
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
        std::ofstream ofs(path, std::ios::trunc);
        if (!ofs.is_open()) return false;
        ofs << cfg.ToJson().dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool LoadConfigFromFile(ClientConfig& cfg, const std::filesystem::path& path = GetConfigFilePath()) {
    try {
        if (!std::filesystem::exists(path)) return false;
        std::ifstream ifs(path);
        if (!ifs.is_open()) return false;
        nlohmann::json j;
        ifs >> j;
        cfg.FromJson(j);
        return true;
    } catch (...) {
        return false;
    }
}

inline std::string KeyCodeToString(int vk) {
    if (vk <= 0) return "NONE";
    if (vk >= 'A' && vk <= 'Z') return std::string(1, static_cast<char>(vk));
    if (vk >= '0' && vk <= '9') return std::string(1, static_cast<char>(vk));
    switch (vk) {
        case 0x05: return "MB4";
        case 0x06: return "MB5";
        case 0x09: return "TAB";
        case 0x14: return "CAPS";
        case 0x20: return "SPACE";
        case 0xA0: return "LSHIFT";
        case 0xA1: return "RSHIFT";
        case 0xA2: return "LCTRL";
        case 0xA3: return "RCTRL";
        case 0xA4: return "LALT";
        case 0xA5: return "RALT";
        case 0x70: return "F1";
        case 0x71: return "F2";
        case 0x72: return "F3";
        case 0x73: return "F4";
        case 0x74: return "F5";
        case 0x75: return "F6";
        case 0x76: return "F7";
        case 0x77: return "F8";
        case 0x78: return "F9";
        case 0x79: return "F10";
        case 0x7A: return "F11";
        case 0x7B: return "F12";
        case 0x2D: return "INS";
        case 0x2E: return "DEL";
        case 0x24: return "HOME";
        case 0x23: return "END";
        default: {
            char buf[16]{};
            snprintf(buf, sizeof(buf), "0x%02X", vk);
            return buf;
        }
    }
}

} // namespace meow
