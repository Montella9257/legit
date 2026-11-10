#include "jvm_bridge.hpp"
#include "mc_mappings.hpp"

#include <windows.h>
#include <algorithm>

namespace meow {

namespace {

using FnJNI_GetCreatedJavaVMs = jint(JNICALL*)(JavaVM**, jsize, jsize*);

struct LocalFrameGuard {
    JNIEnv* env = nullptr;
    bool active = false;
    explicit LocalFrameGuard(JNIEnv* e, jint cap = 128) : env(e) {
        if (env && env->PushLocalFrame(cap) == JNI_OK) {
            active = true;
        }
    }
    ~LocalFrameGuard() {
        if (active && env) {
            env->PopLocalFrame(nullptr);
        }
    }
};

jclass ToGlobalClass(JNIEnv* env, jclass local) {
    if (!local) return nullptr;
    jclass g = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return g;
}

} // namespace

void JvmBridge::ClearEx(JNIEnv* env) {
    if (env && env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

bool JvmBridge::ResolveJvm() {
    if (vm_) return true;
    HMODULE hJvm = GetModuleHandleW(L"jvm.dll");
    if (!hJvm) {
        status_ = "jvm.dll not loaded";
        return false;
    }
    auto fnGetVMs = reinterpret_cast<FnJNI_GetCreatedJavaVMs>(GetProcAddress(hJvm, "JNI_GetCreatedJavaVMs"));
    if (!fnGetVMs) {
        status_ = "JNI_GetCreatedJavaVMs missing";
        return false;
    }
    jsize count = 0;
    JavaVM* vms[4]{};
    if (fnGetVMs(vms, 4, &count) != JNI_OK || count <= 0 || !vms[0]) {
        status_ = "No running JVM found";
        return false;
    }
    vm_ = vms[0];
    return true;
}

jclass JvmBridge::FindMcClass(JNIEnv* env, const char* dotName) {
    if (!c_Class || !m_Class_forName || !knotClassLoader_) return nullptr;
    jstring jName = env->NewStringUTF(dotName);
    if (!jName) { ClearEx(env); return nullptr; }
    jobject clsObj = env->CallStaticObjectMethod(c_Class, m_Class_forName, jName, JNI_FALSE, knotClassLoader_);
    env->DeleteLocalRef(jName);
    if (env->ExceptionCheck() || !clsObj) {
        ClearEx(env);
        return nullptr;
    }
    return ToGlobalClass(env, static_cast<jclass>(clsObj));
}

bool JvmBridge::Init() {
    if (ready_) return true;
    if (!ResolveJvm()) return false;

    JNIEnv* env = nullptr;
    jint res = vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8);
    if (res == JNI_EDETACHED) {
        if (vm_->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) {
            status_ = "Failed to attach thread to JVM";
            return false;
        }
    } else if (res != JNI_OK || !env) {
        status_ = "GetEnv failed";
        return false;
    }

    LocalFrameGuard frame(env, 128);
    if (!frame.active) return false;

    if (!ResolveBindings(env)) {
        return false;
    }
    ready_ = true;
    status_ = "Fabric 1.21.4 Connected";
    return true;
}

bool JvmBridge::ResolveBindings(JNIEnv* env) {
    auto grabSysClass = [&](const char* name) -> jclass {
        jclass c = env->FindClass(name);
        if (env->ExceptionCheck() || !c) { ClearEx(env); return nullptr; }
        return ToGlobalClass(env, c);
    };

    c_Class    = grabSysClass("java/lang/Class");
    c_Thread   = grabSysClass("java/lang/Thread");
    c_Map      = grabSysClass("java/util/Map");
    c_Set      = grabSysClass("java/util/Set");
    c_List     = grabSysClass("java/util/List");
    c_Double   = grabSysClass("java/lang/Double");
    c_Optional = grabSysClass("java/util/Optional");
    c_Object   = grabSysClass("java/lang/Object");

    if (!c_Class || !c_Thread || !c_Map || !c_Set || !c_List || !c_Double || !c_Optional || !c_Object) {
        status_ = "Missing core Java classes";
        return false;
    }

    m_Class_forName = env->GetStaticMethodID(c_Class, "forName", "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;");
    m_List_size     = env->GetMethodID(c_List, "size", "()I");
    m_List_get      = env->GetMethodID(c_List, "get", "(I)Ljava/lang/Object;");
    m_Double_doubleValue = env->GetMethodID(c_Double, "doubleValue", "()D");
    m_Double_valueOf     = env->GetStaticMethodID(c_Double, "valueOf", "(D)Ljava/lang/Double;");
    m_Optional_orElse    = env->GetMethodID(c_Optional, "orElse", "(Ljava/lang/Object;)Ljava/lang/Object;");
    m_Object_equals      = env->GetMethodID(c_Object, "equals", "(Ljava/lang/Object;)Z");

    if (!m_Class_forName || !m_List_size || !m_List_get || !m_Double_doubleValue || !m_Double_valueOf) {
        status_ = "Missing core Java method IDs";
        return false;
    }

    // Locate Fabric's KnotClassLoader via active thread context classloaders
    if (!knotClassLoader_) {
        jmethodID m_curThread = env->GetStaticMethodID(c_Thread, "currentThread", "()Ljava/lang/Thread;");
        jmethodID m_getCtxCL  = env->GetMethodID(c_Thread, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
        jmethodID m_allTraces = env->GetStaticMethodID(c_Thread, "getAllStackTraces", "()Ljava/util/Map;");
        jmethodID m_mapKeySet = env->GetMethodID(c_Map, "keySet", "()Ljava/util/Set;");
        jmethodID m_setToArr  = env->GetMethodID(c_Set, "toArray", "()[Ljava/lang/Object;");

        auto tryLoader = [&](jobject loader) -> bool {
            if (!loader) return false;
            jstring testName = env->NewStringUTF(mc::cls::MinecraftClient);
            jobject clsObj = env->CallStaticObjectMethod(c_Class, m_Class_forName, testName, JNI_FALSE, loader);
            env->DeleteLocalRef(testName);
            if (env->ExceptionCheck() || !clsObj) {
                ClearEx(env);
                return false;
            }
            env->DeleteLocalRef(clsObj);
            knotClassLoader_ = env->NewGlobalRef(loader);
            return true;
        };

        if (m_curThread && m_getCtxCL) {
            jobject cur = env->CallStaticObjectMethod(c_Thread, m_curThread);
            if (cur) {
                jobject cl = env->CallObjectMethod(cur, m_getCtxCL);
                tryLoader(cl);
                if (cl) env->DeleteLocalRef(cl);
                env->DeleteLocalRef(cur);
            }
            ClearEx(env);
        }

        if (!knotClassLoader_ && m_allTraces && m_mapKeySet && m_setToArr && m_getCtxCL) {
            jobject mapObj = env->CallStaticObjectMethod(c_Thread, m_allTraces);
            if (mapObj) {
                jobject keySet = env->CallObjectMethod(mapObj, m_mapKeySet);
                if (keySet) {
                    jobjectArray arr = static_cast<jobjectArray>(env->CallObjectMethod(keySet, m_setToArr));
                    if (arr) {
                        jsize len = env->GetArrayLength(arr);
                        for (jsize i = 0; i < len && !knotClassLoader_; ++i) {
                            jobject thr = env->GetObjectArrayElement(arr, i);
                            if (thr) {
                                jobject cl = env->CallObjectMethod(thr, m_getCtxCL);
                                tryLoader(cl);
                                if (cl) env->DeleteLocalRef(cl);
                                env->DeleteLocalRef(thr);
                            }
                        }
                        env->DeleteLocalRef(arr);
                    }
                    env->DeleteLocalRef(keySet);
                }
                env->DeleteLocalRef(mapObj);
            }
            ClearEx(env);
        }
    }

    if (!knotClassLoader_) {
        status_ = "KnotClassLoader not found (not Fabric 1.21.4?)";
        return false;
    }

    // Resolve Minecraft Intermediary 1.21.4 classes
    c_MinecraftClient    = FindMcClass(env, mc::cls::MinecraftClient);
    c_ClientPlayerEntity = FindMcClass(env, mc::cls::ClientPlayerEntity);
    c_ClientWorld        = FindMcClass(env, mc::cls::ClientWorld);
    c_InteractionManager = FindMcClass(env, mc::cls::InteractionManager);
    c_Mouse              = FindMcClass(env, mc::cls::Mouse);
    c_GameOptions        = FindMcClass(env, mc::cls::GameOptions);
    c_SimpleOption       = FindMcClass(env, mc::cls::SimpleOption);
    c_KeyBinding         = FindMcClass(env, mc::cls::KeyBinding);
    c_GameRenderer       = FindMcClass(env, mc::cls::GameRenderer);
    c_Camera             = FindMcClass(env, mc::cls::Camera);
    c_Window             = FindMcClass(env, mc::cls::Window);
    c_RenderTickCounter  = FindMcClass(env, mc::cls::RenderTickCounter);

    c_Entity             = FindMcClass(env, mc::cls::Entity);
    c_LivingEntity       = FindMcClass(env, mc::cls::LivingEntity);
    c_PlayerEntity       = FindMcClass(env, mc::cls::PlayerEntity);
    c_MobEntity          = FindMcClass(env, mc::cls::MobEntity);
    c_HostileEntity      = FindMcClass(env, mc::cls::HostileEntity);
    c_FishingBobber      = FindMcClass(env, mc::cls::FishingBobberEntity);
    c_EntityHitResult    = FindMcClass(env, mc::cls::EntityHitResult);
    c_Vec3d              = FindMcClass(env, mc::cls::Vec3d);
    c_Box                = FindMcClass(env, mc::cls::Box);
    c_Hand               = FindMcClass(env, mc::cls::Hand);
    c_Text               = FindMcClass(env, mc::cls::Text);

    c_HungerManager      = FindMcClass(env, mc::cls::HungerManager);
    c_PlayerInventory    = FindMcClass(env, mc::cls::PlayerInventory);
    c_ItemStack          = FindMcClass(env, mc::cls::ItemStack);
    c_SwordItem          = FindMcClass(env, mc::cls::SwordItem);
    c_AxeItem            = FindMcClass(env, mc::cls::AxeItem);
    c_MaceItem           = FindMcClass(env, mc::cls::MaceItem);
    c_TridentItem        = FindMcClass(env, mc::cls::TridentItem);

    c_Registries         = FindMcClass(env, mc::cls::Registries);
    c_Registry           = FindMcClass(env, mc::cls::Registry);
    c_Identifier         = FindMcClass(env, mc::cls::Identifier);
    c_GameProfile        = FindMcClass(env, mc::cls::GameProfile);

    if (!c_MinecraftClient || !c_ClientPlayerEntity || !c_ClientWorld || !c_InteractionManager ||
        !c_Entity || !c_LivingEntity || !c_PlayerEntity || !c_EntityHitResult) {
        status_ = "MC 1.21.4 mappings mismatch";
        return false;
    }

    auto mid = [&](jclass c, const char* name, const char* sig) -> jmethodID {
        if (!c) return nullptr;
        jmethodID m = env->GetMethodID(c, name, sig);
        if (env->ExceptionCheck()) { ClearEx(env); return nullptr; }
        return m;
    };
    auto smid = [&](jclass c, const char* name, const char* sig) -> jmethodID {
        if (!c) return nullptr;
        jmethodID m = env->GetStaticMethodID(c, name, sig);
        if (env->ExceptionCheck()) { ClearEx(env); return nullptr; }
        return m;
    };
    auto fid = [&](jclass c, const char* name, const char* sig) -> jfieldID {
        if (!c) return nullptr;
        jfieldID f = env->GetFieldID(c, name, sig);
        if (env->ExceptionCheck()) { ClearEx(env); return nullptr; }
        return f;
    };
    auto sfid = [&](jclass c, const char* name, const char* sig) -> jfieldID {
        if (!c) return nullptr;
        jfieldID f = env->GetStaticFieldID(c, name, sig);
        if (env->ExceptionCheck()) { ClearEx(env); return nullptr; }
        return f;
    };

    m_MC_getInstance    = smid(c_MinecraftClient, mc::mc_client::getInstance, mc::mc_client::getInstanceSig);
    f_MC_player         = fid(c_MinecraftClient, mc::mc_client::player, mc::mc_client::playerSig);
    f_MC_world          = fid(c_MinecraftClient, mc::mc_client::world, mc::mc_client::worldSig);
    f_MC_interactionMgr = fid(c_MinecraftClient, mc::mc_client::interactionManager, mc::mc_client::interactionManagerSig);
    f_MC_crosshairTarget= fid(c_MinecraftClient, mc::mc_client::crosshairTarget, mc::mc_client::crosshairTargetSig);
    f_MC_currentScreen  = fid(c_MinecraftClient, mc::mc_client::currentScreen, mc::mc_client::currentScreenSig);
    f_MC_options        = fid(c_MinecraftClient, mc::mc_client::options, mc::mc_client::optionsSig);
    f_MC_mouse          = fid(c_MinecraftClient, mc::mc_client::mouse, mc::mc_client::mouseSig);
    f_MC_gameRenderer   = fid(c_MinecraftClient, mc::mc_client::gameRenderer, mc::mc_client::gameRendererSig);
    m_MC_doItemUse      = mid(c_MinecraftClient, mc::mc_client::doItemUse, "()V");
    m_MC_getCurrentFps  = mid(c_MinecraftClient, mc::mc_client::getCurrentFps, "()I");
    m_MC_getTickCounter = mid(c_MinecraftClient, mc::mc_client::getRenderTickCounter, mc::mc_client::getRenderTickCounterSig);
    m_MC_getWindow      = mid(c_MinecraftClient, mc::mc_client::getWindow, mc::mc_client::getWindowSig);

    m_TickCounter_getTickDelta = mid(c_RenderTickCounter, mc::tick_counter::getTickDelta, "(Z)F");
    m_Window_getFbW     = mid(c_Window, mc::window::getFramebufferWidth, "()I");
    m_Window_getFbH     = mid(c_Window, mc::window::getFramebufferHeight, "()I");

    m_Mouse_unlock      = mid(c_Mouse, mc::mouse::unlockCursor, "()V");
    m_Mouse_lock        = mid(c_Mouse, mc::mouse::lockCursor, "()V");
    m_Mouse_isLocked    = mid(c_Mouse, mc::mouse::isCursorLocked, "()Z");

    f_Options_gamma     = fid(c_GameOptions, mc::options::gamma, mc::options::gammaSig);
    f_Options_sprintKey = fid(c_GameOptions, mc::options::sprintKey, mc::options::keySig);
    f_Options_useKey    = fid(c_GameOptions, mc::options::useKey, mc::options::keySig);
    f_SimpleOption_value= fid(c_SimpleOption, mc::simple_option::value, mc::simple_option::valueSig);
    m_KeyBinding_setPressed = mid(c_KeyBinding, mc::key_binding::setPressed, "(Z)V");

    m_GameRenderer_getCamera = mid(c_GameRenderer, mc::game_renderer::getCamera, mc::game_renderer::getCameraSig);
    m_GameRenderer_getFov    = mid(c_GameRenderer, mc::game_renderer::getFov, mc::game_renderer::getFovSig);
    m_Camera_getPos     = mid(c_Camera, mc::camera::getPos, mc::camera::getPosSig);
    m_Camera_getYaw     = mid(c_Camera, mc::camera::getYaw, "()F");
    m_Camera_getPitch   = mid(c_Camera, mc::camera::getPitch, "()F");

    f_Vec3d_x = fid(c_Vec3d, mc::vec3d::x, "D");
    f_Vec3d_y = fid(c_Vec3d, mc::vec3d::y, "D");
    f_Vec3d_z = fid(c_Vec3d, mc::vec3d::z, "D");

    f_Box_minX = fid(c_Box, mc::box::minX, "D");
    f_Box_minY = fid(c_Box, mc::box::minY, "D");
    f_Box_minZ = fid(c_Box, mc::box::minZ, "D");
    f_Box_maxX = fid(c_Box, mc::box::maxX, "D");
    f_Box_maxY = fid(c_Box, mc::box::maxY, "D");
    f_Box_maxZ = fid(c_Box, mc::box::maxZ, "D");

    m_EntityHit_getEntity      = mid(c_EntityHitResult, mc::entity_hit::getEntity, mc::entity_hit::getEntitySig);
    m_Interaction_attackEntity = mid(c_InteractionManager, mc::interaction::attackEntity, mc::interaction::attackEntitySig);
    f_World_players            = fid(c_ClientWorld, mc::world::players, mc::world::playersSig);

    f_Entity_fallDist      = fid(c_Entity, mc::entity::fallDistance, "F");
    f_Entity_prevX         = fid(c_Entity, mc::entity::prevX, "D");
    f_Entity_prevY         = fid(c_Entity, mc::entity::prevY, "D");
    f_Entity_prevZ         = fid(c_Entity, mc::entity::prevZ, "D");
    f_Entity_boundingBox   = fid(c_Entity, mc::entity::boundingBox, mc::entity::boundingBoxSig);
    m_Entity_getX          = mid(c_Entity, mc::entity::getX, "()D");
    m_Entity_getY          = mid(c_Entity, mc::entity::getY, "()D");
    m_Entity_getZ          = mid(c_Entity, mc::entity::getZ, "()D");
    m_Entity_getYaw        = mid(c_Entity, mc::entity::getYaw, "()F");
    m_Entity_getPitch      = mid(c_Entity, mc::entity::getPitch, "()F");
    m_Entity_setYaw        = mid(c_Entity, mc::entity::setYaw, "(F)V");
    m_Entity_setPitch      = mid(c_Entity, mc::entity::setPitch, "(F)V");
    m_Entity_isOnGround    = mid(c_Entity, mc::entity::isOnGround, "()Z");
    m_Entity_isSprinting   = mid(c_Entity, mc::entity::isSprinting, "()Z");
    m_Entity_setSprinting  = mid(c_Entity, mc::entity::setSprinting, "(Z)V");
    m_Entity_isTouchingWater = mid(c_Entity, mc::entity::isTouchingWater, "()Z");
    m_Entity_hasVehicle    = mid(c_Entity, mc::entity::hasVehicle, "()Z");
    m_Entity_isAlive       = mid(c_Entity, mc::entity::isAlive, "()Z");
    m_Entity_isSpectator   = mid(c_Entity, mc::entity::isSpectator, "()Z");
    m_Entity_isInvisible   = mid(c_Entity, mc::entity::isInvisible, "()Z");
    m_Entity_distanceTo    = mid(c_Entity, mc::entity::distanceTo, mc::entity::distanceToSig);
    m_Entity_getEyeHeight  = mid(c_Entity, mc::entity::getStandingEyeHeight, "()F");
    m_Entity_getVelocity   = mid(c_Entity, mc::entity::getVelocity, mc::entity::getVelocitySig);
    m_Entity_getTeam       = mid(c_Entity, mc::entity::getScoreboardTeam, mc::entity::getScoreboardTeamSig);
    m_Entity_getName       = mid(c_Entity, mc::entity::getName, mc::entity::getNameSig);

    f_Living_hurtTime        = fid(c_LivingEntity, mc::living::hurtTime, "I");
    m_Living_getHealth       = mid(c_LivingEntity, mc::living::getHealth, "()F");
    m_Living_getMaxHealth    = mid(c_LivingEntity, mc::living::getMaxHealth, "()F");
    m_Living_jump            = mid(c_LivingEntity, mc::living::jump, "()V");
    m_Living_isClimbing      = mid(c_LivingEntity, mc::living::isClimbing, "()Z");
    m_Living_hasStatusEffect = mid(c_LivingEntity, mc::living::hasStatusEffect, mc::living::hasStatusEffectSig);
    m_Living_isUsingItem     = mid(c_LivingEntity, mc::living::isUsingItem, "()Z");
    m_Living_isBlocking      = mid(c_LivingEntity, mc::living::isBlocking, "()Z");
    m_Living_getMainHand     = mid(c_LivingEntity, mc::living::getMainHandStack, mc::living::getMainHandStackSig);
    m_Living_swingHand       = mid(c_LivingEntity, mc::living::swingHand, mc::living::swingHandSig);

    f_Player_fishHook        = fid(c_PlayerEntity, mc::player::fishHook, mc::player::fishHookSig);
    m_Player_getCooldown     = mid(c_PlayerEntity, mc::player::getAttackCooldown, "(F)F");
    m_Player_isCreative      = mid(c_PlayerEntity, mc::player::isCreative, "()Z");
    m_Player_getHunger       = mid(c_PlayerEntity, mc::player::getHungerManager, mc::player::getHungerManagerSig);
    m_Player_getInventory    = mid(c_PlayerEntity, mc::player::getInventory, mc::player::getInventorySig);
    m_Player_getProfile      = mid(c_PlayerEntity, mc::player::getGameProfile, mc::player::getGameProfileSig);
    m_ClientPlayer_sendSprint= mid(c_ClientPlayerEntity, mc::player::sendSprintingPacket, "()V");

    m_Hunger_getFood         = mid(c_HungerManager, mc::hunger::getFoodLevel, "()I");
    f_Inv_selectedSlot       = fid(c_PlayerInventory, mc::inventory::selectedSlot, "I");
    m_Inv_getStack           = mid(c_PlayerInventory, mc::inventory::getStack, mc::inventory::getStackSig);
    f_Bobber_caughtFish      = fid(c_FishingBobber, mc::bobber::caughtFish, "Z");

    m_ItemStack_getItem      = mid(c_ItemStack, mc::item_stack::getItem, mc::item_stack::getItemSig);
    m_ItemStack_isEmpty      = mid(c_ItemStack, mc::item_stack::isEmpty, "()Z");
    m_ItemStack_contains     = mid(c_ItemStack, mc::item_stack::contains, mc::item_stack::containsSig);

    m_Text_getString         = mid(c_Text, "getString", "()Ljava/lang/String;");
    m_GameProfile_getName    = mid(c_GameProfile, "getName", "()Ljava/lang/String;");

    // Cache Hand.MAIN_HAND
    jfieldID f_mainHand = sfid(c_Hand, mc::hand::MAIN_HAND, mc::hand::sig);
    if (f_mainHand) {
        jobject mh = env->GetStaticObjectField(c_Hand, f_mainHand);
        if (mh && !env->ExceptionCheck()) {
            o_MainHand = env->NewGlobalRef(mh);
            env->DeleteLocalRef(mh);
        }
        ClearEx(env);
    }

    // Cache StatusEffects blindness entry & DataComponentTypes food via Registries
    if (c_Registries && c_Registry && c_Identifier) {
        jmethodID m_idOf = smid(c_Identifier, mc::registries::idOf, mc::registries::idOfSig);
        jmethodID m_regGet = mid(c_Registry, mc::registries::regGet, mc::registries::regGetSig);
        jmethodID m_regGetEntry = mid(c_Registry, mc::registries::regGetEntry, mc::registries::regGetEntrySig);

        jfieldID f_statusReg = sfid(c_Registries, mc::registries::STATUS_EFFECT, mc::registries::registrySig);
        if (m_idOf && m_regGetEntry && f_statusReg) {
            jobject reg = env->GetStaticObjectField(c_Registries, f_statusReg);
            jstring sBlind = env->NewStringUTF("minecraft:blindness");
            jobject idBlind = env->CallStaticObjectMethod(c_Identifier, m_idOf, sBlind);
            if (reg && idBlind && !env->ExceptionCheck()) {
                jobject opt = env->CallObjectMethod(reg, m_regGetEntry, idBlind);
                if (opt && !env->ExceptionCheck()) {
                    jobject entry = env->CallObjectMethod(opt, m_Optional_orElse, nullptr);
                    if (entry && !env->ExceptionCheck()) {
                        o_BlindnessEntry = env->NewGlobalRef(entry);
                        env->DeleteLocalRef(entry);
                    }
                    env->DeleteLocalRef(opt);
                }
            }
            if (sBlind) env->DeleteLocalRef(sBlind);
            if (idBlind) env->DeleteLocalRef(idBlind);
            if (reg) env->DeleteLocalRef(reg);
            ClearEx(env);
        }

        jfieldID f_compReg = sfid(c_Registries, mc::registries::DATA_COMPONENT_TYPE, mc::registries::registrySig);
        if (m_idOf && m_regGet && f_compReg) {
            jobject reg = env->GetStaticObjectField(c_Registries, f_compReg);
            jstring sFood = env->NewStringUTF("minecraft:food");
            jobject idFood = env->CallStaticObjectMethod(c_Identifier, m_idOf, sFood);
            if (reg && idFood && !env->ExceptionCheck()) {
                jobject comp = env->CallObjectMethod(reg, m_regGet, idFood);
                if (comp && !env->ExceptionCheck()) {
                    o_FoodComponent = env->NewGlobalRef(comp);
                    env->DeleteLocalRef(comp);
                }
            }
            if (sFood) env->DeleteLocalRef(sFood);
            if (idFood) env->DeleteLocalRef(idFood);
            if (reg) env->DeleteLocalRef(reg);
            ClearEx(env);
        }
    }

    if (!m_MC_getInstance || !f_MC_player || !f_MC_world || !m_Interaction_attackEntity || !m_Player_getCooldown) {
        status_ = "Failed to bind critical MC 1.21.4 methods";
        return false;
    }

    return true;
}

void JvmBridge::Shutdown() {
    if (!vm_) return;
    JNIEnv* env = nullptr;
    if (vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) == JNI_OK && env) {
        if (gammaOverridden_) {
            LocalFrameGuard frame(env, 32);
            activeHandles_.mc = env->CallStaticObjectMethod(c_MinecraftClient, m_MC_getInstance);
            ClearEx(env);
            if (activeHandles_.mc) {
                activeHandles_.options = env->GetObjectField(activeHandles_.mc, f_MC_options);
                SetGammaOverride(false);
            }
            activeHandles_ = {};
        }

        auto delRef = [&](jobject& r) {
            if (r) { env->DeleteGlobalRef(r); r = nullptr; }
        };
        auto delCls = [&](jclass& c) {
            if (c) { env->DeleteGlobalRef(c); c = nullptr; }
        };

        delRef(knotClassLoader_);
        delRef(savedGammaObj_);
        delRef(o_MainHand);
        delRef(o_BlindnessEntry);
        delRef(o_FoodComponent);

        delCls(c_Class); delCls(c_Thread); delCls(c_Map); delCls(c_Set);
        delCls(c_List); delCls(c_Double); delCls(c_Optional); delCls(c_Object);
        delCls(c_MinecraftClient); delCls(c_ClientPlayerEntity); delCls(c_ClientWorld);
        delCls(c_InteractionManager); delCls(c_Mouse); delCls(c_GameOptions);
        delCls(c_SimpleOption); delCls(c_KeyBinding); delCls(c_GameRenderer);
        delCls(c_Camera); delCls(c_Window); delCls(c_RenderTickCounter);
        delCls(c_Entity); delCls(c_LivingEntity); delCls(c_PlayerEntity);
        delCls(c_MobEntity); delCls(c_HostileEntity); delCls(c_FishingBobber);
        delCls(c_EntityHitResult); delCls(c_Vec3d); delCls(c_Box);
        delCls(c_Hand); delCls(c_Text); delCls(c_HungerManager);
        delCls(c_PlayerInventory); delCls(c_ItemStack); delCls(c_SwordItem);
        delCls(c_AxeItem); delCls(c_MaceItem); delCls(c_TridentItem);
        delCls(c_Registries); delCls(c_Registry); delCls(c_Identifier);
        delCls(c_GameProfile);
    }
    ready_ = false;
}

std::string JvmBridge::ReadTextString(JNIEnv* env, jobject textObj) {
    if (!textObj || !m_Text_getString) return {};
    jstring jstr = static_cast<jstring>(env->CallObjectMethod(textObj, m_Text_getString));
    if (env->ExceptionCheck() || !jstr) { ClearEx(env); return {}; }
    const char* utf = env->GetStringUTFChars(jstr, nullptr);
    std::string out = utf ? utf : "";
    if (utf) env->ReleaseStringUTFChars(jstr, utf);
    env->DeleteLocalRef(jstr);
    return out;
}

std::string JvmBridge::ReadEntityName(JNIEnv* env, jobject entity, bool isPlayer) {
    if (!entity) return {};
    if (isPlayer && m_Player_getProfile && m_GameProfile_getName) {
        jobject prof = env->CallObjectMethod(entity, m_Player_getProfile);
        if (!env->ExceptionCheck() && prof) {
            jstring jstr = static_cast<jstring>(env->CallObjectMethod(prof, m_GameProfile_getName));
            if (!env->ExceptionCheck() && jstr) {
                const char* utf = env->GetStringUTFChars(jstr, nullptr);
                std::string out = utf ? utf : "";
                if (utf) env->ReleaseStringUTFChars(jstr, utf);
                env->DeleteLocalRef(jstr);
                env->DeleteLocalRef(prof);
                if (!out.empty()) return out;
            }
            env->DeleteLocalRef(prof);
        }
        ClearEx(env);
    }
    if (m_Entity_getName) {
        jobject textObj = env->CallObjectMethod(entity, m_Entity_getName);
        if (!env->ExceptionCheck() && textObj) {
            std::string out = ReadTextString(env, textObj);
            env->DeleteLocalRef(textObj);
            return out;
        }
        ClearEx(env);
    }
    return "Entity";
}

bool JvmBridge::IsWeaponStack(JNIEnv* env, jobject stack) {
    if (!stack || !m_ItemStack_isEmpty || !m_ItemStack_getItem) return false;
    if (env->CallBooleanMethod(stack, m_ItemStack_isEmpty)) return false;
    jobject item = env->CallObjectMethod(stack, m_ItemStack_getItem);
    if (env->ExceptionCheck() || !item) { ClearEx(env); return false; }
    bool ok = (c_SwordItem   && env->IsInstanceOf(item, c_SwordItem)) ||
              (c_AxeItem     && env->IsInstanceOf(item, c_AxeItem)) ||
              (c_MaceItem    && env->IsInstanceOf(item, c_MaceItem)) ||
              (c_TridentItem && env->IsInstanceOf(item, c_TridentItem));
    env->DeleteLocalRef(item);
    return ok;
}

bool JvmBridge::IsFoodStack(JNIEnv* env, jobject stack) {
    if (!stack || !o_FoodComponent || !m_ItemStack_contains || !m_ItemStack_isEmpty) return false;
    if (env->CallBooleanMethod(stack, m_ItemStack_isEmpty)) return false;
    jboolean hasFood = env->CallBooleanMethod(stack, m_ItemStack_contains, o_FoodComponent);
    if (env->ExceptionCheck()) { ClearEx(env); return false; }
    return hasFood == JNI_TRUE;
}

void JvmBridge::WithFrame(const std::function<void(const FrameSnapshot&, const FrameHandles&)>& fn) {
    FrameSnapshot snap;
    FrameHandles handles;

    if (!ready_) {
        if (!Init()) {
            fn(snap, handles);
            return;
        }
    }

    JNIEnv* env = nullptr;
    if (!vm_ || vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK || !env) {
        fn(snap, handles);
        return;
    }

    LocalFrameGuard frame(env, 128);
    if (!frame.active) {
        fn(snap, handles);
        return;
    }

    curEnv_ = env;
    snap.ready = true;

    jobject mc = env->CallStaticObjectMethod(c_MinecraftClient, m_MC_getInstance);
    if (env->ExceptionCheck() || !mc) { ClearEx(env); curEnv_ = nullptr; fn(snap, handles); return; }
    handles.mc = mc;

    jobject player = env->GetObjectField(mc, f_MC_player);
    jobject world  = env->GetObjectField(mc, f_MC_world);
    jobject im     = env->GetObjectField(mc, f_MC_interactionMgr);
    jobject opts   = f_MC_options ? env->GetObjectField(mc, f_MC_options) : nullptr;
    jobject mouse  = f_MC_mouse ? env->GetObjectField(mc, f_MC_mouse) : nullptr;
    jobject screen = f_MC_currentScreen ? env->GetObjectField(mc, f_MC_currentScreen) : nullptr;
    ClearEx(env);

    handles.player = player;
    handles.world = world;
    handles.interactionManager = im;
    handles.options = opts;
    handles.mouse = mouse;

    snap.inScreen = (screen != nullptr);
    if (m_MC_getCurrentFps) {
        snap.fps = env->CallIntMethod(mc, m_MC_getCurrentFps);
        ClearEx(env);
    }
    if (mouse && m_Mouse_isLocked) {
        snap.isCursorLocked = (env->CallBooleanMethod(mouse, m_Mouse_isLocked) == JNI_TRUE);
        ClearEx(env);
    }
    if (m_MC_getWindow && m_Window_getFbW && m_Window_getFbH) {
        jobject win = env->CallObjectMethod(mc, m_MC_getWindow);
        if (win && !env->ExceptionCheck()) {
            snap.screenW = env->CallIntMethod(win, m_Window_getFbW);
            snap.screenH = env->CallIntMethod(win, m_Window_getFbH);
        }
        ClearEx(env);
    }

    if (player && world && im) {
        snap.inWorld = true;

        // Tick delta
        if (m_MC_getTickCounter && m_TickCounter_getTickDelta) {
            jobject tc = env->CallObjectMethod(mc, m_MC_getTickCounter);
            if (tc && !env->ExceptionCheck()) {
                snap.tickDelta = env->CallFloatMethod(tc, m_TickCounter_getTickDelta, JNI_TRUE);
            }
            ClearEx(env);
        }

        // Camera & FOV
        jobject gr = f_MC_gameRenderer ? env->GetObjectField(mc, f_MC_gameRenderer) : nullptr;
        if (gr && m_GameRenderer_getCamera) {
            jobject cam = env->CallObjectMethod(gr, m_GameRenderer_getCamera);
            if (cam && !env->ExceptionCheck()) {
                jobject camPosObj = m_Camera_getPos ? env->CallObjectMethod(cam, m_Camera_getPos) : nullptr;
                if (camPosObj && !env->ExceptionCheck() && f_Vec3d_x) {
                    snap.camPos.x = env->GetDoubleField(camPosObj, f_Vec3d_x);
                    snap.camPos.y = env->GetDoubleField(camPosObj, f_Vec3d_y);
                    snap.camPos.z = env->GetDoubleField(camPosObj, f_Vec3d_z);
                }
                if (m_Camera_getYaw)   snap.camYaw   = env->CallFloatMethod(cam, m_Camera_getYaw);
                if (m_Camera_getPitch) snap.camPitch = env->CallFloatMethod(cam, m_Camera_getPitch);
                if (m_GameRenderer_getFov) {
                    snap.fovDeg = env->CallFloatMethod(gr, m_GameRenderer_getFov, cam, snap.tickDelta, JNI_TRUE);
                }
            }
            ClearEx(env);
        }

        // Local player state
        snap.playerPos.x = env->CallDoubleMethod(player, m_Entity_getX);
        snap.playerPos.y = env->CallDoubleMethod(player, m_Entity_getY);
        snap.playerPos.z = env->CallDoubleMethod(player, m_Entity_getZ);
        float eyeH = m_Entity_getEyeHeight ? env->CallFloatMethod(player, m_Entity_getEyeHeight) : 1.62f;
        snap.playerEyePos = { snap.playerPos.x, snap.playerPos.y + eyeH, snap.playerPos.z };
        snap.playerYaw   = env->CallFloatMethod(player, m_Entity_getYaw);
        snap.playerPitch = env->CallFloatMethod(player, m_Entity_getPitch);
        snap.fallDistance = env->GetFloatField(player, f_Entity_fallDist);
        snap.onGround    = (env->CallBooleanMethod(player, m_Entity_isOnGround) == JNI_TRUE);
        snap.sprinting   = (env->CallBooleanMethod(player, m_Entity_isSprinting) == JNI_TRUE);
        snap.inWater     = (env->CallBooleanMethod(player, m_Entity_isTouchingWater) == JNI_TRUE);
        snap.hasVehicle  = (env->CallBooleanMethod(player, m_Entity_hasVehicle) == JNI_TRUE);
        snap.climbing    = (env->CallBooleanMethod(player, m_Living_isClimbing) == JNI_TRUE);
        snap.health      = env->CallFloatMethod(player, m_Living_getHealth);
        snap.maxHealth   = env->CallFloatMethod(player, m_Living_getMaxHealth);
        snap.hurtTime    = f_Living_hurtTime ? env->GetIntField(player, f_Living_hurtTime) : 0;
        snap.usingItem   = m_Living_isUsingItem ? (env->CallBooleanMethod(player, m_Living_isUsingItem) == JNI_TRUE) : false;
        snap.blocking    = m_Living_isBlocking ? (env->CallBooleanMethod(player, m_Living_isBlocking) == JNI_TRUE) : false;
        snap.attackCooldown = env->CallFloatMethod(player, m_Player_getCooldown, 0.5f);
        ClearEx(env);

        if (m_Entity_getVelocity && f_Vec3d_x) {
            jobject vel = env->CallObjectMethod(player, m_Entity_getVelocity);
            if (vel && !env->ExceptionCheck()) {
                snap.playerVel.x = env->GetDoubleField(vel, f_Vec3d_x);
                snap.playerVel.y = env->GetDoubleField(vel, f_Vec3d_y);
                snap.playerVel.z = env->GetDoubleField(vel, f_Vec3d_z);
            }
            ClearEx(env);
        }

        if (o_BlindnessEntry && m_Living_hasStatusEffect) {
            snap.blind = (env->CallBooleanMethod(player, m_Living_hasStatusEffect, o_BlindnessEntry) == JNI_TRUE);
            ClearEx(env);
        }

        if (m_Living_getMainHand) {
            jobject handStack = env->CallObjectMethod(player, m_Living_getMainHand);
            if (handStack && !env->ExceptionCheck()) {
                snap.holdingWeapon = IsWeaponStack(env, handStack);
            }
            ClearEx(env);
        }

        if (m_Player_getHunger && m_Hunger_getFood) {
            jobject hm = env->CallObjectMethod(player, m_Player_getHunger);
            if (hm && !env->ExceptionCheck()) {
                snap.foodLevel = env->CallIntMethod(hm, m_Hunger_getFood);
            }
            ClearEx(env);
        }

        if (m_Player_getInventory && f_Inv_selectedSlot) {
            jobject inv = env->CallObjectMethod(player, m_Player_getInventory);
            if (inv && !env->ExceptionCheck()) {
                snap.selectedSlot = env->GetIntField(inv, f_Inv_selectedSlot);
            }
            ClearEx(env);
        }

        if (f_Player_fishHook && f_Bobber_caughtFish) {
            jobject bobber = env->GetObjectField(player, f_Player_fishHook);
            if (bobber && !env->ExceptionCheck()) {
                snap.hasFishHook = true;
                snap.fishCaught = (env->GetBooleanField(bobber, f_Bobber_caughtFish) == JNI_TRUE);
            }
            ClearEx(env);
        }

        snap.canCritNow = (!snap.onGround && snap.fallDistance > 0.0f &&
                           !snap.climbing && !snap.inWater && !snap.hasVehicle && !snap.blind);

        jobject myTeam = m_Entity_getTeam ? env->CallObjectMethod(player, m_Entity_getTeam) : nullptr;
        ClearEx(env);

        // Crosshair entity target
        jobject hit = f_MC_crosshairTarget ? env->GetObjectField(mc, f_MC_crosshairTarget) : nullptr;
        if (hit && !env->ExceptionCheck() && env->IsInstanceOf(hit, c_EntityHitResult) && m_EntityHit_getEntity) {
            jobject target = env->CallObjectMethod(hit, m_EntityHit_getEntity);
            if (target && !env->ExceptionCheck() && !env->IsSameObject(target, player)) {
                handles.targetEntity = target;
                auto& ct = snap.crosshair;
                ct.valid = true;
                ct.isPlayer  = (c_PlayerEntity && env->IsInstanceOf(target, c_PlayerEntity));
                ct.isMob     = (c_MobEntity && env->IsInstanceOf(target, c_MobEntity));
                ct.isHostile = (c_HostileEntity && env->IsInstanceOf(target, c_HostileEntity));
                ct.alive     = (env->CallBooleanMethod(target, m_Entity_isAlive) == JNI_TRUE);
                ct.spectator = (env->CallBooleanMethod(target, m_Entity_isSpectator) == JNI_TRUE);
                ct.invisible = (env->CallBooleanMethod(target, m_Entity_isInvisible) == JNI_TRUE);
                ct.distance  = env->CallFloatMethod(player, m_Entity_distanceTo, target);
                if (c_LivingEntity && env->IsInstanceOf(target, c_LivingEntity)) {
                    ct.health    = env->CallFloatMethod(target, m_Living_getHealth);
                    ct.maxHealth = env->CallFloatMethod(target, m_Living_getMaxHealth);
                    ct.hurtTime  = f_Living_hurtTime ? env->GetIntField(target, f_Living_hurtTime) : 0;
                }
                if (ct.isPlayer && m_Player_isCreative) {
                    ct.creative = (env->CallBooleanMethod(target, m_Player_isCreative) == JNI_TRUE);
                }
                if (myTeam && m_Entity_getTeam) {
                    jobject otherTeam = env->CallObjectMethod(target, m_Entity_getTeam);
                    if (otherTeam && !env->ExceptionCheck()) {
                        ct.sameTeam = (env->CallBooleanMethod(myTeam, m_Object_equals, otherTeam) == JNI_TRUE);
                    }
                }
                ct.name = ReadEntityName(env, target, ct.isPlayer);
            }
        }
        ClearEx(env);

        // Players in world (for ESP / Tracers / AimAssist)
        if (f_World_players) {
            jobject playersList = env->GetObjectField(world, f_World_players);
            if (playersList && !env->ExceptionCheck()) {
                jint count = env->CallIntMethod(playersList, m_List_size);
                count = (std::min)(count, static_cast<jint>(64));
                for (jint i = 0; i < count; ++i) {
                    jobject p = env->CallObjectMethod(playersList, m_List_get, i);
                    if (!p || env->IsSameObject(p, player)) {
                        if (p) env->DeleteLocalRef(p);
                        continue;
                    }
                    PlayerSnapshot ps;
                    ps.alive     = (env->CallBooleanMethod(p, m_Entity_isAlive) == JNI_TRUE);
                    ps.spectator = (env->CallBooleanMethod(p, m_Entity_isSpectator) == JNI_TRUE);
                    ps.invisible = (env->CallBooleanMethod(p, m_Entity_isInvisible) == JNI_TRUE);
                    if (!ps.alive || ps.spectator) {
                        env->DeleteLocalRef(p);
                        continue;
                    }
                    double x = env->CallDoubleMethod(p, m_Entity_getX);
                    double y = env->CallDoubleMethod(p, m_Entity_getY);
                    double z = env->CallDoubleMethod(p, m_Entity_getZ);
                    double px = f_Entity_prevX ? env->GetDoubleField(p, f_Entity_prevX) : x;
                    double py = f_Entity_prevY ? env->GetDoubleField(p, f_Entity_prevY) : y;
                    double pz = f_Entity_prevZ ? env->GetDoubleField(p, f_Entity_prevZ) : z;
                    double ix = px + (x - px) * snap.tickDelta;
                    double iy = py + (y - py) * snap.tickDelta;
                    double iz = pz + (z - pz) * snap.tickDelta;
                    ps.pos = { ix, iy, iz };

                    if (f_Entity_boundingBox && f_Box_minX) {
                        jobject boxObj = env->GetObjectField(p, f_Entity_boundingBox);
                        if (boxObj && !env->ExceptionCheck()) {
                            double dx = ix - x, dy = iy - y, dz = iz - z;
                            ps.box.minX = env->GetDoubleField(boxObj, f_Box_minX) + dx;
                            ps.box.minY = env->GetDoubleField(boxObj, f_Box_minY) + dy;
                            ps.box.minZ = env->GetDoubleField(boxObj, f_Box_minZ) + dz;
                            ps.box.maxX = env->GetDoubleField(boxObj, f_Box_maxX) + dx;
                            ps.box.maxY = env->GetDoubleField(boxObj, f_Box_maxY) + dy;
                            ps.box.maxZ = env->GetDoubleField(boxObj, f_Box_maxZ) + dz;
                            env->DeleteLocalRef(boxObj);
                        }
                    } else {
                        ps.box = { ix - 0.3, iy, iz - 0.3, ix + 0.3, iy + 1.8, iz + 0.3 };
                    }

                    ps.distance  = env->CallFloatMethod(player, m_Entity_distanceTo, p);
                    ps.health    = env->CallFloatMethod(p, m_Living_getHealth);
                    ps.maxHealth = env->CallFloatMethod(p, m_Living_getMaxHealth);
                    ps.hurtTime  = f_Living_hurtTime ? env->GetIntField(p, f_Living_hurtTime) : 0;
                    if (m_Player_isCreative) {
                        ps.creative = (env->CallBooleanMethod(p, m_Player_isCreative) == JNI_TRUE);
                    }
                    if (myTeam && m_Entity_getTeam) {
                        jobject otherTeam = env->CallObjectMethod(p, m_Entity_getTeam);
                        if (otherTeam && !env->ExceptionCheck()) {
                            ps.sameTeam = (env->CallBooleanMethod(myTeam, m_Object_equals, otherTeam) == JNI_TRUE);
                            env->DeleteLocalRef(otherTeam);
                        }
                    }
                    ps.name = ReadEntityName(env, p, true);
                    snap.players.push_back(std::move(ps));
                    env->DeleteLocalRef(p);
                }
            }
            ClearEx(env);
        }
    }

    activeHandles_ = handles;
    fn(snap, handles);
    activeHandles_ = {};
    curEnv_ = nullptr;
}

void JvmBridge::AttackEntity(jobject target, bool resetSprintForCrit) {
    JNIEnv* env = curEnv_;
    if (!env || !target || !activeHandles_.player || !activeHandles_.interactionManager) return;

    if (resetSprintForCrit) {
        env->CallVoidMethod(activeHandles_.player, m_Entity_setSprinting, JNI_FALSE);
        if (m_ClientPlayer_sendSprint) {
            env->CallVoidMethod(activeHandles_.player, m_ClientPlayer_sendSprint);
        }
        ClearEx(env);
    }

    env->CallVoidMethod(activeHandles_.interactionManager, m_Interaction_attackEntity, activeHandles_.player, target);
    ClearEx(env);

    if (m_Living_swingHand && o_MainHand) {
        env->CallVoidMethod(activeHandles_.player, m_Living_swingHand, o_MainHand);
        ClearEx(env);
    }
}

void JvmBridge::JumpPlayer() {
    JNIEnv* env = curEnv_;
    if (!env || !activeHandles_.player || !m_Living_jump) return;
    env->CallVoidMethod(activeHandles_.player, m_Living_jump);
    ClearEx(env);
}

void JvmBridge::SetPlayerRotation(float yaw, float pitch) {
    JNIEnv* env = curEnv_;
    if (!env || !activeHandles_.player || !m_Entity_setYaw || !m_Entity_setPitch) return;
    env->CallVoidMethod(activeHandles_.player, m_Entity_setYaw, yaw);
    env->CallVoidMethod(activeHandles_.player, m_Entity_setPitch, pitch);
    ClearEx(env);
}

void JvmBridge::SetPlayerSprinting(bool sprinting, bool sendPacket) {
    JNIEnv* env = curEnv_;
    if (!env || !activeHandles_.player || !m_Entity_setSprinting) return;
    env->CallVoidMethod(activeHandles_.player, m_Entity_setSprinting, sprinting ? JNI_TRUE : JNI_FALSE);
    if (sendPacket && m_ClientPlayer_sendSprint) {
        env->CallVoidMethod(activeHandles_.player, m_ClientPlayer_sendSprint);
    }
    ClearEx(env);
}

void JvmBridge::SetSprintKeyPressed(bool pressed) {
    JNIEnv* env = curEnv_;
    if (!env || !activeHandles_.options || !f_Options_sprintKey || !m_KeyBinding_setPressed) return;
    jobject key = env->GetObjectField(activeHandles_.options, f_Options_sprintKey);
    if (key && !env->ExceptionCheck()) {
        env->CallVoidMethod(key, m_KeyBinding_setPressed, pressed ? JNI_TRUE : JNI_FALSE);
    }
    ClearEx(env);
}

void JvmBridge::SetUseKeyPressed(bool pressed) {
    JNIEnv* env = curEnv_;
    if (!env || !activeHandles_.options || !f_Options_useKey || !m_KeyBinding_setPressed) return;
    jobject key = env->GetObjectField(activeHandles_.options, f_Options_useKey);
    if (key && !env->ExceptionCheck()) {
        env->CallVoidMethod(key, m_KeyBinding_setPressed, pressed ? JNI_TRUE : JNI_FALSE);
    }
    ClearEx(env);
}

void JvmBridge::DoItemUse() {
    JNIEnv* env = curEnv_;
    if (!env || !activeHandles_.mc || !m_MC_doItemUse) return;
    env->CallVoidMethod(activeHandles_.mc, m_MC_doItemUse);
    ClearEx(env);
}

int JvmBridge::FindFoodHotbarSlot() {
    JNIEnv* env = curEnv_;
    if (!env || !activeHandles_.player || !m_Player_getInventory || !m_Inv_getStack) return -1;
    jobject inv = env->CallObjectMethod(activeHandles_.player, m_Player_getInventory);
    if (!inv || env->ExceptionCheck()) { ClearEx(env); return -1; }
    int found = -1;
    for (int i = 0; i < 9; ++i) {
        jobject stack = env->CallObjectMethod(inv, m_Inv_getStack, static_cast<jint>(i));
        if (stack && !env->ExceptionCheck()) {
            if (IsFoodStack(env, stack)) {
                found = i;
                env->DeleteLocalRef(stack);
                break;
            }
            env->DeleteLocalRef(stack);
        }
    }
    ClearEx(env);
    return found;
}

void JvmBridge::SelectHotbarSlot(int slot) {
    JNIEnv* env = curEnv_;
    if (!env || !activeHandles_.player || !m_Player_getInventory || !f_Inv_selectedSlot) return;
    if (slot < 0 || slot > 8) return;
    jobject inv = env->CallObjectMethod(activeHandles_.player, m_Player_getInventory);
    if (inv && !env->ExceptionCheck()) {
        env->SetIntField(inv, f_Inv_selectedSlot, static_cast<jint>(slot));
    }
    ClearEx(env);
}

void JvmBridge::SetGammaOverride(bool enable) {
    JNIEnv* env = curEnv_;
    if (!env || !activeHandles_.options || !f_Options_gamma || !f_SimpleOption_value) return;
    jobject gammaOpt = env->GetObjectField(activeHandles_.options, f_Options_gamma);
    if (!gammaOpt || env->ExceptionCheck()) { ClearEx(env); return; }

    if (enable && !gammaOverridden_) {
        jobject cur = env->GetObjectField(gammaOpt, f_SimpleOption_value);
        if (cur && !env->ExceptionCheck()) {
            if (savedGammaObj_) env->DeleteGlobalRef(savedGammaObj_);
            savedGammaObj_ = env->NewGlobalRef(cur);
            hasSavedGamma_ = true;
        }
        jobject fullVal = env->CallStaticObjectMethod(c_Double, m_Double_valueOf, 16.0);
        if (fullVal && !env->ExceptionCheck()) {
            env->SetObjectField(gammaOpt, f_SimpleOption_value, fullVal);
            gammaOverridden_ = true;
        }
        ClearEx(env);
    } else if (!enable && gammaOverridden_) {
        if (hasSavedGamma_ && savedGammaObj_) {
            env->SetObjectField(gammaOpt, f_SimpleOption_value, savedGammaObj_);
        }
        gammaOverridden_ = false;
        ClearEx(env);
    }
}

void JvmBridge::UnlockCursor() {
    JNIEnv* env = curEnv_;
    if (!env || !activeHandles_.mouse || !m_Mouse_unlock) return;
    env->CallVoidMethod(activeHandles_.mouse, m_Mouse_unlock);
    ClearEx(env);
}

void JvmBridge::LockCursor() {
    JNIEnv* env = curEnv_;
    if (!env || !activeHandles_.mouse || !m_Mouse_lock) return;
    env->CallVoidMethod(activeHandles_.mouse, m_Mouse_lock);
    ClearEx(env);
}

} // namespace meow
