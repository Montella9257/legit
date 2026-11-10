#include "../src/dll/config.hpp"
#include "../src/dll/modules.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

// Test instrumentation hooks on JvmBridge (compiled without jvm_bridge.cpp)
namespace {
struct BridgeLog {
    int attacks = 0;
    int sprintResets = 0;
    int jumps = 0;
    int itemUses = 0;
    int selectedSlot = -1;
    bool sprintKey = false;
    bool useKey = false;
    bool gammaOverride = false;
    float lastYaw = 0.0f;
    float lastPitch = 0.0f;
    int rotCalls = 0;
    int mockFoodSlot = 3;
};
BridgeLog g_log;
} // namespace

namespace meow {

bool JvmBridge::Init() { ready_ = true; return true; }
void JvmBridge::Shutdown() { ready_ = false; }
void JvmBridge::WithFrame(const std::function<void(const FrameSnapshot&, const FrameHandles&)>&) {}
void JvmBridge::AttackEntity(jobject, bool resetSprintForCrit) {
    ++g_log.attacks;
    if (resetSprintForCrit) ++g_log.sprintResets;
}
void JvmBridge::JumpPlayer() { ++g_log.jumps; }
void JvmBridge::SetPlayerRotation(float yaw, float pitch) {
    g_log.lastYaw = yaw;
    g_log.lastPitch = pitch;
    ++g_log.rotCalls;
}
void JvmBridge::SetPlayerSprinting(bool, bool) {}
void JvmBridge::SetSprintKeyPressed(bool pressed) { g_log.sprintKey = pressed; }
void JvmBridge::SetUseKeyPressed(bool pressed) { g_log.useKey = pressed; }
void JvmBridge::DoItemUse() { ++g_log.itemUses; }
int  JvmBridge::FindFoodHotbarSlot() { return g_log.mockFoodSlot; }
void JvmBridge::SelectHotbarSlot(int slot) { g_log.selectedSlot = slot; }
void JvmBridge::SetGammaOverride(bool enable) { g_log.gammaOverride = enable; }
void JvmBridge::UnlockCursor() {}
void JvmBridge::LockCursor() {}

} // namespace meow

using namespace meow;

static FrameSnapshot MakeBaseSnap() {
    FrameSnapshot s;
    s.ready = true;
    s.inWorld = true;
    s.inScreen = false;
    s.screenW = 1920;
    s.screenH = 1080;
    s.fovDeg = 70.0f;
    s.onGround = true;
    s.attackCooldown = 1.0f;
    s.health = 20.0f;
    s.maxHealth = 20.0f;
    s.foodLevel = 20;
    s.selectedSlot = 0;

    s.crosshair.valid = true;
    s.crosshair.isPlayer = true;
    s.crosshair.alive = true;
    s.crosshair.distance = 2.6f;
    s.crosshair.health = 20.0f;
    s.crosshair.maxHealth = 20.0f;
    s.crosshair.name = "Steve";
    return s;
}

