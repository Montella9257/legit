#include "modules.hpp"

#include <cmath>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

namespace meow {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;
} // namespace

float WrapDegrees(float deg) {
    float r = std::fmod(deg, 360.0f);
    if (r >= 180.0f) r -= 360.0f;
    if (r < -180.0f) r += 360.0f;
    return r;
}

void ComputeLookAngles(const Vec3& from, const Vec3& to, float& outYaw, float& outPitch) {
    double dx = to.x - from.x;
    double dy = to.y - from.y;
    double dz = to.z - from.z;
    double horiz = std::sqrt(dx * dx + dz * dz);
    outYaw = static_cast<float>(std::atan2(dz, dx) * kRadToDeg - 90.0);
    outPitch = static_cast<float>(-std::atan2(dy, horiz) * kRadToDeg);
    outPitch = std::clamp(outPitch, -90.0f, 90.0f);
}

float AngularDistance(float yawA, float pitchA, float yawB, float pitchB) {
    float dy = WrapDegrees(yawB - yawA);
    float dp = pitchB - pitchA;
    return std::sqrt(dy * dy + dp * dp);
}

bool WorldToScreen(const Vec3& worldPos, const Vec3& camPos, float camYawDeg, float camPitchDeg,
                   float fovDeg, int screenW, int screenH, ScreenPos& out) {
    out.visible = false;
    if (screenW <= 0 || screenH <= 0 || fovDeg <= 1.0f) return false;

    double yaw = static_cast<double>(camYawDeg) * kDegToRad;
    double pitch = static_cast<double>(camPitchDeg) * kDegToRad;

    double cosPitch = std::cos(pitch);
    double sinPitch = std::sin(pitch);
    double cosYaw = std::cos(yaw);
    double sinYaw = std::sin(yaw);

    // Minecraft camera basis vectors
    Vec3 forward { -sinYaw * cosPitch, -sinPitch, cosYaw * cosPitch };
    Vec3 right   { -cosYaw, 0.0, -sinYaw };
    // up = right x forward
    Vec3 up {
        right.y * forward.z - right.z * forward.y,
        right.z * forward.x - right.x * forward.z,
        right.x * forward.y - right.y * forward.x
    };

    Vec3 d { worldPos.x - camPos.x, worldPos.y - camPos.y, worldPos.z - camPos.z };
    double vx = d.x * right.x   + d.y * right.y   + d.z * right.z;
    double vy = d.x * up.x      + d.y * up.y      + d.z * up.z;
    double vz = d.x * forward.x + d.y * forward.y + d.z * forward.z;

    if (vz <= 0.05) return false;

    double tanHalfFov = std::tan(static_cast<double>(fovDeg) * kDegToRad * 0.5);
    double aspect = static_cast<double>(screenW) / static_cast<double>(screenH);

    double ndcX = vx / (vz * tanHalfFov * aspect);
    double ndcY = vy / (vz * tanHalfFov);

    out.x = static_cast<float>((ndcX + 1.0) * 0.5 * static_cast<double>(screenW));
    out.y = static_cast<float>((1.0 - ndcY) * 0.5 * static_cast<double>(screenH));
    out.visible = true;
    return true;
}

TriggerEvalResult EvaluateTriggerAttack(const FrameSnapshot& snap, const ClientConfig& cfg, bool menuOpen) {
    if (!cfg.triggerBotEnabled || !snap.ready || !snap.inWorld || snap.inScreen || menuOpen) {
        return TriggerEvalResult::NoTarget;
    }

    const auto& ct = snap.crosshair;
    if (!ct.valid || !ct.isPlayer || !ct.alive || ct.spectator) {
        return TriggerEvalResult::NoTarget;
    }
    if (cfg.triggerIgnoreInvis && ct.invisible) return TriggerEvalResult::NoTarget;
    if (cfg.triggerIgnoreTeams && ct.sameTeam) return TriggerEvalResult::NoTarget;
    if (cfg.triggerIgnoreCreative && ct.creative) return TriggerEvalResult::NoTarget;
    if (ct.distance > cfg.triggerRange) return TriggerEvalResult::NoTarget;
    if (cfg.triggerWeaponOnly && !snap.holdingWeapon) return TriggerEvalResult::NoTarget;
    if (cfg.triggerPauseWhileUsing && (snap.usingItem || snap.blocking)) return TriggerEvalResult::NoTarget;

    bool critImpossible = snap.inWater || snap.climbing || snap.hasVehicle || snap.blind;
    bool canCritNow = snap.canCritNow;
    CritMode mode = static_cast<CritMode>(cfg.triggerCritMode);

    if (mode == CritMode::AutoCrit && snap.onGround && !critImpossible) {
        // Jump slightly before cooldown finishes so fall peak aligns with full cooldown!
        if (snap.attackCooldown >= (cfg.triggerCooldown - 0.18f)) {
            return TriggerEvalResult::NeedAutoJump;
        }
        return TriggerEvalResult::WaitCooldown;
    }

    if (snap.attackCooldown < cfg.triggerCooldown) {
        return TriggerEvalResult::WaitCooldown;
    }

    if (mode == CritMode::OnlyCrits) {
        if (canCritNow) return TriggerEvalResult::ReadyToAttack;
        if (critImpossible) return TriggerEvalResult::NoTarget;
        return TriggerEvalResult::WaitCritFall;
    }

    if (mode == CritMode::AutoCrit) {
        if (canCritNow) return TriggerEvalResult::ReadyToAttack;
        if (critImpossible) return TriggerEvalResult::ReadyToAttack;
        return TriggerEvalResult::WaitCritFall;
    }

    // CritMode::Smart:
    // On ground -> strike immediately for fast DPS.
    // Airborne -> wait until fallDistance > 0 so jump attacks ALWAYS land as 1.5x crits!
    if (!snap.onGround && !critImpossible && !canCritNow) {
        return TriggerEvalResult::WaitCritFall;
    }

    return TriggerEvalResult::ReadyToAttack;
}

