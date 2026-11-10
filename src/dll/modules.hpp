#pragma once

#include "config.hpp"
#include "jvm_bridge.hpp"

#include <chrono>
#include <cstdint>
#include <random>
#include <string>

namespace meow {

struct ScreenPos {
    float x = 0.0f;
    float y = 0.0f;
    bool visible = false;
};

struct TargetHudState {
    bool active = false;
    std::string name;
    float health = 20.0f;
    float maxHealth = 20.0f;
    float animatedHealth = 20.0f;
    float distance = 0.0f;
    int hurtTime = 0;
    bool canCrit = false;
    uint64_t lastSeenMs = 0;
};

enum class TriggerEvalResult {
    NoTarget,
    WaitCooldown,
    WaitCritFall,
    NeedAutoJump,
    ReadyToAttack
};

// Pure testable helpers
float WrapDegrees(float deg);
void ComputeLookAngles(const Vec3& from, const Vec3& to, float& outYaw, float& outPitch);
float AngularDistance(float yawA, float pitchA, float yawB, float pitchB);
bool WorldToScreen(const Vec3& worldPos, const Vec3& camPos, float camYawDeg, float camPitchDeg,
                   float fovDeg, int screenW, int screenH, ScreenPos& out);
TriggerEvalResult EvaluateTriggerAttack(const FrameSnapshot& snap, const ClientConfig& cfg, bool menuOpen);

class ModuleManager {
public:
    ModuleManager();

    void Tick(JvmBridge& bridge, ClientConfig& cfg, const FrameSnapshot& snap,
              const FrameHandles& handles, bool menuOpen, float dtSec);

    const TargetHudState& GetTargetHud() const { return targetHud_; }
    uint64_t LastCritHitMs() const { return lastCritHitMs_; }
    uint64_t LastHitMs() const { return lastHitMs_; }

private:
    uint64_t NowMs() const;
    int RandRange(int lo, int hi);

    void TickTriggerBot(JvmBridge& bridge, const ClientConfig& cfg, const FrameSnapshot& snap,
                        const FrameHandles& handles, bool menuOpen, uint64_t nowMs);
    void TickMobTrigger(JvmBridge& bridge, const ClientConfig& cfg, const FrameSnapshot& snap,
                        const FrameHandles& handles, bool menuOpen, uint64_t nowMs);
    void TickAimAssist(JvmBridge& bridge, const ClientConfig& cfg, const FrameSnapshot& snap,
                       bool menuOpen, float dtSec);
    void TickJumpReset(JvmBridge& bridge, const ClientConfig& cfg, const FrameSnapshot& snap,
                       bool menuOpen);
    void TickAutoFish(JvmBridge& bridge, const ClientConfig& cfg, const FrameSnapshot& snap,
                      bool menuOpen, uint64_t nowMs);
    void TickAutoEat(JvmBridge& bridge, const ClientConfig& cfg, const FrameSnapshot& snap,
                     bool menuOpen);
    void TickAutoSprint(JvmBridge& bridge, const ClientConfig& cfg, const FrameSnapshot& snap,
                        bool menuOpen, uint64_t nowMs);
    void UpdateTargetHud(const ClientConfig& cfg, const FrameSnapshot& snap, uint64_t nowMs, float dtSec);

    std::mt19937 rng_;

    // TriggerBot reaction timing
    bool pendingTrigger_ = false;
    uint64_t triggerReadyAtMs_ = 0;
    uint64_t lastTriggerAttackMs_ = 0;
    uint64_t lastAutoJumpMs_ = 0;

    // MobTrigger timing
    uint64_t lastMobAttackMs_ = 0;

    // Crit / W-Tap sprint pause window
    uint64_t sprintPauseUntilMs_ = 0;
    bool wasSprintPaused_ = false;
    uint64_t lastHitMs_ = 0;
    uint64_t lastCritHitMs_ = 0;

    // JumpReset state
    int prevHurtTime_ = 0;

    // AutoFish state
    bool fishWasCaught_ = false;
    bool fishNeedsRecast_ = false;
    uint64_t fishRecastAtMs_ = 0;

    // AutoEat state
    bool isEating_ = false;
    int eatPrevSlot_ = -1;

    TargetHudState targetHud_{};
};

} // namespace meow
