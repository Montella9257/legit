#pragma once

#include <vector>
#include <string>
#include <functional>
#include <cstdint>
#include "../../third_party/jni/jni.h"

namespace meow {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct AABB {
    double minX = 0.0, minY = 0.0, minZ = 0.0;
    double maxX = 0.0, maxY = 0.0, maxZ = 0.0;
};

struct PlayerSnapshot {
    std::string name;
    Vec3 pos;
    AABB box;
    float distance = 0.0f;
    float health = 20.0f;
    float maxHealth = 20.0f;
    int hurtTime = 0;
    bool invisible = false;
    bool sameTeam = false;
    bool creative = false;
    bool spectator = false;
    bool alive = true;
};

struct CrosshairTargetInfo {
    bool valid = false;
    bool isPlayer = false;
    bool isMob = false;
    bool isHostile = false;
    bool alive = false;
    bool spectator = false;
    bool invisible = false;
    bool creative = false;
    bool sameTeam = false;
    float distance = 0.0f;
    float health = 0.0f;
    float maxHealth = 20.0f;
    int hurtTime = 0;
    std::string name;
};

struct FrameSnapshot {
    bool ready = false;
    bool inWorld = false;
    bool inScreen = false;
    bool isCursorLocked = false;
    int fps = 0;
    int screenW = 0;
    int screenH = 0;
    float tickDelta = 1.0f;

    Vec3 camPos;
    float camYaw = 0.0f;
    float camPitch = 0.0f;
    float fovDeg = 70.0f;

    // Local player state
    Vec3 playerPos;
    Vec3 playerEyePos;
    Vec3 playerVel;
    float playerYaw = 0.0f;
    float playerPitch = 0.0f;
    float fallDistance = 0.0f;
    float attackCooldown = 0.0f;
    float health = 20.0f;
    float maxHealth = 20.0f;
    int foodLevel = 20;
    int selectedSlot = 0;
    int hurtTime = 0;

    bool onGround = true;
    bool sprinting = false;
    bool climbing = false;
    bool inWater = false;
    bool hasVehicle = false;
    bool blind = false;
    bool usingItem = false;
    bool blocking = false;
    bool holdingWeapon = false;
    bool hasFishHook = false;
    bool fishCaught = false;
    bool canCritNow = false;

    CrosshairTargetInfo crosshair;
    std::vector<PlayerSnapshot> players;
};

struct FrameHandles {
    jobject mc = nullptr;
    jobject player = nullptr;
    jobject world = nullptr;
    jobject interactionManager = nullptr;
    jobject options = nullptr;
    jobject mouse = nullptr;
    jobject targetEntity = nullptr;
};

class JvmBridge {
public:
    bool Init();
    void Shutdown();

    bool IsReady() const { return ready_; }
    const std::string& StatusText() const { return status_; }

    // Runs inside Render thread (wglSwapBuffers) with a scoped JNI local frame
    void WithFrame(const std::function<void(const FrameSnapshot&, const FrameHandles&)>& fn);

    // Actions (must only be called inside WithFrame callback!)
    void AttackEntity(jobject target, bool resetSprintForCrit);
    void JumpPlayer();
    void SetPlayerRotation(float yaw, float pitch);
    void SetPlayerSprinting(bool sprinting, bool sendPacket);
    void SetSprintKeyPressed(bool pressed);
    void SetUseKeyPressed(bool pressed);
    void DoItemUse();
    int  FindFoodHotbarSlot();
    void SelectHotbarSlot(int slot);
    void SetGammaOverride(bool enable);
    void UnlockCursor();
    void LockCursor();

private:
    bool ResolveJvm();
    bool ResolveBindings(JNIEnv* env);
    jclass FindMcClass(JNIEnv* env, const char* dotName);

    std::string ReadTextString(JNIEnv* env, jobject textObj);
    std::string ReadEntityName(JNIEnv* env, jobject entity, bool isPlayer);
    bool IsWeaponStack(JNIEnv* env, jobject stack);
    bool IsFoodStack(JNIEnv* env, jobject stack);
    void ClearEx(JNIEnv* env);

