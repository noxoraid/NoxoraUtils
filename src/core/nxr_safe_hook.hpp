#pragma once
#include <Geode/Geode.hpp>
#include "nxr_hacks.hpp"

namespace NXR {

    template <class Self>
    inline bool tryAddHook(Self& self, Hack& hack, const char* name) {
        auto res = self.getHook(name);
        if (!res) {
            geode::log::warn("NXR: hook '{}' not available, skipping ({})", name, res.unwrapErr());
            return false;
        }
        hack.addHookPtr(res.unwrap());
        return true;
    }

    template <class Self>
    inline void trySetPriority(Self& self, const char* name, int priority) {
        auto res = self.setHookPriority(name, priority);
        if (!res) {
            geode::log::warn("NXR: cannot set priority for '{}' ({})", name, res.unwrapErr());
        }
    }
}
