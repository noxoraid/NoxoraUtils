#pragma once
#include <string>
#include <vector>

namespace NXR { namespace Layout {
    struct WindowInfo {
        std::string window_name;
        float w, h, x, y;
    };

    class Manager {
    public:
        static Manager& get() {
            static Manager instance;
            return instance;
        }

        void setLayout(const std::vector<std::vector<std::string>>& layout);
        void setFixedWindowSizeInfo(const std::vector<WindowInfo>& fixedWindows);

        void startCollecting();
        bool isCollecting();
        void finishCollecting();
        void addWindowInfo(const std::string& name, float width, float height);

        void startApplying();
        bool isApplying();
        void finishApplying();

        bool applyWindowTransform(const std::string& name);

        float multipleScale(float value) const { return value * m_scale; }

        WindowInfo* getWindowInfo(const std::string& name);

        void reset();
    private:
        Manager() = default;

        WindowInfo* findWindow(const std::string& name);
        WindowInfo* findFixedWindow(const std::string& name);
        float getMaxWidthInColumn(const std::vector<std::string>& column);
        float getColumnHeight(const std::vector<std::string>& column);
        void calculateWindowPositions();

    private:
        enum class Stage {
            Idle = 0,
            Collecting = 1,
            Calculating = 2,
            Applying = 3
        };

        std::vector<WindowInfo> m_windows;
        std::vector<WindowInfo> m_fixedWindows;
        std::vector<WindowInfo> m_baseWindows;
        std::vector<std::vector<std::string>> m_layout;
        Stage m_stage = Stage::Idle;

        bool m_isBaseCollected = false;
        float m_scale = 1.0f;

        const float PADDING_X = 10.0f;
        const float PADDING_Y = 10.0f;
        const float SPACING_Y = 10.0f;
    };
} }