    JavaVM* vm_ = nullptr;
    JNIEnv* curEnv_ = nullptr;
    jobject knotClassLoader_ = nullptr;
    bool ready_ = false;
    bool attemptedInit_ = false;
    std::string status_ = "Waiting for JVM...";

    // Original gamma backup
    bool hasSavedGamma_ = false;
    jobject savedGammaObj_ = nullptr;
    bool gammaOverridden_ = false;

    // Cached global class refs
    jclass c_Class = nullptr;
    jclass c_Thread = nullptr;
    jclass c_Map = nullptr;
    jclass c_Set = nullptr;
    jclass c_List = nullptr;
    jclass c_Double = nullptr;
    jclass c_Optional = nullptr;
    jclass c_Object = nullptr;

    jclass c_MinecraftClient = nullptr;
    jclass c_ClientPlayerEntity = nullptr;
    jclass c_ClientWorld = nullptr;
    jclass c_InteractionManager = nullptr;
    jclass c_Mouse = nullptr;
    jclass c_GameOptions = nullptr;
    jclass c_SimpleOption = nullptr;
    jclass c_KeyBinding = nullptr;
    jclass c_GameRenderer = nullptr;
    jclass c_Camera = nullptr;
    jclass c_Window = nullptr;
    jclass c_RenderTickCounter = nullptr;

    jclass c_Entity = nullptr;
    jclass c_LivingEntity = nullptr;
    jclass c_PlayerEntity = nullptr;
    jclass c_MobEntity = nullptr;
    jclass c_HostileEntity = nullptr;
    jclass c_FishingBobber = nullptr;
    jclass c_EntityHitResult = nullptr;
    jclass c_Vec3d = nullptr;
    jclass c_Box = nullptr;
    jclass c_Hand = nullptr;
    jclass c_Text = nullptr;

    jclass c_HungerManager = nullptr;
    jclass c_PlayerInventory = nullptr;
    jclass c_ItemStack = nullptr;
    jclass c_SwordItem = nullptr;
    jclass c_AxeItem = nullptr;
    jclass c_MaceItem = nullptr;
    jclass c_TridentItem = nullptr;

    jclass c_Registries = nullptr;
    jclass c_Registry = nullptr;
    jclass c_Identifier = nullptr;
    jclass c_GameProfile = nullptr;

    // Cached static objects (GlobalRefs)
    jobject o_MainHand = nullptr;
    jobject o_BlindnessEntry = nullptr;
    jobject o_FoodComponent = nullptr;

    // Method & Field IDs
    jmethodID m_Class_forName = nullptr;
    jmethodID m_List_size = nullptr;
    jmethodID m_List_get = nullptr;
    jmethodID m_Double_doubleValue = nullptr;
    jmethodID m_Double_valueOf = nullptr;
    jmethodID m_Optional_orElse = nullptr;
    jmethodID m_Object_equals = nullptr;

    jmethodID m_MC_getInstance = nullptr;
    jfieldID  f_MC_player = nullptr;
    jfieldID  f_MC_world = nullptr;
    jfieldID  f_MC_interactionMgr = nullptr;
    jfieldID  f_MC_crosshairTarget = nullptr;
    jfieldID  f_MC_currentScreen = nullptr;
    jfieldID  f_MC_options = nullptr;
    jfieldID  f_MC_mouse = nullptr;
    jfieldID  f_MC_gameRenderer = nullptr;
    jmethodID m_MC_doItemUse = nullptr;
    jmethodID m_MC_getCurrentFps = nullptr;
    jmethodID m_MC_getTickCounter = nullptr;
    jmethodID m_MC_getWindow = nullptr;

    jmethodID m_TickCounter_getTickDelta = nullptr;
    jmethodID m_Window_getFbW = nullptr;
    jmethodID m_Window_getFbH = nullptr;

    jmethodID m_Mouse_unlock = nullptr;
    jmethodID m_Mouse_lock = nullptr;
    jmethodID m_Mouse_isLocked = nullptr;

    jfieldID  f_Options_gamma = nullptr;
    jfieldID  f_Options_sprintKey = nullptr;
    jfieldID  f_Options_useKey = nullptr;
    jfieldID  f_SimpleOption_value = nullptr;
    jmethodID m_KeyBinding_setPressed = nullptr;