ModuleManager::ModuleManager()
    : rng_(static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count())) {}

uint64_t ModuleManager::NowMs() const {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

int ModuleManager::RandRange(int lo, int hi) {
    if (hi <= lo) return lo;
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng_);
}

void ModuleManager::Tick(JvmBridge& bridge, ClientConfig& cfg, const FrameSnapshot& snap,
                         const FrameHandles& handles, bool menuOpen, float dtSec) {
    uint64_t now = NowMs();

    if (snap.ready && snap.inWorld) {
        bridge.SetGammaOverride(cfg.fullbrightEnabled);
    }

    TickTriggerBot(bridge, cfg, snap, handles, menuOpen, now);
    TickMobTrigger(bridge, cfg, snap, handles, menuOpen, now);
    TickAimAssist(bridge, cfg, snap, menuOpen, dtSec);
    TickJumpReset(bridge, cfg, snap, menuOpen);
    TickAutoFish(bridge, cfg, snap, menuOpen, now);
    TickAutoEat(bridge, cfg, snap, menuOpen);
    TickAutoSprint(bridge, cfg, snap, menuOpen, now);
    UpdateTargetHud(cfg, snap, now, dtSec);
}

void ModuleManager::TickTriggerBot(JvmBridge& bridge, const ClientConfig& cfg, const FrameSnapshot& snap,
                                   const FrameHandles& handles, bool menuOpen, uint64_t nowMs) {
    TriggerEvalResult eval = EvaluateTriggerAttack(snap, cfg, menuOpen);

    if (eval == TriggerEvalResult::NeedAutoJump) {
        pendingTrigger_ = false;
        if (nowMs - lastAutoJumpMs_ >= 280) {
            bridge.JumpPlayer();
            lastAutoJumpMs_ = nowMs;
        }
        return;
    }

    if (eval != TriggerEvalResult::ReadyToAttack || !handles.targetEntity) {
        pendingTrigger_ = false;
        return;
    }

    // Prevent double-attacking within the same MC tick (~45ms) before cooldown resets
    if (nowMs - lastTriggerAttackMs_ < 65) {
        return;
    }

    if (!pendingTrigger_) {
        int delay = RandRange(cfg.triggerMinDelayMs, cfg.triggerMaxDelayMs);
        if (delay <= 0) {
            bool doSprintReset = (snap.canCritNow && snap.sprinting && cfg.triggerSprintReset) || cfg.sprintResetEnabled;
            bridge.AttackEntity(handles.targetEntity, doSprintReset);
            lastTriggerAttackMs_ = nowMs;
            lastHitMs_ = nowMs;
            if (snap.canCritNow) lastCritHitMs_ = nowMs;
            if (cfg.sprintResetEnabled) {
                sprintPauseUntilMs_ = nowMs + static_cast<uint64_t>(cfg.sprintResetPulseMs);
            } else if (doSprintReset) {
                sprintPauseUntilMs_ = nowMs + 45;
            }
            return;
        }
        pendingTrigger_ = true;
        triggerReadyAtMs_ = nowMs + static_cast<uint64_t>(delay);
        return;
    }

    if (nowMs >= triggerReadyAtMs_) {
        pendingTrigger_ = false;
        bool doSprintReset = (snap.canCritNow && snap.sprinting && cfg.triggerSprintReset) || cfg.sprintResetEnabled;
        bridge.AttackEntity(handles.targetEntity, doSprintReset);
        lastTriggerAttackMs_ = nowMs;
        lastHitMs_ = nowMs;
        if (snap.canCritNow) lastCritHitMs_ = nowMs;
        if (cfg.sprintResetEnabled) {
            sprintPauseUntilMs_ = nowMs + static_cast<uint64_t>(cfg.sprintResetPulseMs);
        } else if (doSprintReset) {
            sprintPauseUntilMs_ = nowMs + 45;
        }
    }
}

