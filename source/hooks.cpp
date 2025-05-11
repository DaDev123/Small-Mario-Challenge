#include "sead/prim/seadSafeString.h"
#include "main.hpp"
#include "al/util.hpp"
#include "game/Player/HackCap/CapFunction.h"
#include "sead/prim/seadSafeString.hpp"
#include "al/string/StringTmp.h"

void sensorHook(al::LiveActor *actor, al::ActorInitInfo const &initInfo, char const *sensorName, uint typeEnum, float radius, ushort maxCount, sead::Vector3f const& position) {
    sead::Vector3f newPos = sead::Vector3f(position);
    if(position.y > 0)
        newPos.y = position.y * 0.3f;

    al::addHitSensor(actor, initInfo, sensorName, typeEnum, radius, maxCount, newPos);
}

float followDistHook() {
            return 270.f;
}

void playerSizeHook(PlayerActorHakoniwa* player){
  al::setScaleAll(player, 0.3f);
}


void effectHook(al::ActionEffectCtrl* effectController, char const* effectName) {
    {
        if(al::isEqualString(effectName, "RollingStart") || al::isEqualString(effectName, "Rolling") || al::isEqualString(effectName, "RollingStandUp") || al::isEqualString(effectName, "Jump") || al::isEqualString(effectName, "LandDownFall")|| al::isEqualString(effectName, "SpinCapStart") || al::isEqualString(effectName, "FlyingWaitR") || al::isEqualString(effectName, "StayR")|| al::isEqualString(effectName, "SpinGroundR")|| al::isEqualString(effectName, "StartSpinJumpR")|| al::isEqualString(effectName, "SpinJumpDownFallR")|| al::isEqualString(effectName, "Move")|| al::isEqualString(effectName, "Brake")) {
            al::tryDeleteEffect(effectController->mEffectKeeper, effectName);
            return;
        }
    }

    effectController->startAction(effectName);
    

}


void spinFlowerHook(al::LiveActor* actor, float velocity) {
    al::addVelocityToGravity(actor, velocity * scale);
}

float fpHook() {
    return 3000.0f * scale;
}

float fpScaleHook() {
            return 2.94f;
}

// Cap Throw Distance
void capVelScaleHook(al::LiveActor *hackCap, sead::Vector3f const& addition) {
    sead::Vector3f newVelocity = sead::Vector3f(addition.x * 0.35f, addition.y * 0.35f, addition.z * 0.35f);
    al::setVelocity(hackCap, newVelocity);
}

const char* offsetOverideHook(al::ByamlIter const& iter, char const* key) {
             return "Y0.5m";
             return al::tryGetByamlKeyStringOrNULL(iter, key);
    }