#pragma once
#include <string>

namespace NXR { namespace Utils {

    namespace String {
        std::string toLowerCase(const std::string& input);
        std::string toUpperCase(const std::string& input);
        std::string replaceChar(const std::string& input, char from, char to);
    }

    std::string formatTime(double sec, bool ms);
    void updateCursorState(bool show);
    void UncompleteLevel();
    double getRealProgress(GJBaseGameLayer* layer);
    float easeInOut(float t);
    void hsvToRgb(float h, float s, float v, float &r, float &g, float &b);

    float getFps(bool only_read = false);
    void setPitchShifter(int semitones);
    void setReverb(float decayTime);

    cocos2d::ccColor3B hexToColor(std::string_view hexStr);
    cocos2d::ccColor4F hexToColor4F(std::string_view hexStr);

    uintptr_t PatternScan(uintptr_t base, uintptr_t scanSize, const std::string signature);
    bool isOnlyAsciiPath(const std::filesystem::path& p);

    void checkModUpdate(geode::Function<void(bool)> callback);
} }
