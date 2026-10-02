#pragma once
#include <Geode/Geode.hpp>
#include <vector>
#include <algorithm>

namespace NXR::Coins {
    struct CoinEntry {
        geode::Ref<GameObject> object;
        cocos2d::CCPoint home;
        bool collected = false;
    };

    struct Cache {
        PlayLayer* owner = nullptr;
        geode::Ref<GameObject> anchor;
        std::vector<CoinEntry> coins;

        void clear() {
            owner = nullptr;
            anchor = nullptr;
            coins.clear();
        }
    };

    inline Cache& cache() {
        static Cache c;
        return c;
    }

    inline bool isCoinId(int id) {
        return id == 142 || id == 1329 || id == 1614;
    }

    inline void rebuild(PlayLayer* layer) {
        auto& c = cache();
        c.clear();
        if (!layer || !layer->m_objects || layer->m_objects->count() == 0) return;

        c.owner = layer;
        c.anchor = static_cast<GameObject*>(layer->m_objects->objectAtIndex(0));

        for (auto* obj : geode::cocos::CCArrayExt<GameObject*>(layer->m_objects)) {
            if (!obj) continue;
            if (!isCoinId(obj->m_objectID)) continue;
            CoinEntry e;
            e.object = obj;
            e.home = obj->getPosition();
            c.coins.push_back(std::move(e));
        }
    }

    inline Cache& ensure(PlayLayer* layer) {
        auto& c = cache();
        if (!layer || !layer->m_objects || layer->m_objects->count() == 0) {
            c.clear();
            return c;
        }
        auto* first = static_cast<GameObject*>(layer->m_objects->objectAtIndex(0));
        if (c.owner != layer || c.anchor.data() != first) rebuild(layer);
        return c;
    }
}