void ModuleManager::TickMobTrigger(JvmBridge& bridge, const ClientConfig& cfg, const FrameSnapshot& snap,
                                  const FrameHandles& handles, bool menuOpen, uint64_t nowMs) {
    if (!cfg.mobTriggerEnabled || !snap.ready || !snap.inWorld || snap.inScreen || menuOpen) return;
    if (!handles.targetEntity) return;

    const auto& ct = snap.crosshair;
    if (!ct.valid || !ct.isMob || !ct.alive || ct.spectator) return;
    if (cfg.mobHostileOnly && !ct.isHostile) return;
    if (ct.distance > cfg.mobRange) return;
    if (snap.usingItem || snap.blocking) return;
    if (snap.attackCooldown < cfg.mobCooldown) return;
    if (nowMs - lastMobAttackMs_ < 70) return;

    bool critImpossible = snap.inWater || snap.climbing || snap.hasVehicle || snap.blind;
    if (cfg.mobUseCrits && !snap.onGround && !critImpossible && !snap.canCritNow) {
        return;
    }

    bool resetSprint = cfg.mobUseCrits && snap.canCritNow && snap.sprinting;
    bridge.AttackEntity(handles.targetEntity, resetSprint);
    lastMobAttackMs_ = nowMs;
    lastHitMs_ = nowMs;
    if (snap.canCritNow) lastCritHitMs_ = nowMs;
}

void ModuleManager::TickAimAssist(JvmBridge& bridge, const ClientConfig& cfg, const FrameSnapshot& snap,
                                  bool menuOpen, float dtSec) {
    if (!cfg.aimAssistEnabled || !snap.ready || !snap.inWorld || snap.inScreen || menuOpen) return;
    if (cfg.aimWeaponOnly && !snap.holdingWeapon) return;
#ifdef _WIN32
    if (cfg.aimOnlyOnClick && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) return;
#endif

    float bestAng = cfg.aimFov;
    float bestTargetYaw = snap.playerYaw;
    float bestTargetPitch = snap.playerPitch;
    bool found = false;

    for (const auto& p : snap.players) {
        if (!p.alive || p.spectator) continue;
        if (cfg.triggerIgnoreInvis && p.invisible) continue;
        if (cfg.triggerIgnoreTeams && p.sameTeam) continue;
        if (cfg.triggerIgnoreCreative && p.creative) continue;
        if (p.distance > cfg.aimRange) continue;

        Vec3 aimPoint {
            (p.box.minX + p.box.maxX) * 0.5,
            p.box.minY + (p.box.maxY - p.box.minY) * 0.72,
            (p.box.minZ + p.box.maxZ) * 0.5
        };

        float ty = 0.0f, tp = 0.0f;
        ComputeLookAngles(snap.playerEyePos, aimPoint, ty, tp);
        float ang = AngularDistance(snap.playerYaw, snap.playerPitch, ty, tp);
        if (ang < bestAng) {
            bestAng = ang;
            bestTargetYaw = ty;
            bestTargetPitch = tp;
            found = true;
        }
    }

    if (!found) return;

    float dyaw = WrapDegrees(bestTargetYaw - snap.playerYaw);
    float dpitch = bestTargetPitch - snap.playerPitch;
    float stepH = (std::min)(1.0f, cfg.aimSpeedH * dtSec);
    float stepV = (std::min)(1.0f, cfg.aimSpeedV * dtSec);

    float newYaw = snap.playerYaw + dyaw * stepH;
    float newPitch = std::clamp(snap.playerPitch + dpitch * stepV, -90.0f, 90.0f);
    bridge.SetPlayerRotation(newYaw, newPitch);
}

void ModuleManager::TickJumpReset(JvmBridge& bridge, const ClientConfig& cfg, const FrameSnapshot& snap,
                                  bool menuOpen) {
    if (!cfg.jumpResetEnabled || !snap.ready || !snap.inWorld || snap.inScreen || menuOpen) {
        prevHurtTime_ = snap.hurtTime;
        return;
    }
    if (snap.hurtTime >= 8 && prevHurtTime_ < 8 &&
        snap.onGround && !snap.inWater && !snap.climbing) {
        bridge.JumpPlayer();
    }
    prevHurtTime_ = snap.hurtTime;
}

