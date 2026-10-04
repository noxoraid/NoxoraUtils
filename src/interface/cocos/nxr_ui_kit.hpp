#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "../../core/nxr_form.hpp"
#include "../../core/nxr_hacks.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_utils.hpp"
#include "../../core/nxr_ui_mode.hpp"

namespace NXR::Kit {
    using namespace geode::prelude;

    class ScrollGrip : public CCLayer {
    public:
        static ScrollGrip* create(geode::ScrollLayer* target, float x, float y, float height, float zoneWidth);
        void update(float dt) override;
        bool ccTouchBegan(CCTouch* touch, CCEvent* event) override;
        void ccTouchMoved(CCTouch* touch, CCEvent* event) override;
        void ccTouchEnded(CCTouch* touch, CCEvent* event) override;
        void ccTouchCancelled(CCTouch* touch, CCEvent* event) override;
        void registerWithTouchDispatcher() override;

    private:
        bool init(geode::ScrollLayer* target, float x, float y, float height, float zoneWidth);
        float range() const;
        float thumbHeight() const;
        void redraw();
        void apply(const CCPoint& world, bool begin);
        bool onTop() const;

        geode::ScrollLayer* m_target = nullptr;
        CCDrawNode* m_draw = nullptr;
        float m_h = 0.f;
        float m_zone = 0.f;
        float m_grab = 0.f;
        float m_shownY = 1e9f;
        bool m_down = false;
        bool m_shownDown = false;
    };

    namespace Pal {
        inline ccColor3B hexOr(const char* key, const char* fallback) {
            return NXR::Utils::hexToColor(NXRConfig::get().get<std::string>(key, fallback));
        }
        inline ccColor3B accent() { return hexOr(NXR::Ui::kAccentColorKey, "22D3EE"); }
        inline ccColor3B accent2() { return hexOr(NXR::Ui::kGradientColorKey, "7C3AED"); }
        inline ccColor3B panel() { return hexOr(NXR::Ui::kPanelColorKey, "0D121B"); }
        inline bool gradientOn() { return NXRConfig::get().get<bool>(NXR::Ui::kGradientOnKey, false); }
        inline bool gradientHorizontal() { return NXRConfig::get().get<int>(NXR::Ui::kGradientDirKey, 0) == 1; }
        inline ccColor3B onAccent() {
            const ccColor3B c = accent();
            const float lum = 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
            return lum > 150.f ? ccc3(6, 26, 34) : ccc3(246, 249, 252);
        }
        inline ccColor3B text() { return ccc3(232, 238, 246); }
        inline ccColor3B muted() { return ccc3(134, 148, 170); }
        inline ccColor3B danger() { return ccc3(255, 128, 128); }
        inline ccColor3B white() { return ccc3(255, 255, 255); }
    }

    inline ccColor4F fillColor(int r, int g, int b, float a = 1.f) {
        return ccc4f(static_cast<float>(r) / 255.f * a, static_cast<float>(g) / 255.f * a, static_cast<float>(b) / 255.f * a, a);
    }

    inline ccColor4F fromColor(const ccColor3B& c, float a = 1.f) {
        return fillColor(c.r, c.g, c.b, a);
    }

    inline ccColor4F mixColor(const ccColor3B& a, const ccColor3B& b, float t, float alpha = 1.f) {
        t = std::clamp(t, 0.f, 1.f);
        const int r = static_cast<int>(a.r + (b.r - a.r) * t);
        const int g = static_cast<int>(a.g + (b.g - a.g) * t);
        const int bl = static_cast<int>(a.b + (b.b - a.b) * t);
        return fillColor(r, g, bl, alpha);
    }

    enum class Icon {
        StarOn,
        StarOff,
        Chevron,
        Close,
        Search,
        Global,
        Player,
        Level,
        Bot,
        Utils,
        Creator,
        Settings,
        About,
        Check,
        Back,
        Edit,
        Info,
    };

    CCSprite* makeIcon(Icon icon, float targetSize, const ccColor3B& color);
    Icon iconForWindow(const std::string& name);

    void drawRound(CCDrawNode* node, float x, float y, float w, float h, float radius, const ccColor4F& color);
    void drawCircle(CCDrawNode* node, float cx, float cy, float radius, const ccColor4F& color);
    void drawGradient(CCDrawNode* node, float x, float y, float w, float h, float radius, const ccColor4F& from, const ccColor4F& to, bool horizontal);
    void drawAccent(CCDrawNode* node, float x, float y, float w, float h, float radius, float alpha = 1.f);

