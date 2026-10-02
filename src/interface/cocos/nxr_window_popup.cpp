#include "nxr_window_popup.hpp"
#include "../../core/nxr_theme.hpp"
#include "nxr_text_style.hpp"

using namespace geode::prelude;

NXRWindowPopup* NXRWindowPopup::create(const std::string& title, const std::function<void(NXRHacksTab*)>& build) {
    auto* ret = new NXRWindowPopup();
    if (ret->init(title, build)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool NXRWindowPopup::init(const std::string& title, const std::function<void(NXRHacksTab*)>& build) {
    if (!Popup::init(400.f, 290.f, NXR::Theme::square())) return false;

    const auto size = m_mainLayer->getContentSize();

    auto* closeSprite = CCSprite::create("NXR_closeBtn.png"_spr);
    closeSprite->setScale(0.75f);
    m_closeBtn->setSprite(closeSprite);

    auto* label = geode::Label::create(title, "GoogleSans.fnt"_spr);
    label->setPosition({size.width / 2.f, size.height - 14.f});
    label->setScale(0.6f);
    m_mainLayer->addChild(label);

    auto* tab = NXRHacksTab::create();
    tab->m_scrollLayer->setPosition({(size.width - 360.f) / 2.f, 5.f});
    m_mainLayer->addChild(tab);

    if (build) build(tab);

    tab->m_scrollLayer->m_contentLayer->updateLayout();
    tab->m_scrollLayer->moveToTop();

    NXR::Ui::applyTextStyle(m_mainLayer);
    return true;
}