void ModuleManager::TickAutoFish(JvmBridge& bridge, const ClientConfig& cfg, const FrameSnapshot& snap,
                                 bool menuOpen, uint64_t nowMs) {
    if (!cfg.autoFishEnabled || !snap.ready || !snap.inWorld || snap.inScreen || menuOpen) {
        fishWasCaught_ = false;
        fishNeedsRecast_ = false;
        return;
    }

    if (snap.hasFishHook && snap.fishCaught && !fishWasCaught_) {
        // Bite detected! Reel in
        bridge.DoItemUse();
        fishNeedsRecast_ = true;
        int delay = cfg.autoFishRecastDelayMs + RandRange(0, cfg.autoFishJitterMs);
        fishRecastAtMs_ = nowMs + static_cast<uint64_t>(delay);
    }
    fishWasCaught_ = snap.fishCaught;

    if (fishNeedsRecast_ && !snap.hasFishHook && nowMs >= fishRecastAtMs_) {
        bridge.DoItemUse();
        fishNeedsRecast_ = false;
    }
}

void ModuleManager::TickAutoEat(JvmBridge& bridge, const ClientConfig& cfg, const FrameSnapshot& snap,
                                bool menuOpen) {
    if (!cfg.autoEatEnabled || !snap.ready || !snap.inWorld || snap.inScreen || menuOpen) {
        if (isEating_) {
            bridge.SetUseKeyPressed(false);
            if (eatPrevSlot_ >= 0) bridge.SelectHotbarSlot(eatPrevSlot_);
            isEating_ = false;
            eatPrevSlot_ = -1;
        }
        return;
    }

    bool needEat = (snap.foodLevel <= cfg.autoEatHunger) || (snap.health <= cfg.autoEatHealth && snap.foodLevel < 20);
    if (!isEating_) {
        if (needEat) {
            int slot = bridge.FindFoodHotbarSlot();
            if (slot >= 0) {
                eatPrevSlot_ = snap.selectedSlot;
                bridge.SelectHotbarSlot(slot);
                bridge.SetUseKeyPressed(true);
                isEating_ = true;
            }
        }
    } else {
        if (!needEat) {
            bridge.SetUseKeyPressed(false);
            if (eatPrevSlot_ >= 0) bridge.SelectHotbarSlot(eatPrevSlot_);
            isEating_ = false;
            eatPrevSlot_ = -1;
        } else {
            bridge.SetUseKeyPressed(true);
        }
    }
}

void ModuleManager::TickAutoSprint(JvmBridge& bridge, const ClientConfig& cfg, const FrameSnapshot& snap,
                                   bool menuOpen, uint64_t nowMs) {
    if (!snap.ready || !snap.inWorld || snap.inScreen || menuOpen) return;

    if (nowMs < sprintPauseUntilMs_) {
        bridge.SetSprintKeyPressed(false);
        wasSprintPaused_ = true;
        return;
    }

    if (cfg.autoSprintEnabled) {
        bridge.SetSprintKeyPressed(true);
        wasSprintPaused_ = false;
    } else if (wasSprintPaused_) {
        bridge.SetSprintKeyPressed(false);
        wasSprintPaused_ = false;
    }
}

void ModuleManager::UpdateTargetHud(const ClientConfig& cfg, const FrameSnapshot& snap, uint64_t nowMs, float dtSec) {
    if (!cfg.targetHudEnabled || !snap.ready || !snap.inWorld) {
        targetHud_.active = false;
        return;
    }

    if (snap.crosshair.valid && snap.crosshair.isPlayer && snap.crosshair.alive) {
        targetHud_.active = true;
        targetHud_.name = snap.crosshair.name;
        targetHud_.health = snap.crosshair.health;
        targetHud_.maxHealth = (std::max)(1.0f, snap.crosshair.maxHealth);
        targetHud_.distance = snap.crosshair.distance;
        targetHud_.hurtTime = snap.crosshair.hurtTime;
        targetHud_.canCrit = snap.canCritNow;
        targetHud_.lastSeenMs = nowMs;
    } else if (targetHud_.active) {
        // Keep visible for 2.2s after looking away, updating live HP from players list if nearby
        if (nowMs - targetHud_.lastSeenMs > 2200) {
            targetHud_.active = false;
        } else {
            for (const auto& p : snap.players) {
                if (p.name == targetHud_.name) {
                    targetHud_.health = p.health;
                    targetHud_.maxHealth = (std::max)(1.0f, p.maxHealth);
                    targetHud_.distance = p.distance;
                    targetHud_.hurtTime = p.hurtTime;
                    break;
                }
            }
            targetHud_.canCrit = snap.canCritNow;
        }
    }

    if (targetHud_.active) {
        float lerpRate = (std::min)(1.0f, dtSec * 10.0f);
        targetHud_.animatedHealth += (targetHud_.health - targetHud_.animatedHealth) * lerpRate;
    }
}

} // namespace meow