    float lineHeightUnit();
    geode::Label* makeLabel(const std::string& text, float linePt, const ccColor3B& color, const CCPoint& anchor);
    void fitLabel(geode::Label* label, float maxWidth);
    std::string wrapText(const std::string& text, float maxWidth, float linePt);
    int countLines(const std::string& text);

    struct ColorSpec {
        std::string label;
        std::string key;
        std::string defaultHex;
        bool alpha = false;
        std::string rainbowKey;
        std::function<void()> onChange;
    };

    struct SliderSpec {
        std::string label;
        std::string key;
        std::string suffix;
        float min = 0.f;
        float max = 1.f;
        float def = 0.f;
        float step = 0.01f;
        SliderScale scale = SliderScale::Linear;
        std::vector<SliderPreset> presets;
        std::function<void(float)> callback;
        std::function<void(float)> onCommit;
        std::function<float()> getter;
        std::function<void(float)> setter;
        bool integer = false;
    };

    enum class NumberKind {
        Integer,
        Decimal,
        Hex,
    };

    class Host {
    public:
        virtual ~Host() = default;
        virtual float dp(float value) const = 0;
        virtual void openColorPicker(const ColorSpec& spec, std::function<void()> onDone) = 0;
        virtual void openNumberInput(const std::string& title, const std::string& hint, const std::string& current, NumberKind kind, std::function<void(const std::string&)> onApply) = 0;
        virtual void openHackSheet(NXR::Hack& hack) = 0;
        virtual void openInfoSheet(NXR::Hack& hack) = 0;
        virtual void favoritesChanged() = 0;
        virtual void statusChanged() = 0;
    };

    class Control {
    public:
        enum class Touch {
            None,
            Tap,
            Drag,
        };

        explicit Control(Host* host) : m_host(host) {}
        virtual ~Control() = default;

        void setWidth(float width) { m_width = width; }

        virtual float height() = 0;
        virtual void build(CCNode* view, float width) = 0;
        virtual void release() {}
        virtual Touch touchBegan(const CCPoint&) { return Touch::None; }
        virtual bool wantsDrag(const CCPoint&, const CCPoint&) { return false; }
        virtual void touchMoved(const CCPoint&) {}
        virtual void touchEnded(const CCPoint&, bool) {}
        virtual void touchCancelled() {}
        virtual bool passthrough(const CCPoint&) { return false; }
        virtual void tick(float) {}

    protected:
        Host* m_host = nullptr;
        float m_width = 0.f;

        float dp(float value) const { return m_host->dp(value); }
        Host* host() const { return m_host; }
    };

    using ControlPtr = std::shared_ptr<Control>;

    class PanelList : public CCNode {
    public:
        static PanelList* create(const CCSize& size, bool virtualize = true);

        void setItems(std::vector<ControlPtr> items, float topPad = 0.f, float bottomPad = 0.f);
        void setScroll(float value);
        float getScroll() const { return m_scroll; }
        float maxScroll() const;
        bool containsWorld(const CCPoint& world);
        bool onTouchBegan(const CCPoint& world);
        void onTouchMoved(const CCPoint& world);
        void onTouchEnded(const CCPoint& world);
        void onTouchCancelled();
        bool passthroughAt(const CCPoint& world);
        bool isInteracting() const { return m_mode != Mode::Idle; }
        void setSlop(float slop) { m_slop = slop; }
        void setGutter(float width);
        float contentWidth() const { return m_viewW - m_gutter; }
        size_t itemCount() const { return m_entries.size(); }

        void update(float dt) override;

    private:
        enum class Mode {
            Idle,
            Pending,
            Scrolling,
            Captured,
            Bar,
        };

        struct Entry {
            ControlPtr control;
            float top = 0.f;
            float height = 0.f;
            CCNode* view = nullptr;
        };

        bool init(const CCSize& size, bool virtualize);
        void buildEntry(Entry& entry);
        void destroyEntry(Entry& entry);
        void refreshVisible();
        void clearEntries();
        int entryAt(const CCPoint& world) const;
        CCPoint itemLocal(const CCPoint& world, const Entry& entry) const;
        void applyScrollPosition();
        void redrawBar();
        void dragBar(const CCPoint& world, bool begin);
        bool inGutter(const CCPoint& world) const;
        float thumbHeight() const;

