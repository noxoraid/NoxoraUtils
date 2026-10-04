#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace NXR::LStar {
    constexpr int kMetrics = 8;
    constexpr double kMinLeft = 0.001;
    constexpr double kMaxRight = 1000000000000000.0;
    constexpr int kSecantIterations = 10;
    constexpr int kBisectIterations = 60;
    constexpr double kDefaultNerve = 0.0016520833717346;
    constexpr double kDefaultFatigue = 0.0002727763242154;
    constexpr double kDefaultCps = 0.2784421686721826;

    struct Click {
        int64_t key = 0;
        double window = 1.0;
    };

    struct Params {
        double fps = 240.0;
        double respawn = 0.0;
        double target = 86400.0;
        double kT = kDefaultNerve;
        double kU = kDefaultFatigue;
        double kC = kDefaultCps;
    };

    inline double fastErfc(double x) {
        constexpr int size = 100000;
        constexpr double maxX = 6.0;
        static const std::vector<double> table = [] {
            std::vector<double> t(static_cast<size_t>(size) + 1);
            for (int i = 0; i <= size; i++) t[static_cast<size_t>(i)] = std::erfc(static_cast<double>(i) / size * maxX);
            return t;
        }();
        if (std::isnan(x)) return 1.0;
        if (x >= maxX) return 0.0;
        if (x <= 0.0) return 1.0;
        const double scaled = x * (size / maxX);
        const int index = static_cast<int>(scaled);
        if (index >= size) return 0.0;
        const double frac = scaled - index;
        return table[static_cast<size_t>(index)] + frac * (table[static_cast<size_t>(index) + 1] - table[static_cast<size_t>(index)]);
    }

    inline double expectedTime(const std::vector<double>& T, const std::vector<double>& W, int maxIndex, int startIndex, double L) {
        if (std::isnan(L) || L <= 0.0) return 1e100;

        double r = 1.0;
        double fails = 0.0;
        const double last = T[static_cast<size_t>(maxIndex)];

        for (int j = startIndex; j <= maxIndex; j++) {
            const double x = W[static_cast<size_t>(j)] * L;
            if (std::isnan(x) || x > 6.0) continue;
            const double q = fastErfc(x);
            double p = 1.0 - q;
            if (p < 1e-15) p = 1e-15;
            fails += T[static_cast<size_t>(j)] * r * q;
            r *= p;
            if (r < 1e-200) {
                r = 0.0;
                break;
            }
        }
        if (r <= 0.0 || std::isnan(r) || std::isnan(fails)) return 1e100;
        const double result = (last * r + fails) / r;
        return (std::isnan(result) || std::isinf(result)) ? 1e100 : result;
    }

    inline double solve(const std::vector<double>& T, const std::vector<double>& W, int maxIndex, double target) {
        int start = 0;
        double prev = kMinLeft;

        double L0 = prev;
        double f0 = expectedTime(T, W, maxIndex, start, L0) - target;
        double L1 = L0 * 1.001 + 0.001;
        double f1 = expectedTime(T, W, maxIndex, start, L1) - target;
        double next = L1;
        bool converged = false;

        if (std::fabs(f0) < 1e90 && std::fabs(f1) < 1e90) {
            for (int i = 0; i < kSecantIterations; i++) {
                const double denom = f1 - f0;
                if (std::fabs(denom) < 1e-9 || std::isnan(denom) || std::isinf(denom)) break;
                const double delta = f1 * (L1 - L0) / denom;
                if (std::isnan(delta) || std::isinf(delta)) break;
                next = L1 - delta;
                if (std::isnan(next) || std::isinf(next)) break;
                next = std::clamp(next, kMinLeft, kMaxRight);
                const double fNext = expectedTime(T, W, maxIndex, start, next) - target;
                if (std::isnan(fNext)) break;
                if (std::fabs(fNext) < 1e-4) {
                    converged = true;
                    break;
                }
                L0 = L1;
                f0 = f1;
                L1 = next;
                f1 = fNext;
            }
        }

        if (!converged) {
            double left = kMinLeft;
            double right = prev + 10.0;
            int expansions = 0;
            while (expectedTime(T, W, maxIndex, start, right) > target && expansions < 64) {
                right *= 2.0;
                expansions++;
                if (right > kMaxRight) {
                    right = kMaxRight;
                    break;
                }
            }
            for (int i = 0; i < kBisectIterations; i++) {
                const double mid = (left + right) * 0.5;
                const double value = expectedTime(T, W, maxIndex, start, mid);
                if (value > target) left = mid;
                else right = mid;
                if ((right - left < 1e-5) || ((right - left) / (mid > 0.0 ? mid : 1.0) < 1e-5)) break;
            }
            next = (left + right) * 0.5;
        }

        if (std::isnan(next) || std::isinf(next)) return kMinLeft;
        return std::clamp(next, kMinLeft, kMaxRight);
    }

    inline std::array<double, kMetrics> compute(const std::vector<Click>& clicks, const Params& params) {
        std::array<double, kMetrics> out{};
        const size_t count = clicks.size();
        if (count == 0) return out;

        const double fps = params.fps > 0.0 ? params.fps : 240.0;
        const double target = params.target > 0.0 ? params.target : 86400.0;
        constexpr double magic = 0.5 * 0.7071067811865475;

        std::vector<double> T(count, 0.0);
        std::array<std::vector<double>, kMetrics> W;
        for (auto& w : W) w.assign(count, 0.0);

        auto sane = [](double v) {
            if (std::isnan(v) || std::isinf(v) || v < 0.0) return 0.0;
            return v;
        };

        double prevTime = 0.0;
        int64_t prevInput = 0;

        for (size_t m = 0; m < count; m++) {
            const double t = params.respawn + static_cast<double>(clicks[m].key) / fps;
            const int64_t input = static_cast<int64_t>(m) + 1;
            T[m] = t;

            const double window = (clicks[m].window > 0.0 ? clicks[m].window : 1.0) / fps;
            const double nerve = std::exp(-params.kT * t);
            double fatigue = std::exp(-params.kU * static_cast<double>(input));
            if (std::isnan(fatigue) || std::isinf(fatigue)) fatigue = 0.0;

            double deltaTime = 1.0;
            if (input - prevInput != 0) {
                const double dt = (t - prevTime) / static_cast<double>(input - prevInput);
                deltaTime = (dt <= 0.0 || std::isnan(dt)) ? 1.0 : dt;
            }
            const double maxVal = std::max(1.0, 2.0 / deltaTime);
            double cps = std::pow(4.0 / maxVal, params.kC);
            if (std::isnan(cps) || std::isinf(cps) || cps < 0.0) cps = 1.0;

            const double base = window * magic;
            W[0][m] = sane(base);
            W[1][m] = sane(base * nerve);
            W[2][m] = sane(base * fatigue);
            W[3][m] = sane(base * cps);
            W[4][m] = sane(base * nerve * fatigue);
            W[5][m] = sane(base * nerve * cps);
            W[6][m] = sane(base * fatigue * cps);
            W[7][m] = sane(base * nerve * fatigue * cps);

            prevTime = t;
            prevInput = input;
        }

        for (int i = 0; i < kMetrics; i++) {
            out[static_cast<size_t>(i)] = solve(T, W[static_cast<size_t>(i)], static_cast<int>(count) - 1, target);
        }
        return out;
    }
}
