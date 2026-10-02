#pragma once
#include <Geode/Geode.hpp>
#include <array>
#include <memory>
#include <type_traits>
#include <utility>

namespace NXR::Practice {

#define NXR_PLAYER_FIELDS(X) \
    X(m_wasTeleported) X(m_fixGravityBug) X(m_reverseSync) X(m_yVelocityBeforeSlope) \
    X(m_dashX) X(m_dashY) X(m_dashAngle) X(m_dashStartTime) \
    X(m_dashRing) X(m_slopeStartTime) X(m_justPlacedStreak) X(m_maybeLastGroundObject) \
    X(m_lastCollisionBottom) X(m_lastCollisionTop) X(m_lastCollisionLeft) X(m_lastCollisionRight) \
    X(m_unk50C) X(m_unk510) X(m_currentSlope2) X(m_preLastGroundObject) \
    X(m_slopeAngle) X(m_slopeSlidingMaybeRotated) X(m_quickCheckpointMode) X(m_collidedObject) \
    X(m_lastGroundObject) X(m_collidingWithLeft) X(m_collidingWithRight) X(m_maybeSavedPlayerFrame) \
    X(m_scaleXRelated2) X(m_groundYVelocity) X(m_yVelocityRelated) X(m_scaleXRelated3) \
    X(m_scaleXRelated4) X(m_scaleXRelated5) X(m_isCollidingWithSlope) X(m_isBallRotating) \
    X(m_unk669) X(m_currentSlope3) X(m_currentSlope) X(unk_584) \
    X(m_collidingWithSlopeId) X(m_slopeFlipGravityRelated) X(m_slopeAngleRadians) X(m_rotationSpeed) \
    X(m_rotateSpeed) X(m_isRotating) X(m_isBallRotating2) X(m_speedMultiplier) \
    X(m_yStart) X(m_gravity) X(m_gameModeChangedTime) X(m_padRingRelated) \
    X(m_maybeIsFalling) X(m_shouldTryPlacingCheckpoint) X(m_maybeCanRunIntoBlocks) X(m_isOnGround3) \
    X(m_checkpointTimeout) X(m_lastCheckpointTime) X(m_lastJumpTime) X(m_lastFlipTime) \
    X(m_lastSpiderFlipTime) X(m_unkBool5) X(m_accelerationOrSpeed) X(m_snapDistance) \
    X(m_ringJumpRelated) X(m_objectSnappedTo) X(m_onFlyCheckpointTries) X(m_slopeRotation) \
    X(m_currentSlopeYVelocity) X(m_unk3d0) X(m_blackOrbRelated) X(m_unk3e0) \
    X(m_unk3e1) X(m_isAccelerating) X(m_isCurrentSlopeTop) X(m_collidedTopMinY) \
    X(m_collidedBottomMaxY) X(m_collidedLeftMaxX) X(m_collidedRightMinX) X(m_canPlaceCheckpoint) \
    X(m_maybeIsColliding) X(m_jumpBuffered) X(m_stateRingJump) X(m_wasJumpBuffered) \
    X(m_wasRobotJump) X(m_stateJumpBuffered) X(m_stateRingJump2) X(m_touchedRing) \
    X(m_touchedCustomRing) X(m_touchedGravityPortal) X(m_maybeTouchedBreakableBlock) X(m_jumpRelatedAC2) \
    X(m_touchedPad) X(m_yVelocity) X(m_fallSpeed) X(m_isOnSlope) \
    X(m_wasOnSlope) X(m_slopeVelocity) X(m_maybeUpsideDownSlope) X(m_isShip) \
    X(m_isBird) X(m_isBall) X(m_isDart) X(m_isRobot) \
    X(m_isSpider) X(m_isUpsideDown) X(m_isOnGround) X(m_isGoingLeft) \
    X(m_isSideways) X(m_isSwing) X(m_reverseRelated) X(m_maybeReverseSpeed) \
    X(m_maybeReverseAcceleration) X(m_xVelocityRelated2) X(m_isDashing) X(m_unk9e8) \
    X(m_groundObjectMaterial) X(m_vehicleSize) X(m_playerSpeed) X(m_shipRotation) \
    X(m_lastPortalPos) X(m_unkUnused3) X(m_isOnGround2) X(m_lastLandTime) \
    X(m_platformerVelocityRelated) X(m_maybeIsBoosted) X(m_scaleXRelatedTime) X(m_decreaseBoostSlide) \
    X(m_unkA29) X(m_isLocked) X(m_controlsDisabled) X(m_lastGroundedPos) \
    X(m_lastActivatedPortal) X(m_hasEverJumped) X(m_ringOrStreakRelated) X(m_unkA99) \
    X(m_totalTime) X(m_isBeingSpawnedByDualPortal) X(m_unkAAC) X(m_unkAngle1) \
    X(m_yVelocityRelated3) X(m_followRelated) X(m_unk838) X(m_stateOnGround) \
    X(m_stateUnk) X(m_stateNoStickX) X(m_stateNoStickY) X(m_stateUnk2) \
    X(m_stateBoostX) X(m_stateBoostY) X(m_maybeStateForce2) X(m_stateScale) \
    X(m_platformerXVelocity) X(m_leftPressedFirst) X(m_scaleXRelated) X(m_maybeHasStopped) \
    X(m_xVelocityRelated) X(m_maybeGoingCorrectSlopeDirection) X(m_isSliding) X(m_maybeSlopeForce) \
    X(m_isOnIce) X(m_physDeltaRelated) X(m_isOnGround4) X(m_maybeSlidingTime) \
    X(m_maybeSlidingStartTime) X(m_changedDirectionsTime) X(m_slopeEndTime) X(m_isMoving) \
    X(m_platformerMovingLeft) X(m_platformerMovingRight) X(m_isSlidingRight) X(m_maybeChangedDirectionAngle) \
    X(m_unkUnused2) X(m_isPlatformer) X(m_stateNoAutoJump) X(m_stateDartSlide) \
    X(m_stateHitHead) X(m_stateFlipGravity) X(m_gravityMod) X(m_stateForce) \
    X(m_stateForceVector) X(m_affectedByForces) X(m_somethingPlayerSpeedTime) X(m_playerSpeedAC) \
    X(m_fixRobotJump) X(m_inputsLocked) X(m_unkUnused) X(m_isOutOfBounds) \
    X(m_fallStartY) X(m_disablePlayerSqueeze) X(m_robotHasRun3) X(m_robotHasRun2) \
    X(m_ignoreDamage) X(m_enable22Changes)

#define X(f) \
    template <class P, bool = requires(P& p) { \
        p.f; \
        requires !std::is_array_v<std::remove_cvref_t<decltype(p.f)>>; \
        requires std::is_default_constructible_v<std::remove_cvref_t<decltype(p.f)>>; \
        p.f = std::declval<std::remove_cvref_t<decltype(p.f)>&>(); \
    }> struct Slot_##f { \
        void take(P&) {} \
        void put(P&) const {} \
    }; \
    template <class P> struct Slot_##f<P, true> { \
        std::remove_cvref_t<decltype(std::declval<P&>().f)> v{}; \
        void take(P& p) { v = p.f; } \
        void put(P& p) const { p.f = v; } \
    };
    NXR_PLAYER_FIELDS(X)
