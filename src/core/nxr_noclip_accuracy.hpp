#pragma once
#include <Geode/Geode.hpp>

class NXRNoclipAccuracy {
public:
    static NXRNoclipAccuracy& get() {
        static NXRNoclipAccuracy instance;
        return instance;
    }

    NXRNoclipAccuracy& operator=(const NXRNoclipAccuracy&) = delete;
    NXRNoclipAccuracy(const NXRNoclipAccuracy&) = delete;

    int frames = 0, deaths = 0, deaths_full = 0;
    bool wouldDie = false, prevDied = false;

    void handle_update(GJBaseGameLayer* self, float delta) {
        auto pl = PlayLayer::get();
        if (!pl || self->m_player1->m_isDead || pl->m_levelEndAnimationStarted) {
            prevDied = false;
            return;
        }

        frames++;

        if (wouldDie) {
            wouldDie = false;
            deaths++;

            if (!prevDied) {
                deaths_full++;
                prevDied = true;
            }
        } else {
            prevDied = false;
        }
    }

    void handle_reset() {
        frames = 0;
        deaths = 0;
        deaths_full = 0;
        wouldDie = false;
        prevDied = false;
    }

    void handle_death() {
        wouldDie = true;
    }

    float getPercentage() const {
        if (frames == 0) return 100.0f;
        return (1.0f - static_cast<float>(deaths) / frames) * 100.0f;
    }

private:
    NXRNoclipAccuracy() = default;
};
