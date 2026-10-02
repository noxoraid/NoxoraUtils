#include "nxr_bot.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace NXR::Bot;

namespace {
    // Frame terakhir di A yang posisinya (x) paling dekat dengan titik awal B.
    // Dipakai kalau B direkam dari startpos (frame B mulai dari 1 lagi, bukan frame asli level).
    bool matchByPosition(const Macro& a, const Macro& b, uint32_t& frameInA) {
        if (a.frames.empty() || b.frames.empty()) return false;

        const auto& target = b.frames.front();
        const float tx = target.p1.x;
        const float ty = target.p1.y;
        if (tx == 0.f) return false;

        float best = std::numeric_limits<float>::max();
        bool found = false;

        for (const auto& row : a.frames) {
            if (row.p1.x == 0.f) continue;
            const float dx = std::fabs(row.p1.x - tx);
            const float dy = std::fabs(row.p1.y - ty) * 0.01f;
            const float score = dx + dy;
            if (score < best) {
                best = score;
                frameInA = row.frame;
                found = true;
            }
        }

        // toleransi satu langkah fisika (~ 0.1 block per tick, dilonggarkan)
        return found && best < 40.f;
    }

    bool sameLayout(const Macro& a, const Macro& b) {
        return a.layout == b.layout;
    }

    void mergeSupersSeparate(const Macro& a, const Macro& b, Macro& out) {
        if (!sameLayout(a, b)) return;
        out.layout = a.layout;

        for (const auto& sa : a.supers) {
            const SuperFrame* sb = b.superAt(sa.frame);
            if (!sb) continue;
            SuperFrame sf;
            sf.frame = sa.frame;
            sf.p1 = sa.p1;
            sf.p2 = sb->p2;
            out.supers.push_back(std::move(sf));
        }
    }
}

bool NXR::Bot::mergeMacros(const Macro& a, const Macro& b, MergeMode mode, Macro& out, MergeReport& report, std::string& error) {
    if (a.events.empty() && a.frames.empty()) { error = "Macro 1 kosong"; return false; }
    if (b.events.empty() && b.frames.empty()) { error = "Macro 2 kosong"; return false; }

    if (std::abs(a.tps - b.tps) > 0.5f) {
        error = "TPS kedua macro beda, tidak bisa digabung";
        return false;
    }

    Macro result;
    result.levelName = a.levelName.empty() ? b.levelName : a.levelName;
    result.hasStats = a.hasStats || b.hasStats;
    result.stats = a.hasStats ? a.stats : b.stats;
    result.levelId = a.levelId != 0 ? a.levelId : b.levelId;
    result.version = a.version;
    result.fps = a.fps;
    result.tps = a.tps;
    result.layout = a.layout;

    if (mode == MergeMode::Players) {
        // P1 dari A, P2 dari B. Frame harus sejajar (sama-sama direkam dari awal yang sama).
        for (const auto& ev : a.events) {
            if (ev.player() == 1) result.events.push_back(ev);
        }
        for (const auto& ev : b.events) {
            if (ev.player() == 2) result.events.push_back(ev);
        }

        for (const auto& ra : a.frames) {
            const MacroFrame* rb = b.rowAt(ra.frame);
            if (!rb) continue;
            MacroFrame row = ra;
            row.p2 = rb->p2;
            row.full = ra.full && rb->full;
            result.frames.push_back(row);
        }

        if (result.frames.empty() && !a.frames.empty()) {
            error = "Frame macro 1 dan 2 tidak sejajar. Rekam dua-duanya dari titik awal yang sama";
            return false;
        }

        mergeSupersSeparate(a, b, result);
        result.totalFrames = std::max(a.totalFrames, b.totalFrames);
        result.finalize(true);

        report.cutFrame = 0;
        report.shift = 0;
        report.note = "P1 dari macro 1, P2 dari macro 2";
    } else {
        // Sambung waktu: A sampai titik sambung, lalu B.
        if (b.frames.empty() && b.events.empty()) { error = "Macro 2 kosong"; return false; }

        const uint32_t bFirstRow = b.frames.empty() ? 1u : b.frames.front().frame;
        int64_t shift = 0;
        uint32_t cut = bFirstRow;

        if (bFirstRow <= 2) {
            // B direkam dari startpos: frame dimulai dari 1 lagi, cari frame padanan di A lewat posisi.
            uint32_t frameInA = 0;
            if (!matchByPosition(a, b, frameInA)) {
                error = "Titik awal macro 2 tidak ketemu di macro 1 (posisi beda jauh). Pastikan startpos-nya ada di jalur macro 1";
                return false;
            }
            shift = static_cast<int64_t>(frameInA) - static_cast<int64_t>(bFirstRow);
            cut = frameInA;
        } else {
            // B direkam dari checkpoint: frame sudah absolut.
            if (a.endFrame() < bFirstRow) {
                error = "Macro 1 lebih pendek dari titik awal macro 2";
                return false;
            }
            cut = bFirstRow;
        }

        auto shifted = [shift](uint64_t frame) {
            const int64_t v = static_cast<int64_t>(frame) + shift;
            return static_cast<uint64_t>(v < 1 ? 1 : v);
        };

        // A: semua yang sebelum titik sambung
        std::array<std::array<bool, 4>, 2> held{};
        for (const auto& ev : a.events) {
            if (ev.frame() >= cut) break;
            result.events.push_back(ev);
            const int btn = ev.button();
            if (btn >= 1 && btn <= 3) held[ev.player() == 2 ? 1 : 0][btn] = ev.down();
        }
        for (const auto& row : a.frames) {
            if (row.frame >= cut) break;
            result.frames.push_back(row);
        }
        for (const auto& sf : a.supers) {
            if (sf.frame >= cut) break;
            result.supers.push_back(sf);
        }

        // Lepas tombol yang masih ketahan dari A di titik sambung, B mulai dari keadaan bersih
        for (int slot = 0; slot < 2; slot++) {
            for (int btn = 1; btn <= 3; btn++) {
                if (held[slot][btn]) {
                    result.events.push_back(InputEvent::make(cut, slot == 1 ? 2 : 1, static_cast<uint8_t>(btn), false));
                }
            }
        }

        // B: digeser ke posisi frame yang benar
        for (const auto& ev : b.events) {
            result.events.push_back(InputEvent::make(shifted(ev.frame()), ev.player(), ev.button(), ev.down()));
        }
        for (auto row : b.frames) {
            row.frame = static_cast<uint32_t>(shifted(row.frame));
            result.frames.push_back(row);
        }
        if (sameLayout(a, b) || a.supers.empty()) {
            if (a.supers.empty()) result.layout = b.layout;
            for (auto sf : b.supers) {
                sf.frame = static_cast<uint32_t>(shifted(sf.frame));
                result.supers.push_back(std::move(sf));
            }
        }

        result.totalFrames = std::max<uint64_t>(cut, shifted(b.totalFrames));
        result.finalize(true);

        report.cutFrame = cut;
        report.shift = shift;
        report.note = "Macro 1 sampai frame " + std::to_string(cut) + ", lalu macro 2";
    }

    report.events = result.events.size();
    report.frames = result.frames.size();
    out = std::move(result);
    return true;
}
