#pragma once
#include <Geode/Geode.hpp>
#include <functional>
#include <string>
#include <vector>
#include "../../core/nxr_hacks.hpp"

class NXRTableLayer : public cocos2d::CCLayer {
public:
    static NXRTableLayer* get();
    static bool isOpened();
    static void open();
    static void close();

    static void openHackSettings(NXR::Hack& hack, const std::string& origin = "");
    static void openUiSettings(const std::string& origin = "");
    static bool popupBlocking();

    ~NXRTableLayer() override;

protected:
    struct Row {
        std::string label;
        std::string hackId;
        std::string desc;
        bool cheating = false;
        bool disabled = false;
        float width = 160.f;
        std::function<void()> onTap;
        std::function<std::string()> text;
        std::function<void()> onSettings;

        geode::Label* labelNode = nullptr;
        cocos2d::CCSprite* checkOn = nullptr;
        cocos2d::CCSprite* checkOff = nullptr;
    };

    enum class Kind { Rows, BotPanel, HackSettings, UiSettings };

    struct Spec {
        std::string name;
        std::string id;
        Kind kind = Kind::Rows;
        std::vector<Row> rows;
        NXR::Hack* hack = nullptr;
        std::string windowName;
        bool closable = false;
        float x = -1.f;
        float y = -1.f;
    };

    struct Win {
        std::string name;
        std::string id;
        Kind kind = Kind::Rows;
        NXR::Hack* hack = nullptr;
        bool closable = false;
        cocos2d::CCNode* node = nullptr;
        cocos2d::CCDrawNode* arrow = nullptr;
        cocos2d::CCLayerColor* body = nullptr;
        cocos2d::CCNode* host = nullptr;
        geode::ScrollLayer* scroll = nullptr;
        geode::Ref<cocos2d::CCObject> owner;
        std::vector<Row> rows;
        float width = 160.f;
        float bodyH = 0.f;
        float contentH = 0.f;
        bool collapsed = false;
        bool panel() const { return kind != Kind::Rows; }
    };

    enum class Grab { None, Title, Body };

    static NXRTableLayer* create();
    bool init() override;
    void onEnter() override;
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
    void buildWindow(Spec spec, int index, int total);
    void drawArrow(Win& w);
    void refreshRows();
    void refreshRow(Row& row);
    void savePositions();
    void onLongPress(float dt);
    void applyHostPriority();
    void openExtra(Spec spec, const std::string& origin);
    void closeWindow(int index);
    int findById(const std::string& id) const;

    int windowAt(const cocos2d::CCPoint& world) const;
    int rowAt(Win& w, const cocos2d::CCPoint& world) const;
    void scrollBy(Win& w, float dy, bool clamp);
    void bringToFront(int index);
    float windowHeight(const Win& w) const;
    cocos2d::CCPoint clampTopLeft(const Win& w, cocos2d::CCPoint p) const;
    std::vector<Row> settingsRows();

    std::vector<Win> m_wins;
    std::vector<Spec> m_extras;

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