    jmethodID m_GameRenderer_getCamera = nullptr;
    jmethodID m_GameRenderer_getFov = nullptr;
    jmethodID m_Camera_getPos = nullptr;
    jmethodID m_Camera_getYaw = nullptr;
    jmethodID m_Camera_getPitch = nullptr;

    jfieldID  f_Vec3d_x = nullptr;
    jfieldID  f_Vec3d_y = nullptr;
    jfieldID  f_Vec3d_z = nullptr;

    jfieldID  f_Box_minX = nullptr;
    jfieldID  f_Box_minY = nullptr;
    jfieldID  f_Box_minZ = nullptr;
    jfieldID  f_Box_maxX = nullptr;
    jfieldID  f_Box_maxY = nullptr;
    jfieldID  f_Box_maxZ = nullptr;

    jmethodID m_EntityHit_getEntity = nullptr;
    jmethodID m_Interaction_attackEntity = nullptr;
    jfieldID  f_World_players = nullptr;

    jfieldID  f_Entity_fallDist = nullptr;
    jfieldID  f_Entity_prevX = nullptr;
    jfieldID  f_Entity_prevY = nullptr;
    jfieldID  f_Entity_prevZ = nullptr;
    jfieldID  f_Entity_boundingBox = nullptr;
    jmethodID m_Entity_getX = nullptr;
    jmethodID m_Entity_getY = nullptr;
    jmethodID m_Entity_getZ = nullptr;
    jmethodID m_Entity_getYaw = nullptr;
    jmethodID m_Entity_getPitch = nullptr;
    jmethodID m_Entity_setYaw = nullptr;
    jmethodID m_Entity_setPitch = nullptr;
    jmethodID m_Entity_isOnGround = nullptr;
    jmethodID m_Entity_isSprinting = nullptr;
    jmethodID m_Entity_setSprinting = nullptr;
    jmethodID m_Entity_isTouchingWater = nullptr;
    jmethodID m_Entity_hasVehicle = nullptr;
    jmethodID m_Entity_isAlive = nullptr;
    jmethodID m_Entity_isSpectator = nullptr;
    jmethodID m_Entity_isInvisible = nullptr;
    jmethodID m_Entity_distanceTo = nullptr;
    jmethodID m_Entity_getEyeHeight = nullptr;
    jmethodID m_Entity_getVelocity = nullptr;
    jmethodID m_Entity_getTeam = nullptr;
    jmethodID m_Entity_getName = nullptr;

    jfieldID  f_Living_hurtTime = nullptr;
    jmethodID m_Living_getHealth = nullptr;
    jmethodID m_Living_getMaxHealth = nullptr;
    jmethodID m_Living_jump = nullptr;
    jmethodID m_Living_isClimbing = nullptr;
    jmethodID m_Living_hasStatusEffect = nullptr;
    jmethodID m_Living_isUsingItem = nullptr;
    jmethodID m_Living_isBlocking = nullptr;
    jmethodID m_Living_getMainHand = nullptr;
    jmethodID m_Living_swingHand = nullptr;

    jfieldID  f_Player_fishHook = nullptr;
    jmethodID m_Player_getCooldown = nullptr;
    jmethodID m_Player_isCreative = nullptr;
    jmethodID m_Player_getHunger = nullptr;
    jmethodID m_Player_getInventory = nullptr;
    jmethodID m_Player_getProfile = nullptr;
    jmethodID m_ClientPlayer_sendSprint = nullptr;

    jmethodID m_Hunger_getFood = nullptr;
    jfieldID  f_Inv_selectedSlot = nullptr;
    jmethodID m_Inv_getStack = nullptr;
    jfieldID  f_Bobber_caughtFish = nullptr;

    jmethodID m_ItemStack_getItem = nullptr;
    jmethodID m_ItemStack_isEmpty = nullptr;
    jmethodID m_ItemStack_contains = nullptr;

    jmethodID m_Text_getString = nullptr;
    jmethodID m_GameProfile_getName = nullptr;

    // Keep handles for actions during current frame
    FrameHandles activeHandles_{};
};

} // namespace meow