int main() {
    // 1. Config JSON roundtrip & clamping
    {
        ClientConfig cfg;
        cfg.triggerBotEnabled = true;
        cfg.triggerCritMode = static_cast<int>(CritMode::AutoCrit);
        cfg.triggerCooldown = 0.94f;
        cfg.triggerMinDelayMs = 15;
        cfg.triggerMaxDelayMs = 35;
        cfg.espEnabled = true;
        cfg.autoFishEnabled = true;
        cfg.accentPreset = 2;

        auto tmpPath = std::filesystem::temp_directory_path() / "meow_test_cfg.json";
        assert(SaveConfigToFile(cfg, tmpPath));

        ClientConfig loaded;
        assert(LoadConfigFromFile(loaded, tmpPath));
        assert(loaded.triggerBotEnabled == true);
        assert(loaded.triggerCritMode == 2);
        assert(std::fabs(loaded.triggerCooldown - 0.94f) < 1e-4f);
        assert(loaded.triggerMinDelayMs == 15);
        assert(loaded.triggerMaxDelayMs == 35);
        assert(loaded.espEnabled == true);
        assert(loaded.autoFishEnabled == true);
        assert(loaded.accentPreset == 2);

        // Out-of-bounds JSON clamping
        nlohmann::json bad = loaded.ToJson();
        bad["triggerCooldown"] = 5.0f;
        bad["accentPreset"] = 99;
        bad["triggerRange"] = 50.0f;
        ClientConfig clamped;
        clamped.FromJson(bad);
        assert(clamped.triggerCooldown <= 1.0f);
        assert(clamped.accentPreset == 3);
        assert(clamped.triggerRange <= 3.5f);
        std::filesystem::remove(tmpPath);
    }

    // 2. Angle math & WorldToScreen projection
    {
        assert(std::fabs(WrapDegrees(190.0f) - (-170.0f)) < 1e-3f);
        assert(std::fabs(WrapDegrees(-190.0f) - 170.0f) < 1e-3f);

        // Looking straight South (+Z): yaw=0, pitch=0
        float yaw = 0, pitch = 0;
        ComputeLookAngles({0, 64, 0}, {0, 64, 10}, yaw, pitch);
        assert(std::fabs(WrapDegrees(yaw - 0.0f)) < 1e-3f);
        assert(std::fabs(pitch) < 1e-3f);

        // WorldToScreen centered straight ahead
        ScreenPos sp;
        assert(WorldToScreen({0, 64, 10}, {0, 64, 0}, 0.0f, 0.0f, 70.0f, 1920, 1080, sp));
        assert(sp.visible);
        assert(std::fabs(sp.x - 960.0f) < 1.0f);
        assert(std::fabs(sp.y - 540.0f) < 1.0f);

        // Object behind camera must not be visible
        ScreenPos behind;
        assert(!WorldToScreen({0, 64, -10}, {0, 64, 0}, 0.0f, 0.0f, 70.0f, 1920, 1080, behind));
        assert(!behind.visible);

        // Point to the right of South (+Z) is West (-X) -> screen X > 960
        ScreenPos rightSide;
        assert(WorldToScreen({-2, 64, 10}, {0, 64, 0}, 0.0f, 0.0f, 70.0f, 1920, 1080, rightSide));
        assert(rightSide.x > 960.0f);
    }

    // 3. TriggerBot evaluation modes (Smart, OnlyCrits, AutoCrit)
    {
        ClientConfig cfg;
        cfg.triggerBotEnabled = true;
        cfg.triggerCooldown = 0.92f;
        cfg.triggerRange = 3.0f;

        // Menu open blocks attack
        FrameSnapshot snap = MakeBaseSnap();
        assert(EvaluateTriggerAttack(snap, cfg, true) == TriggerEvalResult::NoTarget);

        // Cooldown too low -> WaitCooldown
        snap.attackCooldown = 0.70f;
        assert(EvaluateTriggerAttack(snap, cfg, false) == TriggerEvalResult::WaitCooldown);

        // Smart mode on ground -> ReadyToAttack
        snap.attackCooldown = 0.94f;
        cfg.triggerCritMode = static_cast<int>(CritMode::Smart);
        assert(EvaluateTriggerAttack(snap, cfg, false) == TriggerEvalResult::ReadyToAttack);

        // Smart mode rising in air (fallDistance == 0) -> WaitCritFall!
        snap.onGround = false;
        snap.fallDistance = 0.0f;
        snap.canCritNow = false;
        assert(EvaluateTriggerAttack(snap, cfg, false) == TriggerEvalResult::WaitCritFall);

        // Smart mode falling -> ReadyToAttack (crit!)
        snap.fallDistance = 0.25f;
        snap.canCritNow = true;
        assert(EvaluateTriggerAttack(snap, cfg, false) == TriggerEvalResult::ReadyToAttack);

        // OnlyCrits on ground -> WaitCritFall!
        cfg.triggerCritMode = static_cast<int>(CritMode::OnlyCrits);
        snap.onGround = true;
        snap.fallDistance = 0.0f;
        snap.canCritNow = false;
        assert(EvaluateTriggerAttack(snap, cfg, false) == TriggerEvalResult::WaitCritFall);

        // AutoCrit on ground -> NeedAutoJump!
        cfg.triggerCritMode = static_cast<int>(CritMode::AutoCrit);
        snap.attackCooldown = 0.85f;
        assert(EvaluateTriggerAttack(snap, cfg, false) == TriggerEvalResult::NeedAutoJump);

        // Filters
        snap.attackCooldown = 1.0f;
        snap.canCritNow = true;
        snap.onGround = false;
        snap.crosshair.invisible = true;
        assert(EvaluateTriggerAttack(snap, cfg, false) == TriggerEvalResult::NoTarget);
        snap.crosshair.invisible = false;
        snap.crosshair.sameTeam = true;
        assert(EvaluateTriggerAttack(snap, cfg, false) == TriggerEvalResult::NoTarget);
    }

    // 4. ModuleManager simulation (TriggerBot crit sprint reset + AutoFish + JumpReset + AutoEat)
    {
        g_log = {};
        ClientConfig cfg;
        cfg.triggerBotEnabled = true;
        cfg.triggerCritMode = static_cast<int>(CritMode::OnlyCrits);
        cfg.triggerMinDelayMs = 0;
        cfg.triggerMaxDelayMs = 0;
        cfg.triggerSprintReset = true;
        cfg.autoFishEnabled = true;
        cfg.jumpResetEnabled = true;
        cfg.autoEatEnabled = true;
        cfg.autoEatHunger = 14;
        cfg.fullbrightEnabled = true;

        ModuleManager mm;
        JvmBridge bridge;
        FrameHandles handles;
        int dummyTarget = 42;
        handles.targetEntity = reinterpret_cast<jobject>(&dummyTarget);

        FrameSnapshot snap = MakeBaseSnap();
        snap.onGround = false;
        snap.fallDistance = 0.3f;
        snap.canCritNow = true;
        snap.sprinting = true;
        snap.hasFishHook = true;
        snap.fishCaught = true;
        snap.foodLevel = 12;

        mm.Tick(bridge, cfg, snap, handles, false, 0.016f);
        assert(g_log.attacks == 1);
        assert(g_log.sprintResets == 1);
        assert(g_log.itemUses == 1);
        assert(g_log.selectedSlot == 3);
        assert(g_log.useKey == true);
        assert(g_log.gammaOverride == true);

        // JumpReset triggers on hurtTime >= 8 while onGround
        snap.onGround = true;
        snap.hurtTime = 9;
        mm.Tick(bridge, cfg, snap, handles, false, 0.016f);
        assert(g_log.jumps == 1);
    }

    std::printf("ALL MEOW.DLL CORE UNIT TESTS PASSED!\n");
    return 0;
}