        std::vector<Entry> m_entries;
        CCDrawNode* m_bar = nullptr;
        float m_gutter = 0.f;
        float m_grab = 0.f;
        float m_barScroll = -1.f;
        int m_barState = -1;
        geode::ScrollLayer* m_clip = nullptr;
        float m_viewW = 0.f;
        float m_viewH = 0.f;
        float m_contentH = 0.f;
        float m_scroll = 0.f;
        float m_velocity = 0.f;
        float m_slop = 8.f;
        bool m_virtualize = true;
        bool m_inertia = false;
        bool m_spring = false;
        Mode m_mode = Mode::Idle;
        int m_active = -1;
        Control::Touch m_touchKind = Control::Touch::None;
        CCPoint m_startWorld;
        CCPoint m_lastWorld;
        double m_lastTime = 0.0;
    };

    class ChipStrip {
    public:
        void build(CCNode* parent, Host* host, float x, float y, float w, float h, const std::vector<std::string>& labels, int selected, bool highlightSelected);
        void release();
        void setSelected(int index);
        void setOffset(float offset);
        float offset() const { return m_offset; }
        float maxOffset() const { return std::max(0.f, m_contentW - m_w); }
        bool scrollable() const { return m_contentW > m_w + 0.5f; }
        bool contains(const CCPoint& p) const;
        int hit(const CCPoint& p) const;
        void ensureVisible(int index);

    private:
        struct Chip {
            CCNode* node = nullptr;
            CCDrawNode* bg = nullptr;
            geode::Label* label = nullptr;
            float x = 0.f;
            float w = 0.f;
        };

        void redrawChip(size_t index);
        void layout();

        Host* m_host = nullptr;
        std::vector<Chip> m_chips;
        float m_x = 0.f;
        float m_y = 0.f;
        float m_w = 0.f;
        float m_h = 0.f;
        float m_contentW = 0.f;
        float m_offset = 0.f;
        int m_selected = -1;
        bool m_highlight = true;
    };

    class ColorState {
    public:
        float h = 0.f;
        float s = 0.f;
        float v = 1.f;
        float a = 1.f;
        bool alpha = false;
        bool rainbow = false;

        void setRGB(int r, int g, int b);
        ccColor3B rgb() const;
        std::string hex6() const;
        std::string hex8() const;
        void subscribe(void* owner, std::function<void(void*)> fn);
        void unsubscribe(void* owner);
        void notify(void* source);

    private:
        std::vector<std::pair<void*, std::function<void(void*)>>> m_subscribers;
    };

    ControlPtr makeSection(Host* host, const std::string& text);
    ControlPtr makeText(Host* host, const std::string& text, float sizeDp = 12.f, bool center = false, bool accent = false);
    ControlPtr makeSeparator(Host* host, float heightDp = 12.f);
    ControlPtr makeSpacer(Host* host, float heightDp);
    ControlPtr makeToggle(Host* host, const std::string& label, std::function<bool()> get, std::function<void(bool)> set);
    ControlPtr makeChoice(Host* host, const std::string& label, std::vector<std::string> options, std::function<int()> get, std::function<int(int)> select);
    ControlPtr makeSlider(Host* host, SliderSpec spec);
    ControlPtr makeColorRow(Host* host, ColorSpec spec);
    ControlPtr makeNumberRow(Host* host, const std::string& label, bool integer, float min, float max, std::function<float()> get, std::function<void(float)> set);
    ControlPtr makeButtons(Host* host, std::vector<std::pair<std::string, std::function<void()>>> buttons, bool danger = false);
    ControlPtr makeSelector(Host* host, const std::string& label, std::function<std::string()> text, std::function<void(std::function<void()>)> open);
    ControlPtr makeKeybind(Host* host, const std::string& label, std::function<std::string()> get, std::function<void()> onSet, std::function<void()> onClear);
    ControlPtr makeLink(Host* host, const std::string& title, std::function<std::string()> subtitle, std::function<void()> tap);
    ControlPtr makeNodeRow(Host* host, CCNode* holder, float heightDp);
    ControlPtr makeHackRow(Host* host, const std::string& hackId, const std::string& windowName);
    ControlPtr makeRailTab(Host* host, const std::string& name, Icon icon, bool active, std::function<void()> tap);

    std::vector<ControlPtr> makePickerItems(Host* host, std::shared_ptr<ColorState> state, const std::string& defaultHex, bool showRainbow);
    ControlPtr makeKeypad(Host* host, NumberKind kind, std::shared_ptr<std::string> text);
    void loadColorState(ColorState& state, const ColorSpec& spec);
    void saveColorState(const ColorState& state, const ColorSpec& spec);
    std::string formatNumber(float value, bool integer, float step);

}
