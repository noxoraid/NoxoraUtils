#pragma once
#include <Geode/Geode.hpp>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "nxr_ui_kit.hpp"
#include "nxr_ui_page.hpp"

class NXRHacksLayer : public cocos2d::CCLayer, public NXR::Kit::Host {
public:
    struct Zone {
        cocos2d::CCRect rect;
        std::function<void()> action;
    };

    struct Page {
        int id = 0;
        std::string title;
        std::string hackId;
        NXR::Hack* hack = nullptr;
        bool withEnable = false;
        std::function<std::vector<NXR::Kit::ControlPtr>(Page&)> build;
        std::unique_ptr<NXR::Kit::PageBuilder> builder;
        std::vector<std::pair<std::string, std::function<void()>>> footer;
        float scroll = 0.f;
    };

    ~NXRHacksLayer() override;

    static NXRHacksLayer* create();
    static NXRHacksLayer* get();
    static bool isOpened();
    static void openHack(NXR::Hack& hack);
    static void applyQuery(const std::string& text);

    void show();
    void onClose(cocos2d::CCObject* object);
    void setQuery(const std::string& text);
    void rebuildUiLater();
    void redrawBackground();

    float dp(float value) const override { return value * m_unit; }
    void openColorPicker(const NXR::Kit::ColorSpec& spec, std::function<void()> onDone) override;
    void openNumberInput(const std::string& title, const std::string& hint, const std::string& current, NXR::Kit::NumberKind kind, std::function<void(const std::string&)> onApply) override;
    void openHackSheet(NXR::Hack& hack) override;
    void openInfoSheet(NXR::Hack& hack) override;
    void favoritesChanged() override;
    void statusChanged() override;

    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchCancelled(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void registerWithTouchDispatcher() override;
    void keyBackClicked() override;
    void update(float dt) override;

private:
    enum class Target {
        None,
        Outside,
        Scrim,
        Zone,
        Rail,
        Body,
        Sheet,
    };

    bool init();
    void buildUi();
    void buildHeader();
    void buildRail();
    void refreshBody(bool keepScroll);
    std::vector<NXR::Kit::ControlPtr> tabItems();
    std::vector<std::string> tabNames();
    void switchTab(const std::string& name);
    std::vector<std::string> statusChips();
    void fillSettings(NXR::Kit::PageBuilder& b);
    void fillAbout(NXR::Kit::PageBuilder& b);
    void fillKeybinds(NXR::Kit::PageBuilder& b);

    void pushPage(std::shared_ptr<Page> page);
    void popPage();
    void renderSheet();
    void rebuildPage(int id);
    void closeSheet();
    void finishSheetClose();
    void setSheetOnly(bool value);
    std::shared_ptr<Page> makeHackPage(NXR::Hack& hack, bool withForm);
    void openKeybinds();

    cocos2d::CCPoint sheetTarget() const;
    float sheetWidth() const;
    cocos2d::CCRect panelRect() const;
    cocos2d::CCRect sheetRect() const;
    int zoneAt(const std::vector<Zone>& zones, const cocos2d::CCPoint& p) const;
    void closeLater();
    void defer(std::function<void()> fn);
    std::function<void()> guarded(std::function<void()> fn);

    static NXRHacksLayer* instance;

    float m_unit = 1.f;
    float m_px = 0.f;
    float m_py = 0.f;
    float m_pw = 0.f;
    float m_ph = 0.f;

    cocos2d::CCNode* m_root = nullptr;
    cocos2d::CCNode* m_panel = nullptr;
    cocos2d::CCDrawNode* m_bgDraw = nullptr;
    cocos2d::CCNode* m_header = nullptr;
    NXR::Kit::ListView* m_rail = nullptr;
    NXR::Kit::ListView* m_body = nullptr;

    cocos2d::CCNode* m_sheet = nullptr;
    cocos2d::CCNode* m_sheetContent = nullptr;
    NXR::Kit::ListView* m_sheetList = nullptr;
    std::vector<std::shared_ptr<Page>> m_pages;
    std::vector<Zone> m_zones;
    std::vector<Zone> m_sheetZones;
    std::unique_ptr<NXR::Kit::PageBuilder> m_tabBuilder;
    int m_nextPageId = 1;

    std::string m_tab;
    std::string m_query;
    std::vector<std::string> m_lastChips;
    float m_chipTimer = 0.f;
    bool m_sheetOnly = false;
    bool m_sheetClosing = false;
    bool m_closed = false;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);

    cocos2d::CCTouch* m_touch = nullptr;
    Target m_target = Target::None;
    int m_zoneIndex = -1;
    bool m_zoneInSheet = false;
    bool m_moved = false;
    cocos2d::CCPoint m_start;
};
