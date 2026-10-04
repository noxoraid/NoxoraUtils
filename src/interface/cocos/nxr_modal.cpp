#include "nxr_modal.hpp"
#include "nxr_ui_kit.hpp"
#include "nxr_menu.hpp"
#include "nxr_text_style.hpp"

using namespace geode::prelude;
using namespace NXR::Kit;

namespace {
    geode::Label* styledLabel(const std::string& text, float scale, const ccColor3B& color, const CCPoint& anchor) {
        auto* label = geode::Label::create(text, "GoogleSans.fnt"_spr);
        label->setScale(scale);
        label->setColor(color);
        label->setAnchorPoint(anchor);
        label->setUserObject(NXR::Ui::kStyledMarker, CCString::create("1"));
        return label;
    }

    void fitWidth(geode::Label* label, float maxWidth) {
        if (label->getScaledContentWidth() > maxWidth && label->getContentWidth() > 0.f) {
            label->setScale(maxWidth / label->getContentWidth());
        }
    }
}

void NXR::Modal::skin(const PopupParts& parts, const std::string& title, float width, float height) {
    if (!parts.mainLayer) return;

    if (parts.bgSprite) parts.bgSprite->setVisible(false);

    auto* bg = CCDrawNode::create();
    const float radius = 18.f;
    drawRound(bg, -1.5f, -1.5f, width + 3.f, height + 3.f, radius + 1.5f, fromColor(Pal::accent(), 0.5f));
    if (Pal::gradientOn()) {
        drawGradient(bg, 0.f, 0.f, width, height, radius, fromColor(Pal::panel(), 0.98f), mixColor(Pal::panel(), Pal::accent2(), 0.38f, 0.98f), Pal::gradientHorizontal());
    } else {
        drawRound(bg, 0.f, 0.f, width, height, radius, fromColor(Pal::panel(), 0.98f));
    }
    drawRound(bg, 16.f, height - 41.f, width - 32.f, 1.f, 0.f, fillColor(44, 58, 82));
    parts.mainLayer->addChild(bg, -5);

    auto* heading = styledLabel(title, 0.62f, Pal::text(), CCPoint(0.f, 0.5f));
    heading->setPosition({20.f, height - 21.f});
    fitWidth(heading, width - 76.f);
    parts.mainLayer->addChild(heading, 5);

    if (parts.closeBtn) {
        parts.closeBtn->setSprite(makeIcon(Icon::Close, 16.f, Pal::muted()));
        if (parts.buttonMenu) {
            const CCPoint world = parts.mainLayer->convertToWorldSpace(CCPoint(width - 24.f, height - 21.f));
            parts.closeBtn->setPosition(parts.buttonMenu->convertToNodeSpace(world));
        }
    }
}

CCMenuItemSpriteExtra* NXR::Modal::button(const std::string& text, float width, float height, bool primary, std::function<void()> callback) {
    auto* holder = CCNode::create();
    holder->setContentSize({width, height});

    auto* draw = CCDrawNode::create();
    if (primary) {
        drawAccent(draw, 0.f, 0.f, width, height, height * 0.34f);
    } else {
        drawRound(draw, 0.f, 0.f, width, height, height * 0.34f, fillColor(58, 72, 98));
        drawRound(draw, 1.f, 1.f, width - 2.f, height - 2.f, height * 0.34f - 1.f, fillColor(30, 41, 60));
    }
    holder->addChild(draw);

    auto* label = styledLabel(text, 0.58f, primary ? Pal::onAccent() : Pal::text(), CCPoint(0.5f, 0.5f));
    label->setPosition({width * 0.5f, height * 0.5f});
    fitWidth(label, width - 16.f);
    holder->addChild(label, 1);

    return geode::cocos::CCMenuItemExt::createSpriteExtra(holder, [callback = std::move(callback)](CCMenuItemSpriteExtra*) {
        if (callback) callback();
    });
}

CCNode* NXR::Modal::iconNode(float size) {
    return makeIcon(Icon::Info, size, Pal::accent());
}

std::vector<std::string> NXR::Modal::wrap(const std::string& text, size_t maxChars) {
    std::vector<std::string> lines;
    std::string line;
    std::string word;
    auto flushWord = [&] {
        if (word.empty()) return;
        if (!line.empty() && line.size() + 1 + word.size() > maxChars) {
            lines.push_back(line);
            line.clear();
        }
        if (!line.empty()) line.push_back(' ');
        line += word;
        word.clear();
    };
    for (char c : text) {
        if (c == '\n') {
            flushWord();
            lines.push_back(line);
            line.clear();
        } else if (c == ' ') {
            flushWord();
        } else {
            word.push_back(c);
        }
    }
    flushWord();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

NXR::Modal::Row* NXR::Modal::Row::create(const std::string& text, float width, float height, bool selected) {
    auto* ret = new Row();
    if (ret->init(text, width, height, selected)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool NXR::Modal::Row::init(const std::string& text, float width, float height, bool selected) {
    if (!CCNode::init()) return false;
    m_w = width;
    m_h = height;
    m_selected = selected;
    this->setContentSize({width, height});

    m_bg = CCDrawNode::create();
    this->addChild(m_bg);

    m_label = styledLabel(text, 0.55f, Pal::text(), CCPoint(0.f, 0.5f));
    m_label->setPosition({14.f, height * 0.5f});
    fitWidth(m_label, width - 28.f);
    this->addChild(m_label, 1);

    redraw();
    return true;
}

void NXR::Modal::Row::setSelected(bool value) {
    if (m_selected == value) return;
    m_selected = value;
    redraw();
}

void NXR::Modal::Row::redraw() {
    m_bg->clear();
    if (m_selected) {
        drawAccent(m_bg, 0.f, 0.f, m_w, m_h, m_h * 0.3f);
        m_label->setColor(Pal::onAccent());
    } else {
        drawRound(m_bg, 0.f, 0.f, m_w, m_h, m_h * 0.3f, fillColor(24, 33, 49));
        m_label->setColor(Pal::text());
    }
}
