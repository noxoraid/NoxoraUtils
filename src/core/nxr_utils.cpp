#include "nxr_utils.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>

#include <fmt/core.h>
#include <Geode/Geode.hpp>
#include <Geode/binding/FLAlertLayer.hpp>

#include "nxr_gui.hpp"

std::string NXR::Utils::String::toLowerCase(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
                [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string NXR::Utils::String::toUpperCase(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
                [](unsigned char c) { return std::toupper(c); });
    return result;
}

std::string NXR::Utils::String::replaceChar(const std::string& input, char from, char to) {
    std::string result = input;
    std::replace(result.begin(), result.end(), from, to);
    return result;
}

std::string NXR::Utils::formatTime(double sec, bool ms = false) {
    int h = sec / 3600, m = (int(sec) % 3600) / 60, s = int(sec) % 60;
    int cs = (sec - int(sec)) * 100;

    auto base = h > 0 ? fmt::format("{:02d}:{:02d}:{:02d}", h, m, s)
                      : fmt::format("{:02d}:{:02d}", m, s);
    return ms ? fmt::format("{}.{:02d}", base, cs) : base;
}

void NXR::Utils::updateCursorState(bool show) {
    bool canShowInLevel = true;
    if (auto* playLayer = PlayLayer::get()) {
        canShowInLevel = playLayer->m_hasCompletedLevel ||
                         playLayer->m_isPaused ||
                         GameManager::sharedState()->getGameVariable("0024");
    }
    if (show || canShowInLevel)
        PlatformToolbox::showCursor();
    else
        PlatformToolbox::hideCursor();
}

void NXR::Utils::UncompleteLevel() {
    auto pl = PlayLayer::get();

	if (!pl) {
		NXR::MaterialLayer(FLAlertLayer::create("Uncomplete Level", "Enter playlayer to uncomplete the level", "OK"))->show();
		return;
	}

    auto gsm = GameStatsManager::get();
    auto glm = GameLevelManager::get();
    auto level = pl->m_level;
    int level_id = level->m_levelID.value();

    if (level->m_normalPercent >= 100 && gsm->hasCompletedLevel(level)) {
        gsm->m_completedLevels->removeObjectForKey(fmt::format("c_{}", level_id));
        gsm->m_completedLevels->removeObjectForKey(fmt::format("star_{}", level_id));
        gsm->m_completedLevels->removeObjectForKey(fmt::format("gstar_{}", level_id));
        gsm->m_completedLevels->removeObjectForKey(fmt::format("dstar_{}", level_id));
        gsm->m_completedLevels->removeObjectForKey(fmt::format("gdemon_{}", level_id));
        gsm->m_completedLevels->removeObjectForKey(fmt::format("demon_{}", level_id));
        gsm->m_completedLevels->removeObjectForKey(fmt::format("ddemon_{}", level_id));

        if (level_id >= 1 && level_id <= 22)
            gsm->setStat("4", gsm->getStat("4") - 1);
        else
            gsm->setStat("3", gsm->getStat("3") - 1);

        if (level->m_stars > 0) {

            if (level->isPlatformer())
                gsm->setStat("28", gsm->getStat("28") - level->m_stars);
            else
                gsm->setStat("6", gsm->getStat("6") - level->m_stars);
        }

        if (level->m_demon > 0)
            gsm->setStat("5", gsm->getStat("5") - 1);

        if (level_id >= 1 && level_id <= 22)
            gsm->setStat("8", gsm->getStat("8") - level->m_coinsVerified);
        else
            gsm->setStat("12", gsm->getStat("12") - level->m_coinsVerified);

    }

    auto currencyKey = gsm->getCurrencyKey(level);
    int awardedCurrency = gsm->getAwardedCurrencyForLevel(level);
    gsm->setStat("14", gsm->getStat("14") - awardedCurrency);

    cocos2d::CCDictionary* dict = nullptr;
    if (level->m_dailyID > 0) {
        dict = gsm->m_timelyCurrencyScores;
    }
    else if (level->m_levelType == GJLevelType::Main) {
        dict = gsm->m_mainCurrencyScores;
    }
    else if (level->m_gauntletLevel) {
        dict = gsm->m_gauntletCurrencyScores;
    }
    else {
        dict = gsm->m_onlineCurrencyScores;
    }
    dict->removeObjectForKey(currencyKey);

    for (auto i = 0; i < level->m_coins; i++) {
        auto key = level->getCoinKey(i + 1);
        if (gsm->hasUserCoin(key))
            gsm->m_verifiedUserCoins->removeObjectForKey(key);
        else if (gsm->hasPendingUserCoin(key))
            gsm->m_pendingUserCoins->removeObjectForKey(key);
        else if (gsm->hasSecretCoin(key))
            gsm->m_playerStats->removeObjectForKey(fmt::format("unique_{}", key));
    }

    level->m_normalPercent = 0;
    level->m_newNormalPercent2 = 0;
    level->m_practicePercent = 0;
    level->m_attempts = 0;
    level->m_attemptTime = 0;
    level->m_firstCoinVerified = 0;
    level->m_secondCoinVerified = 0;
    level->m_thirdCoinVerified = 0;
    level->m_isVerified = 0;
    level->m_isVerifiedRaw = 0;
    level->m_bestTime = 0;
    level->m_localBestTimes = "";
    level->m_inputsTime = "";
    level->m_personalBests = "";
    level->m_jumps = 0;
    level->m_clicks = 0;
    level->m_orbCompletion = 0;
    level->m_platformerSeed = 0;
    level->m_bestPoints = 0;
    level->m_isCompletionLegitimate = true;
}

double NXR::Utils::getRealProgress(GJBaseGameLayer* layer) {
    if (!layer || !layer->m_level) return 0.0;

    const auto level = layer->m_level;
    const auto timestamp = level->m_timestamp;

    double rawPercent = 0.0;
    if (timestamp > 0) {
        rawPercent = (layer->m_gameState.m_levelTime * 240.0 / timestamp * 100.0);
    } else if (layer->m_levelLength > 0.0f && layer->m_player1) {
        rawPercent = (layer->m_player1->getPositionX() * 100.0 / layer->m_levelLength);
    }

    if (std::isnan(rawPercent) || std::isinf(rawPercent)) return 0.0;

    return std::clamp(rawPercent, 0.0, 100.0);
}

float NXR::Utils::easeInOut(float x) {
    return -(cosf(M_PI * x) - 1) / 2;
}

void NXR::Utils::hsvToRgb(float h, float s, float v, float &r, float &g, float &b) {
    float chroma = v * s;
    h *= 360.0f;
    float h_prime = h / 60.0f;
    float x = chroma * (1.0f - std::fabs(std::fmod(h_prime, 2.0f) - 1.0f));
    float r1, g1, b1;
    if (0 <= h_prime && h_prime < 1) { r1 = chroma; g1 = x; b1 = 0; }
    else if (1 <= h_prime && h_prime < 2) { r1 = x; g1 = chroma; b1 = 0; }
    else if (2 <= h_prime && h_prime < 3) { r1 = 0; g1 = chroma; b1 = x; }
    else if (3 <= h_prime && h_prime < 4) { r1 = 0; g1 = x; b1 = chroma; }
    else if (4 <= h_prime && h_prime < 5) { r1 = x; g1 = 0; b1 = chroma; }
    else if (5 <= h_prime && h_prime < 6) { r1 = chroma; g1 = 0; b1 = x; }
    float m = v - chroma;
    r = r1 + m; g = g1 + m; b = b1 + m;
}

float NXR::Utils::getFps(bool only_read) {
    static auto lastTime = std::chrono::high_resolution_clock::now();
    static float frameTimes[240] = {}, frameTimeSum = 0.0f, accumulatedTime = 0.0f, cachedFps = 0.0f;
    static int index = 0, count = 0;
    static bool ready = false;

    if (only_read) {
        return cachedFps;
    }

    auto now = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(now - lastTime).count();
    lastTime = now;

    frameTimeSum -= frameTimes[index];
    frameTimeSum += frameTimes[index] = deltaTime;
    index = (index + 1) % 240;
    if (count < 240) count++;

    accumulatedTime += deltaTime;
    if (!ready || accumulatedTime >= 1.0f) {
        cachedFps = static_cast<float>(count) / frameTimeSum;
        if (accumulatedTime >= 1.0f) {
            accumulatedTime = 0.0f;
            ready = true;
        }
    }

    return cachedFps;
}

void NXR::Utils::setPitchShifter(int semitones) {
    static std::vector<FMOD::DSP*> shifters;
    auto* channel = FMODAudioEngine::get()->m_backgroundMusicChannel;
    if (!channel) return;

    semitones = std::clamp(semitones, -72, 72);

    int count = semitones == 0 ? 0 : std::ceil(std::abs(semitones) / 12.0f);

    while (shifters.size() > count) {
        channel->removeDSP(shifters.back());
        shifters.back()->release();
        shifters.pop_back();
    }

    while (shifters.size() < count) {
        FMOD::DSP* dsp = nullptr;
        if (FMODAudioEngine::get()->m_system->createDSPByType(FMOD_DSP_TYPE_PITCHSHIFT, &dsp) != FMOD_OK) return;
        channel->addDSP(0, dsp);
        shifters.push_back(dsp);
    }

    float factor = std::pow(2.0f, (static_cast<float>(semitones) / count) / 12.0f);
    for (auto* dsp : shifters) {
        dsp->setParameterFloat(FMOD_DSP_PITCHSHIFT_FFTSIZE, 2048);
        dsp->setParameterFloat(FMOD_DSP_PITCHSHIFT_PITCH, factor);
    }
}

void NXR::Utils::setReverb(float decayTime) {
    static FMOD::DSP* reverb_dsp = nullptr;
    auto* channel = FMODAudioEngine::get()->m_backgroundMusicChannel;
    if (!channel) return;

    if (decayTime <= 0.0f) {
        if (!reverb_dsp) return;
        channel->removeDSP(reverb_dsp);
        reverb_dsp->release();
        return void(reverb_dsp = nullptr);
    }

    if (!reverb_dsp) {
        if (FMODAudioEngine::get()->m_system->createDSPByType(FMOD_DSP_TYPE_SFXREVERB, &reverb_dsp) != FMOD_OK) return;
        channel->addDSP(0, reverb_dsp);
    }

    reverb_dsp->setParameterFloat(FMOD_DSP_SFXREVERB_DECAYTIME, std::clamp(decayTime, 100.0f, 20000.0f));
}

cocos2d::ccColor3B NXR::Utils::hexToColor(std::string_view hexStr) {
    if (hexStr.empty()) return { 255, 255, 255 };
    if (hexStr[0] == '#') hexStr.remove_prefix(1);

    uint32_t hexValue = 0xFFFFFF;
    std::from_chars(hexStr.data(), hexStr.data() + hexStr.size(), hexValue, 16);

    return {
        static_cast<GLubyte>((hexValue >> 16) & 0xFF),
        static_cast<GLubyte>((hexValue >> 8) & 0xFF),
        static_cast<GLubyte>(hexValue & 0xFF)
    };
}

cocos2d::ccColor4F NXR::Utils::hexToColor4F(std::string_view hexStr) {
    if (hexStr.empty()) return {1.f, 1.f, 1.f, 1.f};
    if (hexStr[0] == '#') hexStr.remove_prefix(1);

    uint32_t hexValue = 0;
    auto [ptr, ec] = std::from_chars(hexStr.data(), hexStr.data() + hexStr.size(), hexValue, 16);

    if (hexStr.length() == 6) {
        hexValue = (hexValue << 8) | 0xFF;
    }

    return {
        ((hexValue >> 24) & 0xFF) / 255.0f,
        ((hexValue >> 16) & 0xFF) / 255.0f,
        ((hexValue >> 8)  & 0xFF) / 255.0f,
        (hexValue & 0xFF)         / 255.0f
    };
}

struct PatternByte {
    bool isWildcard;
    uint8_t value;
};

uintptr_t NXR::Utils::PatternScan(uintptr_t base, uintptr_t scanSize, const std::string signature) {
    std::vector<PatternByte> patternData;

    for (size_t i = 0; i < signature.size(); ++i) {
        if (signature[i] == ' ') {
            continue;
        }

        if (signature[i] == '?') {
            patternData.push_back({ true, 0 });
        }
        else {
            std::string byteStr = signature.substr(i, 2);
            patternData.push_back({ false, static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16)) });
            i++;
        }
    }

    for (uintptr_t i = base; i < base + scanSize; ++i) {
        bool found = true;

        for (size_t j = 0; j < patternData.size(); ++j) {
            if (patternData[j].isWildcard) {
                continue;
            }

            if (patternData[j].value != *reinterpret_cast<uint8_t*>(i + j)) {
                found = false;
                break;
            }
        }

        if (found) {
            return i;
        }
    }

    return 0;
}

bool NXR::Utils::isOnlyAsciiPath(const std::filesystem::path& p) {
    auto u8str = p.u8string();
    return std::all_of(u8str.begin(), u8str.end(), [](char8_t c) {
        return static_cast<unsigned char>(c) <= 127;
    });
}

void NXR::Utils::checkModUpdate(geode::Function<void(bool)> callback) {
    static async::TaskHolder<Result<std::optional<VersionInfo>>> updateTask;

    updateTask.spawn("check-updates-task", geode::Mod::get()->checkUpdates(),
        [callback = std::move(callback)](geode::Result<std::optional<VersionInfo>> res) mutable {
            if (!res) {
                geode::log::debug("failed to check for updates: {}", res.unwrapErr());
                callback(false);
                return;
            }

            auto updateOpt = res.unwrap();
            bool hasUpdate = updateOpt.has_value();

            if (hasUpdate) {
                geode::log::debug("new update available: {}", updateOpt.value().toVString());
            } else {
                geode::log::debug("mod is up to date");
            }

            callback(hasUpdate);
        }
    );
}
