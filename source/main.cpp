#include "main.hpp"
#include "al/area/AreaObj.h"
#include "al/area/AreaObjGroup.h"
#include "al/camera/CameraDirector.h"
#include "al/camera/CameraPoser.h"
#include "al/util.hpp"
#include "al/util/LiveActorUtil.h"
#include "al/util/StringUtil.h"
#include "cameras/CameraPoserCustom.h"
#include "debugMenu.hpp"
#include "game/Player/PlayerHitPointData.h"
#include "game/Player/PlayerDamageKeeper.h"
#include "game/GameData/GameDataFunction.h"
#include "game/Layouts/CoinCounter.h"

// Player scaling
const float smallScale = 0.3f;
const float bigScale = 4.0f;

// Death tracking
static int deathCount = 0;
static bool wasPlayerDead = false;
static int rapidIncreaseTimer = 0;

// Menu and display flags
static bool showMenu = false;
static bool isInGame = false;
static bool rainbowEnabled = false;
static bool showBackground = false;

// Rainbow effect
static float rainbowHue = 0.0f;
static const float rainbowSpeed = 0.02f;

// Warp point list
DebugWarpPoint warpPoints[40];
int listCount = 0;
int curWarpPoint = 0;

// Utility: HSV to RGB conversion (for rainbow effect)
sead::Color4f hsvToRgb(float h, float s, float v) {
    float hi = fmodf(h * 6.0f, 6.0f);
    float f = h * 6.0f - floorf(h * 6.0f);
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    sead::Color4f c;

    if (hi < 1.0f)      c = {v, t, p, 0.f};
    else if (hi < 2.0f) c = {q, v, p, 0.f};
    else if (hi < 3.0f) c = {p, v, t, 0.f};
    else if (hi < 4.0f) c = {p, q, v, 0.f};
    else if (hi < 5.0f) c = {t, p, v, 0.f};
    else                c = {v, p, q, 0.f};

    return c;
}

// Draws semi-transparent background box
void drawBackground(agl::DrawContext *context) {
    if (!showBackground) return;

    sead::Vector3<float> p1 = {-1.0f, -0.3f, 0.0f};
    sead::Vector3<float> p2 = {-0.44f, -0.3f, 0.0f};
    sead::Vector3<float> p3 = {-1.0f, -1.0f, 0.0f};
    sead::Vector3<float> p4 = {-0.44f, -1.0f, 0.0f};

    sead::Color4f c = rainbowEnabled
        ? hsvToRgb(rainbowHue, 0.5f, 0.2f)
        : sead::Color4f(0.1f, 0.1f, 0.15f, 0.9f);
    c.a = 0.9f;

    agl::utl::DevTools::beginDrawImm(context, sead::Matrix34<float>::ident, sead::Matrix44<float>::ident);
    agl::utl::DevTools::drawTriangleImm(context, p1, p2, p3, c);
    agl::utl::DevTools::drawTriangleImm(context, p3, p4, p2, c);
}

// Reset death counter
void resetDeathCounter() {
    deathCount = 0;
    gLogger->LOG("Death counter reset.\n");
}

// New function to handle death counter functionality
void updateDeathCounter(PlayerActorHakoniwa* player, StageScene* stageScene) {
    if (!player) return;

    auto *hitData = stageScene->mHolder.mData->mGameDataFile->getPlayerHitPointData();
    bool isDead = hitData->getCurrent() <= 0;

    // Check if player died and increment counter
    if (isDead && !wasPlayerDead) {
        deathCount++;
        gLogger->LOG("Player died. Total deaths: %d\n", deathCount);
    }
    wasPlayerDead = isDead;

    if (!isDead) hitData->getMaxCurrently();

    // Update coin counter display
    if (auto *layout = stageScene->stageSceneLayout; layout && layout->coinCounter) {
        al::setPaneStringFormat(layout->coinCounter, "TxtDebug", "%04d", deathCount);
        al::setPaneLocalRotate(layout->coinCounter, "TxtDebug", {0.f, 0.f, 0.3f});

        if (deathCount == 69) {
            al::setPaneStringFormat(layout->coinCounter, "TxtEvent", "Nice");
            al::showPane(layout->coinCounter, "TxtEvent");
            al::emitEffect(player, "Invincible", al::getTransPtr(player));
            hitData->getEventHealth();
        } else {
            al::hidePane(layout->coinCounter, "TxtEvent");
        }
    }

    // Input controls for death counter
    if (al::isPadTriggerRight(-1)) deathCount++;
    if (al::isPadTriggerLeft(-1) && deathCount > 0) deathCount--;
    if (al::isPadTriggerDown(-1) && al::isPadHoldL(-1)) resetDeathCounter();

    // Rapid increase/decrease with L button held
    if (al::isPadHoldL(-1)) {
        if (al::isPadHoldRight(-1)) {
            if (++rapidIncreaseTimer >= 5) {
                deathCount++;
                rapidIncreaseTimer = 0;
            }
        } else if (al::isPadHoldLeft(-1) && deathCount > 0) {
            if (++rapidIncreaseTimer >= 5) {
                deathCount--;
                rapidIncreaseTimer = 0;
            }
        } else {
            rapidIncreaseTimer = 0;
        }
    }

    // Forced damage check
    if (player->mPlayerAnimator &&
        (player->mPlayerAnimator->isAnim("NoDamageDown") || player->mPlayerAnimator->isAnim("LandStiffen")) &&
        hitData->getCurrent() > 0) {
        
        hitData->kill();
        gLogger->LOG("Forced damage triggered.\n");
        if (hitData->getCurrent() == 0) player->mPlayerAnimator->startAnimDead();
    }
}

