#pragma once

// Verified against FabricMC/intermediary mappings/1.21.4.tiny and FabricMC/yarn 1.21.4
namespace meow::mc {

namespace cls {
inline constexpr const char* MinecraftClient       = "net.minecraft.class_310";
inline constexpr const char* ClientPlayerEntity    = "net.minecraft.class_746";
inline constexpr const char* ClientWorld           = "net.minecraft.class_638";
inline constexpr const char* InteractionManager    = "net.minecraft.class_636";
inline constexpr const char* Mouse                 = "net.minecraft.class_312";
inline constexpr const char* GameOptions           = "net.minecraft.class_315";
inline constexpr const char* SimpleOption          = "net.minecraft.class_7172";
inline constexpr const char* KeyBinding            = "net.minecraft.class_304";
inline constexpr const char* GameRenderer          = "net.minecraft.class_757";
inline constexpr const char* Camera                = "net.minecraft.class_4184";
inline constexpr const char* Window                = "net.minecraft.class_1041";
inline constexpr const char* RenderTickCounter     = "net.minecraft.class_9779";

inline constexpr const char* Entity                = "net.minecraft.class_1297";
inline constexpr const char* LivingEntity          = "net.minecraft.class_1309";
inline constexpr const char* PlayerEntity          = "net.minecraft.class_1657";
inline constexpr const char* MobEntity             = "net.minecraft.class_1308";
inline constexpr const char* HostileEntity         = "net.minecraft.class_1588";
inline constexpr const char* FishingBobberEntity   = "net.minecraft.class_1536";

inline constexpr const char* EntityHitResult       = "net.minecraft.class_3966";
inline constexpr const char* Vec3d                 = "net.minecraft.class_243";
inline constexpr const char* Box                   = "net.minecraft.class_238";
inline constexpr const char* Hand                  = "net.minecraft.class_1268";
inline constexpr const char* Nameable              = "net.minecraft.class_1275";
inline constexpr const char* Text                  = "net.minecraft.class_2561";

inline constexpr const char* HungerManager         = "net.minecraft.class_1702";
inline constexpr const char* PlayerInventory       = "net.minecraft.class_1661";
inline constexpr const char* Inventory             = "net.minecraft.class_1263";
inline constexpr const char* ItemStack             = "net.minecraft.class_1799";
inline constexpr const char* SwordItem             = "net.minecraft.class_1829";
inline constexpr const char* AxeItem               = "net.minecraft.class_1743";
inline constexpr const char* MaceItem              = "net.minecraft.class_9362";
inline constexpr const char* TridentItem           = "net.minecraft.class_1835";

inline constexpr const char* Registries            = "net.minecraft.class_7923";
inline constexpr const char* Registry              = "net.minecraft.class_2378";
inline constexpr const char* Identifier            = "net.minecraft.class_2960";
inline constexpr const char* GameProfile           = "com.mojang.authlib.GameProfile";
} // namespace cls

namespace mc_client {
inline constexpr const char* getInstance           = "method_1551";
inline constexpr const char* getInstanceSig        = "()Lnet/minecraft/class_310;";
inline constexpr const char* player                = "field_1724";
inline constexpr const char* playerSig             = "Lnet/minecraft/class_746;";
inline constexpr const char* world                 = "field_1687";
inline constexpr const char* worldSig              = "Lnet/minecraft/class_638;";
inline constexpr const char* interactionManager    = "field_1761";
inline constexpr const char* interactionManagerSig = "Lnet/minecraft/class_636;";
inline constexpr const char* crosshairTarget       = "field_1765";
inline constexpr const char* crosshairTargetSig    = "Lnet/minecraft/class_239;";
inline constexpr const char* currentScreen         = "field_1755";
inline constexpr const char* currentScreenSig      = "Lnet/minecraft/class_437;";
inline constexpr const char* options               = "field_1690";
inline constexpr const char* optionsSig            = "Lnet/minecraft/class_315;";
inline constexpr const char* mouse                 = "field_1729";
inline constexpr const char* mouseSig              = "Lnet/minecraft/class_312;";
inline constexpr const char* gameRenderer          = "field_1773";
inline constexpr const char* gameRendererSig       = "Lnet/minecraft/class_757;";
inline constexpr const char* attackCooldown        = "field_1771";
inline constexpr const char* doItemUse             = "method_1583";
inline constexpr const char* getCurrentFps         = "method_47599";
inline constexpr const char* getRenderTickCounter  = "method_61966";
inline constexpr const char* getRenderTickCounterSig = "()Lnet/minecraft/class_9779;";
inline constexpr const char* getWindow             = "method_22683";
inline constexpr const char* getWindowSig          = "()Lnet/minecraft/class_1041;";
} // namespace mc_client

namespace tick_counter {
inline constexpr const char* getTickDelta          = "method_60637"; // (Z)F
}

namespace window {
inline constexpr const char* getFramebufferWidth   = "method_4489";
inline constexpr const char* getFramebufferHeight  = "method_4506";
}

namespace mouse {
inline constexpr const char* unlockCursor          = "method_1610";
inline constexpr const char* lockCursor            = "method_1612";
inline constexpr const char* isCursorLocked        = "method_1613";
}

namespace options {
inline constexpr const char* gamma                 = "field_1840";
inline constexpr const char* gammaSig              = "Lnet/minecraft/class_7172;";
inline constexpr const char* sprintKey             = "field_1867";
inline constexpr const char* attackKey             = "field_1886";
inline constexpr const char* forwardKey            = "field_1894";
inline constexpr const char* jumpKey               = "field_1903";
inline constexpr const char* useKey                = "field_1904";
inline constexpr const char* keySig                = "Lnet/minecraft/class_304;";
}

namespace simple_option {
inline constexpr const char* value                 = "field_37868";
inline constexpr const char* valueSig              = "Ljava/lang/Object;";
}

namespace key_binding {
inline constexpr const char* setPressed            = "method_23481";
inline constexpr const char* isPressed             = "method_1434";
}

namespace game_renderer {
inline constexpr const char* getCamera             = "method_19418";
inline constexpr const char* getCameraSig          = "()Lnet/minecraft/class_4184;";
inline constexpr const char* getFov                = "method_3196";
inline constexpr const char* getFovSig             = "(Lnet/minecraft/class_4184;FZ)F";
}

namespace camera {
inline constexpr const char* getPos                = "method_19326";
inline constexpr const char* getPosSig             = "()Lnet/minecraft/class_243;";
inline constexpr const char* getPitch              = "method_19329";
inline constexpr const char* getYaw                = "method_19330";
}

namespace vec3d {
inline constexpr const char* x                     = "field_1352";
inline constexpr const char* y                     = "field_1351";
inline constexpr const char* z                     = "field_1350";
}

namespace box {
inline constexpr const char* minX                  = "field_1323";
inline constexpr const char* minY                  = "field_1322";
inline constexpr const char* minZ                  = "field_1321";
inline constexpr const char* maxX                  = "field_1320";
inline constexpr const char* maxY                  = "field_1325";
inline constexpr const char* maxZ                  = "field_1324";
}

namespace entity_hit {
inline constexpr const char* getEntity             = "method_17782";
inline constexpr const char* getEntitySig          = "()Lnet/minecraft/class_1297;";
}

namespace interaction {
inline constexpr const char* attackEntity          = "method_2918";
inline constexpr const char* attackEntitySig       = "(Lnet/minecraft/class_1657;Lnet/minecraft/class_1297;)V";
}

namespace world {
inline constexpr const char* players               = "field_18226";
inline constexpr const char* playersSig            = "Ljava/util/List;";
inline constexpr const char* getEntities           = "method_18112";
}

namespace entity {
inline constexpr const char* fallDistance          = "field_6017";
inline constexpr const char* prevX                 = "field_6014";
inline constexpr const char* prevY                 = "field_6036";
inline constexpr const char* prevZ                 = "field_5969";
inline constexpr const char* boundingBox           = "field_6005";
inline constexpr const char* boundingBoxSig        = "Lnet/minecraft/class_238;";
inline constexpr const char* getX                  = "method_23317";
inline constexpr const char* getY                  = "method_23318";
inline constexpr const char* getZ                  = "method_23321";
inline constexpr const char* getYaw                = "method_36454";
inline constexpr const char* getPitch              = "method_36455";
inline constexpr const char* setYaw                = "method_36456";
inline constexpr const char* setPitch              = "method_36457";
inline constexpr const char* isOnGround            = "method_24828";
inline constexpr const char* isSprinting           = "method_5624";
inline constexpr const char* setSprinting          = "method_5728";
inline constexpr const char* isTouchingWater       = "method_5799";
inline constexpr const char* hasVehicle            = "method_5765";
inline constexpr const char* isAlive               = "method_5805";
inline constexpr const char* isSpectator           = "method_7325";
inline constexpr const char* isInvisible           = "method_5767";
inline constexpr const char* distanceTo            = "method_5739";
inline constexpr const char* distanceToSig         = "(Lnet/minecraft/class_1297;)F";
inline constexpr const char* getStandingEyeHeight  = "method_5751";
inline constexpr const char* getHeight             = "method_17682";
inline constexpr const char* getVelocity           = "method_18798";
inline constexpr const char* getVelocitySig        = "()Lnet/minecraft/class_243;";
inline constexpr const char* getScoreboardTeam     = "method_5781";
inline constexpr const char* getScoreboardTeamSig  = "()Lnet/minecraft/class_268;";
inline constexpr const char* getName               = "method_5477"; // via Nameable
inline constexpr const char* getNameSig            = "()Lnet/minecraft/class_2561;";
}

namespace living {
inline constexpr const char* hurtTime              = "field_6235";
inline constexpr const char* getHealth             = "method_6032";
inline constexpr const char* getMaxHealth          = "method_6063";
inline constexpr const char* jump                  = "method_6043";
inline constexpr const char* isClimbing            = "method_6101";
inline constexpr const char* hasStatusEffect       = "method_6059";
inline constexpr const char* hasStatusEffectSig    = "(Lnet/minecraft/class_6880;)Z";
inline constexpr const char* isUsingItem           = "method_6115";
inline constexpr const char* isBlocking            = "method_6039";
inline constexpr const char* getMainHandStack      = "method_6047";
inline constexpr const char* getMainHandStackSig   = "()Lnet/minecraft/class_1799;";
inline constexpr const char* swingHand             = "method_6104";
inline constexpr const char* swingHandSig          = "(Lnet/minecraft/class_1268;)V";
}

namespace player {
inline constexpr const char* fishHook              = "field_7513";
inline constexpr const char* fishHookSig           = "Lnet/minecraft/class_1536;";
inline constexpr const char* getAttackCooldown     = "method_7261"; // (F)F
inline constexpr const char* isCreative            = "method_7337";
inline constexpr const char* getHungerManager      = "method_7344";
inline constexpr const char* getHungerManagerSig   = "()Lnet/minecraft/class_1702;";
inline constexpr const char* getInventory          = "method_31548";
inline constexpr const char* getInventorySig       = "()Lnet/minecraft/class_1661;";
inline constexpr const char* getGameProfile        = "method_7334";
inline constexpr const char* getGameProfileSig     = "()Lcom/mojang/authlib/GameProfile;";
inline constexpr const char* sendSprintingPacket   = "method_46742";
}

namespace hunger {
inline constexpr const char* getFoodLevel          = "method_7586";
}

namespace inventory {
inline constexpr const char* selectedSlot          = "field_7545";
inline constexpr const char* getStack              = "method_5438";
inline constexpr const char* getStackSig           = "(I)Lnet/minecraft/class_1799;";
}

namespace bobber {
inline constexpr const char* caughtFish            = "field_23232";
}

namespace item_stack {
inline constexpr const char* getItem               = "method_7909";
inline constexpr const char* getItemSig            = "()Lnet/minecraft/class_1792;";
inline constexpr const char* isEmpty               = "method_7960";
inline constexpr const char* contains              = "method_57826";
inline constexpr const char* containsSig           = "(Lnet/minecraft/class_9331;)Z";
}

namespace hand {
inline constexpr const char* MAIN_HAND             = "field_5808";
inline constexpr const char* sig                   = "Lnet/minecraft/class_1268;";
}

namespace registries {
inline constexpr const char* STATUS_EFFECT         = "field_41174";
inline constexpr const char* DATA_COMPONENT_TYPE   = "field_49658";
inline constexpr const char* registrySig           = "Lnet/minecraft/class_2378;";
inline constexpr const char* idOf                  = "method_60654";
inline constexpr const char* idOfSig               = "(Ljava/lang/String;)Lnet/minecraft/class_2960;";
inline constexpr const char* regGet                = "method_63535";
inline constexpr const char* regGetSig             = "(Lnet/minecraft/class_2960;)Ljava/lang/Object;";
inline constexpr const char* regGetEntry           = "method_10223";
inline constexpr const char* regGetEntrySig        = "(Lnet/minecraft/class_2960;)Ljava/util/Optional;";
}

} // namespace meow::mc
