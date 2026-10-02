#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <cmath>
#include "../../core/nxr_ring_buffer.hpp"
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE("Creator", "Macro Buff", "Auto-places spikes on jump while playtesting. Highly experimental: works only on pure jumps, pads and gravity shifts break the structure. Cube and Robot only", false);

class $modify(NXRMacroBuffGJBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Macro Buff");

        NXR::tryAddHook(self, hack, "GJBaseGameLayer::processCommands");
    }

    struct Fields {
        bool cached = false;
        CCPoint spikeOffset;
        CCSize spikeSize;

        RingBuffer<CCRect> playerTrail{480};
        RingBuffer<GameObject*> spikes{800};
        int frame = 0;
    };

    void cacheSpike() {
        if (m_fields->cached) return;

        auto* lel = LevelEditorLayer::get();
        if (!lel) return;

        auto* probeSpike = lel->createObject(8, CCPoint(0.f, 0.f), true);
        if (!probeSpike) return;

        CCRect rect = probeSpike->getObjectRect();
        m_fields->spikeOffset = probeSpike->getPosition() - rect.origin;
        m_fields->spikeSize = rect.size;

        lel->removeObject(probeSpike, true);
        m_fields->cached = true;
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        auto* lel = LevelEditorLayer::get();
        if (!lel || static_cast<GJBaseGameLayer*>(lel) != this || !m_player1) return;

        cacheSpike();
        if (!m_fields->cached) return;

        CCRect playerRect = m_player1->getObjectRect();
        m_fields->playerTrail.push(playerRect);

        auto intersects = [](const CCRect& a, const CCRect& b) {
            return !(a.origin.x + a.size.width <= b.origin.x
                || b.origin.x + b.size.width <= a.origin.x
                || a.origin.y + a.size.height <= b.origin.y
                || b.origin.y + b.size.height <= a.origin.y);
        };

        m_fields->spikes.for_each([&](GameObject*& spike) {
            if (!spike) return;

            CCRect spikeRect = spike->getObjectRect();
            bool remove = false;

            m_fields->playerTrail.for_each([&](const CCRect& trailRect) {
                if (!remove && intersects(spikeRect, trailRect)) remove = true;
            });

            if (remove) {
                lel->removeObject(spike, true);
                spike = nullptr;
            }
        });

        if (m_player1->m_isOnGround) return;

        m_fields->frame++;
        if (m_fields->frame < 5) return;
        m_fields->frame = 0;

        const float vy = static_cast<float>(m_player1->m_yVelocity);

        auto spawnSpikeAt = [&](float originX, float originY) {
            CCPoint finalPos = CCPoint(originX, originY) + m_fields->spikeOffset;
            if (auto* obj = lel->createObject(8, finalPos, true)) {
                m_fields->spikes.push(obj);
            }
        };

        const float top = playerRect.origin.y + playerRect.size.height + 0.1f;
        const float bottom = playerRect.origin.y - m_fields->spikeSize.height - 0.1f;
        const float left = playerRect.origin.x - m_fields->spikeSize.width - 0.1f;
        const float right = playerRect.origin.x + playerRect.size.width + 0.1f;

        if (vy >= 0.f) spawnSpikeAt(left, top);
        else spawnSpikeAt(right, top);

        if (std::abs(vy) >= 5.0f) {
            if (vy >= 0.f) spawnSpikeAt(right, bottom);
            else spawnSpikeAt(left, bottom);
        }
    }
};

class $modify(NXRMacroBuffLevelEditorLayer, LevelEditorLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Macro Buff");

        NXR::tryAddHook(self, hack, "LevelEditorLayer::onPlaytest");
    }

    void onPlaytest() {
        LevelEditorLayer::onPlaytest();

        if (auto* layer = static_cast<NXRMacroBuffGJBaseGameLayer*>(static_cast<GJBaseGameLayer*>(LevelEditorLayer::get()))) {
            layer->m_fields->playerTrail.clear();
            layer->m_fields->spikes.clear();
            layer->m_fields->frame = 0;
        }
    }
};