// Populate warpPoints array from DebugList
al::StageInfo *initDebugListHook(const al::Scene *curScene) {
    al::StageInfo *info = al::getStageInfoMap(curScene, 0);
    al::PlacementInfo rootInfo;

    al::tryGetPlacementInfoAndCount(&rootInfo, &listCount, info, "DebugList");

    for (size_t i = 0; i < listCount; i++) {
        al::PlacementInfo objInfo;
        al::getPlacementInfoByIndex(&objInfo, rootInfo, i);
        const char *displayName = "";
        al::tryGetDisplayName(&displayName, objInfo);
        strcpy(warpPoints[i].pointName, displayName);
        al::tryGetTrans(&warpPoints[i].warpPos, objInfo);
    }

    return info;
}

// Main debug overlay drawing
void drawMainHook(HakoniwaSequence *seq, sead::Viewport *viewport, sead::DrawContext *drawCtx) {
    if (rainbowEnabled) {
        rainbowHue += rainbowSpeed;
        if (rainbowHue >= 1.0f) rainbowHue = 0.0f;
    }

    if (!showMenu) {
        al::executeDraw(seq->mLytKit, "２Ｄバック（メイン画面）");
        return;
    }

    int dispWidth = al::getLayoutDisplayWidth();
    int dispHeight = al::getLayoutDisplayHeight();

    gTextWriter->mViewport = viewport;
    sead::Color4f textColor = rainbowEnabled
        ? hsvToRgb(fmodf(rainbowHue + 0.5f, 1.0f), 0.7f, 1.0f)
        : sead::Color4f(1.f, 1.f, 1.f, 0.9f);
    textColor.a = 0.9f;
    gTextWriter->mColor = textColor;

    if (seq->curScene && isInGame) {
        drawBackground((agl::DrawContext *)drawCtx);
        gTextWriter->beginDraw();
        gTextWriter->setCursorFromTopLeft(sead::Vector2f(10.f, dispHeight / 2.f));
        gTextWriter->setScaleFromFontHeight(14.f);

        sead::Color4f boldColor = textColor;
        boldColor.a = 1.0f;
        gTextWriter->mColor = boldColor;

        gTextWriter->printf("--------------- Info ---------------\n\n");
        gTextWriter->printf("Player Scale: %.2f\n", smallScale);
        gTextWriter->printf("Deaths: %d\n", deathCount);

        gTextWriter->printf("\n--------------- Keybinds ---------------\n\n");
        gTextWriter->printf("D-Pad Up: Toggle Menu\n");
        gTextWriter->printf("D-Pad Right: Add Death\n");
        gTextWriter->printf("D-Pad Left: Remove Death\n");
        gTextWriter->printf("L + D-Pad Down: Reset Deaths\n");
        gTextWriter->printf("Hold L + Right/Left: Rapid Add/Remove\n");

        gTextWriter->printf("\n--------------- More Info ---------------\n\n");
        gTextWriter->printf("Mod By: Secret Dev\n");

        gTextWriter->endDraw();
        isInGame = false;
    }

    al::executeDraw(seq->mLytKit, "２Ｄバック（メイン画面）");
}

// Called every frame in stage scene
void stageSceneHook() {
    StageScene *stageScene;
    __asm("MOV %[result], X0" : [result] "=r"(stageScene));
    isInGame = true;

    auto *pHolder = al::getScenePlayerHolder(stageScene);
    auto *player = (PlayerActorHakoniwa*)al::tryGetPlayerActor(pHolder, 0);
    if (!player) return;

    // Update death counter
    updateDeathCounter(player, stageScene);

    // Scaling
    auto *cap = player->mHackCap;
    if (al::getScale(player)->x != smallScale) al::setScaleAll(player, smallScale);
    if (al::getScale(cap)->x != smallScale) al::setScaleAll(cap, smallScale);
    if (auto *hack = player->mHackKeeper->currentHackActor; hack && al::getScale(hack)->x != smallScale) {
        al::setScaleAll(hack, smallScale);
    }

    if (al::isPadTriggerUp(-1)) showMenu = !showMenu;
}

bool hakoniwaSequenceHook(HakoniwaSequence *sequence) {
    auto *stageScene = (StageScene*)sequence->curScene;
    isInGame = !stageScene->isPause();
    return al::isFirstStep(sequence);
}

// Debug print hook
void seadPrintHook(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    gLogger->LOG(fmt, args);
    va_end(args);
}

// Low-level ASM hooks
void stageInitHook(StageScene *initStageScene, al::SceneInitInfo *sceneInitInfo) {
    __asm("MOV X19, X0");
    __asm("LDR X24, [X1, #0x18]");
    __asm("MOV X1, X24");
}

ulong threadInit() {
    __asm("STR X21, [X19,#0x208]");
    return 0x20;
}