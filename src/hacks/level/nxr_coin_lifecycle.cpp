#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "../../core/nxr_coin_scan.hpp"

class $modify(NXRCoinLifecyclePlayLayer, PlayLayer) {
    void resetLevel() {
        NXR::Coins::cache().clear();
        PlayLayer::resetLevel();
    }

    void onQuit() {
        NXR::Coins::cache().clear();
        PlayLayer::onQuit();
    }
};