#undef X

    struct SlotEnd {
        void take(PlayerObject&) {}
        void put(PlayerObject&) const {}
    };

    struct SavedPlayer :
#define X(f) Slot_##f<PlayerObject>,
        NXR_PLAYER_FIELDS(X)
#undef X
        SlotEnd {
        bool valid = false;
        cocos2d::CCPoint position;
        float rotation = 0.f;
        std::array<bool, 4> holding{};

        void takeAll(PlayerObject& p) {
#define X(f) Slot_##f<PlayerObject>::take(p);
            NXR_PLAYER_FIELDS(X)
#undef X
        }

        void putAll(PlayerObject& p) const {
#define X(f) Slot_##f<PlayerObject>::put(p);
            NXR_PLAYER_FIELDS(X)
#undef X
        }
    };

    struct SavedPair {
        SavedPlayer p1;
        SavedPlayer p2;
    };

    inline SavedPlayer capture(PlayerObject* p) {
        SavedPlayer s;
        if (!p) return s;
        s.position = p->getPosition();
        s.rotation = p->getRotation();
        s.takeAll(*p);
        for (int button = 1; button <= 3; button++) {
            auto it = p->m_holdingButtons.find(button);
            s.holding[button] = it != p->m_holdingButtons.end() && it->second;
        }
        s.valid = true;
        return s;
    }

    inline void restore(PlayerObject* p, const SavedPlayer& s) {
        if (!p || !s.valid) return;
        p->setPosition(s.position);
        p->setRotation(s.rotation);
        s.putAll(*p);
        for (int button = 1; button <= 3; button++) {
            p->m_holdingButtons[button] = s.holding[button];
        }
    }

    inline std::shared_ptr<SavedPair> capturePair(PlayLayer* pl) {
        auto pair = std::make_shared<SavedPair>();
        if (!pl) return pair;
        pair->p1 = capture(pl->m_player1);
        pair->p2 = capture(pl->m_player2);
        return pair;
    }

    inline void restorePair(PlayLayer* pl, const std::shared_ptr<SavedPair>& pair) {
        if (!pl || !pair) return;
        restore(pl->m_player1, pair->p1);
        if (pl->m_gameState.m_isDualMode) restore(pl->m_player2, pair->p2);
    }
}
