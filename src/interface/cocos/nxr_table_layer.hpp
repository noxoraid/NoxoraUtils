#pragma once
#include <Geode/Geode.hpp>
#include <functional>
#include <string>
#include <vector>

// Table layout: one draggable, collapsible, scrollable window per tab. Works with mouse and touch,
// built only with cocos2d so it behaves the same on Windows, macOS and Android.
class NXRTableLayer : public cocos2d::CCLayer {
public:
    static NXRTableLayer* get();
    static bool isOpened();
    static void open();
    static void close();

    ~NXRTableLayer() override;

protected:
    struct Row {
        std::string label;
        std::string hackId;                      // empty for action rows
        std::string desc;
        bool cheating = false;
        bool disabled = false;
        std::function<void()> onTap;             // action rows
        std::function<std::string()> text;       // dynamic label for action rows
        std::function<void()> onSettings;        // hacks with a settings popup

        geode::Label* labelNode = nullptr;
        cocos2d::CCLayerColor* bar = nullptr;
        cocos2d::CCSprite* checkOn = nullptr;
        cocos2d::CCSprite* checkOff = nullptr;
    };

    struct Win {
        std::string name;
        cocos2d::CCNode* node = nullptr;
        cocos2d::CCDrawNode* arrow = nullptr;
        cocos2d::CCLayerColor* body = nullptr;
        geode::ScrollLayer* scroll = nullptr;
        std::vector<Row> rows;
        float bodyH = 0.f;
        float contentH = 0.f;
        bool collapsed = false;
    };

    enum class Grab { None, Title, Body };

    static NXRTableLayer* create();
    bool init() override;
    void update(float dt) override;
    void registerWithTouchDispatcher() override;

    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchCancelled(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
#ifdef GEODE_IS_DESKTOP
    void scrollWheel(float y, float x) override;
#endif

    void rebuild(bool save = true);
    void buildWindow(const std::string& name, std::vector<Row> rows, int index, int total);
    void drawArrow(Win& w);
    void refreshRows();
    void refreshRow(Row& row);
    void savePositions();
    void onLongPress(float dt);

    int windowAt(const cocos2d::CCPoint& world) const;
    int rowAt(Win& w, const cocos2d::CCPoint& world) const;
    void scrollBy(Win& w, float dy, bool clamp);
    void bringToFront(int index);
    float windowHeight(const Win& w) const;
    cocos2d::CCPoint clampTopLeft(const Win& w, cocos2d::CCPoint p) const;
    std::vector<Row> settingsRows();

    std::vector<Win> m_wins;

    Grab m_grab = Grab::None;
    int m_active = -1;
    int m_row = -1;
    bool m_moved = false;
    bool m_longPressed = false;
    cocos2d::CCPoint m_start;
    cocos2d::CCPoint m_lastLocal;
    cocos2d::CCPoint m_grabOffset;
    float m_refreshTimer = 0.f;

    static NXRTableLayer* s_instance;
};
