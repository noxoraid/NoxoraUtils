#include "nxr_ui_kit.hpp"
#include <chrono>

using namespace geode::prelude;

namespace {
    double nowSeconds() {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }
}

namespace NXR::Kit {
    CCSprite* makeIcon(Icon icon, float targetSize, const ccColor3B& color) {
        CCSprite* sprite = nullptr;
        switch (icon) {
            case Icon::StarOn: sprite = CCSprite::create("NXR_uiStarOn.png"_spr); break;
            case Icon::StarOff: sprite = CCSprite::create("NXR_uiStarOff.png"_spr); break;
            case Icon::Chevron: sprite = CCSprite::create("NXR_uiChevron.png"_spr); break;
            case Icon::Close: sprite = CCSprite::create("NXR_uiClose.png"_spr); break;
            case Icon::Search: sprite = CCSprite::create("NXR_uiSearch.png"_spr); break;
            case Icon::Global: sprite = CCSprite::create("NXR_uiGlobal.png"_spr); break;
            case Icon::Player: sprite = CCSprite::create("NXR_uiPlayer.png"_spr); break;
            case Icon::Level: sprite = CCSprite::create("NXR_uiLevel.png"_spr); break;
            case Icon::Bot: sprite = CCSprite::create("NXR_uiBot.png"_spr); break;
            case Icon::Utils: sprite = CCSprite::create("NXR_uiUtils.png"_spr); break;
            case Icon::Creator: sprite = CCSprite::create("NXR_uiCreator.png"_spr); break;
            case Icon::Settings: sprite = CCSprite::create("NXR_uiSettings.png"_spr); break;
            case Icon::About: sprite = CCSprite::create("NXR_uiAbout.png"_spr); break;
            case Icon::Check: sprite = CCSprite::create("NXR_uiCheck.png"_spr); break;
            case Icon::Back: sprite = CCSprite::create("NXR_uiBack.png"_spr); break;
        }
        if (!sprite) sprite = CCSprite::create();
        const float base = std::max(sprite->getContentWidth(), sprite->getContentHeight());
        if (base > 0.f) sprite->setScale(targetSize / base);
        sprite->setColor(color);
        return sprite;
    }

    Icon iconForWindow(const std::string& name) {
        if (name == "Global") return Icon::Global;
        if (name == "Player") return Icon::Player;
        if (name == "Level") return Icon::Level;
        if (name == "Bot") return Icon::Bot;
        if (name == "Utils") return Icon::Utils;
        if (name == "Creator") return Icon::Creator;
        return Icon::Utils;
    }

    void drawRound(CCDrawNode* node, float x, float y, float w, float h, float radius, const ccColor4F& color) {
        if (!node || w <= 0.f || h <= 0.f) return;
        radius = std::clamp(radius, 0.f, std::min(w, h) * 0.5f);

        std::vector<CCPoint> points;
        if (radius < 0.5f) {
            points.push_back(CCPoint(x, y));
            points.push_back(CCPoint(x + w, y));
            points.push_back(CCPoint(x + w, y + h));
            points.push_back(CCPoint(x, y + h));
        } else {
            constexpr int segments = 6;
            constexpr float pi = 3.14159265f;
            auto arc = [&](float cx, float cy, float start) {
                for (int i = 0; i <= segments; i++) {
                    const float angle = start + (pi * 0.5f) * static_cast<float>(i) / static_cast<float>(segments);
                    points.push_back(CCPoint(cx + radius * std::cos(angle), cy + radius * std::sin(angle)));
                }
            };
            arc(x + w - radius, y + radius, -pi * 0.5f);
            arc(x + w - radius, y + h - radius, 0.f);
            arc(x + radius, y + h - radius, pi * 0.5f);
            arc(x + radius, y + radius, pi);
        }

        node->drawPolygon(points.data(), static_cast<unsigned int>(points.size()), color, 0.f, ccc4f(0.f, 0.f, 0.f, 0.f));
    }

    void drawCircle(CCDrawNode* node, float cx, float cy, float radius, const ccColor4F& color) {
        if (!node || radius <= 0.f) return;
        constexpr int segments = 20;
        constexpr float pi = 3.14159265f;
        std::vector<CCPoint> points;
        for (int i = 0; i < segments; i++) {
            const float angle = 2.f * pi * static_cast<float>(i) / static_cast<float>(segments);
            points.push_back(CCPoint(cx + radius * std::cos(angle), cy + radius * std::sin(angle)));
        }
        node->drawPolygon(points.data(), static_cast<unsigned int>(points.size()), color, 0.f, ccc4f(0.f, 0.f, 0.f, 0.f));
    }

    float lineHeightUnit() {
        static float unit = 0.f;
        if (unit <= 0.f) {
            auto* sample = geode::Label::create("Ag", "GoogleSans.fnt"_spr);
            unit = sample ? sample->getContentHeight() : 0.f;
            if (unit < 1.f) unit = 20.f;
        }
        return unit;
    }

    geode::Label* makeLabel(const std::string& text, float linePt, const ccColor3B& color, const CCPoint& anchor) {
        auto* label = geode::Label::create(text, "GoogleSans.fnt"_spr);
        label->setAnchorPoint(anchor);
        label->setScale(linePt / lineHeightUnit());
        label->setColor(color);
        return label;
    }

    void fitLabel(geode::Label* label, float maxWidth) {
        if (!label || maxWidth <= 0.f) return;
        const float width = label->getScaledContentWidth();
        if (width > maxWidth) label->setScale(label->getScale() * (maxWidth / width));
    }

    std::string wrapText(const std::string& text, float maxWidth, float linePt) {
        static float average = 0.f;
        if (average <= 0.f) {
            const std::string sample = "The quick brown fox jumps over the lazy dog 0123456789";
            auto* label = geode::Label::create(sample, "GoogleSans.fnt"_spr);
            average = label ? label->getContentWidth() / static_cast<float>(sample.size()) : 10.f;
            if (average < 1.f) average = 10.f;
        }

        const float charWidth = average * (linePt / lineHeightUnit());
        const size_t wrapAt = std::max<size_t>(8, static_cast<size_t>(maxWidth * 0.92f / charWidth));

        std::string wrapped;
        std::string line;
        std::string word;
        auto flushWord = [&]() {
            if (word.empty()) return;
            if (!line.empty() && line.size() + 1 + word.size() > wrapAt) {
                wrapped += line + "\n";
                line.clear();
            }
            if (!line.empty()) line += " ";
            line += word;
            word.clear();
        };

        for (char c : text) {
            if (c == '\n') {
                flushWord();
                wrapped += line + "\n";
                line.clear();
            } else if (c == ' ') {
                flushWord();
            } else {
                word.push_back(c);
            }
        }
        flushWord();
        wrapped += line;
        return wrapped;
    }

    int countLines(const std::string& text) {
        int lines = 1;
        for (char c : text) {
            if (c == '\n') lines++;
        }
        return lines;
    }

    PanelList* PanelList::create(const CCSize& size, bool virtualize) {
        auto* ret = new PanelList();
        if (ret->init(size, virtualize)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool PanelList::init(const CCSize& size, bool virtualize) {
        if (!CCNode::init()) return false;

        m_viewW = size.width;
        m_viewH = size.height;
        m_contentH = size.height;
        m_virtualize = virtualize;

        this->setContentSize(size);
        this->setAnchorPoint(CCPoint(0.f, 0.f));

        m_clip = geode::ScrollLayer::create(size, false, true);
        if (!m_clip) return false;
        m_clip->setTouchEnabled(false);
        m_clip->ignoreAnchorPointForPosition(false);
        m_clip->setAnchorPoint(CCPoint(0.f, 0.f));
        m_clip->setPosition(CCPoint(0.f, 0.f));
        m_clip->m_contentLayer->setAnchorPoint(CCPoint(0.f, 0.f));
        this->addChild(m_clip);

        this->scheduleUpdate();
        return true;
    }

    float PanelList::maxScroll() const {
        return std::max(0.f, m_contentH - m_viewH);
    }

    void PanelList::clearEntries() {
        for (auto& entry : m_entries) destroyEntry(entry);
        m_entries.clear();
    }

    void PanelList::setItems(std::vector<ControlPtr> items, float topPad, float bottomPad) {
        m_mode = Mode::Idle;
        m_active = -1;
        m_inertia = false;
        m_spring = false;
        m_velocity = 0.f;
        m_scroll = 0.f;

        clearEntries();

        float y = topPad;
        for (auto& item : items) {
            Entry entry;
            entry.control = std::move(item);
            entry.control->setWidth(m_viewW);
            entry.height = entry.control->height();
            entry.top = y;
            y += entry.height;
            m_entries.push_back(std::move(entry));
        }

        m_contentH = std::max(y + bottomPad, m_viewH);
        m_clip->m_contentLayer->setContentSize(CCSize(m_viewW, m_contentH));
        applyScrollPosition();
        refreshVisible();
    }

    void PanelList::buildEntry(Entry& entry) {
        if (entry.view) return;

        auto* node = CCNode::create();
        node->setAnchorPoint(CCPoint(0.f, 0.f));
        node->setContentSize(CCSize(m_viewW, entry.height));
        node->setPosition(CCPoint(0.f, m_contentH - entry.top - entry.height));
        m_clip->m_contentLayer->addChild(node);
        entry.view = node;
        entry.control->build(node, m_viewW);
    }

    void PanelList::destroyEntry(Entry& entry) {
        if (!entry.view) return;
        entry.control->release();
        entry.view->removeFromParent();
        entry.view = nullptr;
    }

    void PanelList::applyScrollPosition() {
        m_clip->m_contentLayer->setPosition(CCPoint(0.f, m_viewH - m_contentH + m_scroll));
    }

    void PanelList::refreshVisible() {
        const float margin = m_virtualize ? 90.f : 1e9f;
        const float top = m_scroll - margin;
        const float bottom = m_scroll + m_viewH + margin;
        for (auto& entry : m_entries) {
            const bool visible = entry.top + entry.height > top && entry.top < bottom;
            if (visible) buildEntry(entry);
            else destroyEntry(entry);
        }
    }

    void PanelList::setScroll(float value) {
        m_scroll = std::clamp(value, 0.f, maxScroll());
        applyScrollPosition();
        refreshVisible();
    }

    bool PanelList::containsWorld(const CCPoint& world) {
        const CCPoint local = this->convertToNodeSpace(world);
        return local.x >= 0.f && local.y >= 0.f && local.x <= m_viewW && local.y <= m_viewH;
    }

    int PanelList::entryAt(const CCPoint& world) const {
        const CCPoint local = m_clip->m_contentLayer->convertToNodeSpace(world);
        for (size_t i = 0; i < m_entries.size(); i++) {
            const auto& entry = m_entries[i];
            const float bottom = m_contentH - entry.top - entry.height;
            if (local.y >= bottom && local.y < bottom + entry.height) return static_cast<int>(i);
        }
        return -1;
    }

    CCPoint PanelList::itemLocal(const CCPoint& world, const Entry& entry) const {
        const CCPoint local = m_clip->m_contentLayer->convertToNodeSpace(world);
        return CCPoint(local.x, local.y - (m_contentH - entry.top - entry.height));
    }

    bool PanelList::passthroughAt(const CCPoint& world) {
        if (!containsWorld(world)) return false;
        const int index = entryAt(world);
        if (index < 0) return false;
        auto& entry = m_entries[static_cast<size_t>(index)];
        if (!entry.view) return false;
        return entry.control->passthrough(itemLocal(world, entry));
    }

    bool PanelList::onTouchBegan(const CCPoint& world) {
        m_velocity = 0.f;
        m_inertia = false;
        m_spring = false;
        m_startWorld = world;
        m_lastWorld = world;
        m_lastTime = nowSeconds();
        m_mode = Mode::Pending;
        m_active = -1;
        m_touchKind = Control::Touch::None;

        const int index = entryAt(world);
        if (index >= 0) {
            auto& entry = m_entries[static_cast<size_t>(index)];
            if (entry.view) {
                const auto kind = entry.control->touchBegan(itemLocal(world, entry));
                if (kind != Control::Touch::None) {
                    m_active = index;
                    m_touchKind = kind;
                    if (kind == Control::Touch::Drag) m_mode = Mode::Captured;
                }
            }
        }
        return true;
    }

    void PanelList::onTouchMoved(const CCPoint& world) {
        if (m_mode == Mode::Idle) return;

        if (m_mode == Mode::Captured) {
            if (m_active >= 0 && m_active < static_cast<int>(m_entries.size())) {
                auto& entry = m_entries[static_cast<size_t>(m_active)];
                if (entry.view) entry.control->touchMoved(itemLocal(world, entry));
            }
            return;
        }

        if (m_mode == Mode::Pending) {
            if (ccpDistance(world, m_startWorld) <= m_slop) return;

            if (m_active >= 0 && m_active < static_cast<int>(m_entries.size())) {
                auto& entry = m_entries[static_cast<size_t>(m_active)];
                if (entry.view && entry.control->wantsDrag(itemLocal(m_startWorld, entry), itemLocal(world, entry))) {
                    m_mode = Mode::Captured;
                    entry.control->touchMoved(itemLocal(world, entry));
                    return;
                }
                if (entry.view) entry.control->touchCancelled();
            }

            m_active = -1;
            m_mode = Mode::Scrolling;
            m_lastWorld = world;
            m_lastTime = nowSeconds();
            return;
        }

        const double now = nowSeconds();
        const float rawDelta = world.y - m_lastWorld.y;
        float delta = rawDelta;
        const float limit = maxScroll();
        if (m_scroll < 0.f || m_scroll > limit) delta *= 0.4f;

        m_scroll += delta;
        applyScrollPosition();
        refreshVisible();

        const double dt = std::max(now - m_lastTime, 0.001);
        const float instant = static_cast<float>(static_cast<double>(rawDelta) / dt);
        m_velocity = m_velocity * 0.6f + instant * 0.4f;
        m_lastWorld = world;
        m_lastTime = now;
    }

    void PanelList::onTouchEnded(const CCPoint& world) {
        const Mode mode = m_mode;
        m_mode = Mode::Idle;

        if (mode == Mode::Captured) {
            if (m_active >= 0 && m_active < static_cast<int>(m_entries.size())) {
                auto& entry = m_entries[static_cast<size_t>(m_active)];
                if (entry.view) entry.control->touchEnded(itemLocal(world, entry), false);
            }
            m_active = -1;
            return;
        }

        if (mode == Mode::Pending) {
            if (m_active >= 0 && m_active < static_cast<int>(m_entries.size())) {
                auto& entry = m_entries[static_cast<size_t>(m_active)];
                if (entry.view) entry.control->touchEnded(itemLocal(world, entry), true);
            }
            m_active = -1;
            return;
        }

        if (mode == Mode::Scrolling) {
            if (nowSeconds() - m_lastTime > 0.08) m_velocity = 0.f;
            m_velocity = std::clamp(m_velocity, -4200.f, 4200.f);

            if (m_scroll < 0.f || m_scroll > maxScroll()) {
                m_spring = true;
                m_inertia = false;
            } else if (std::fabs(m_velocity) > 60.f) {
                m_inertia = true;
            } else {
                m_velocity = 0.f;
            }
        }
        m_active = -1;
    }

    void PanelList::onTouchCancelled() {
        const Mode mode = m_mode;
        m_mode = Mode::Idle;

        if ((mode == Mode::Pending || mode == Mode::Captured) && m_active >= 0 && m_active < static_cast<int>(m_entries.size())) {
            auto& entry = m_entries[static_cast<size_t>(m_active)];
            if (entry.view) entry.control->touchCancelled();
        }
        if (mode == Mode::Scrolling && (m_scroll < 0.f || m_scroll > maxScroll())) m_spring = true;
        m_active = -1;
    }

    void PanelList::update(float dt) {
        if (dt > 0.1f) dt = 0.1f;

        if (m_mode != Mode::Scrolling) {
            if (m_spring) {
                const float target = std::clamp(m_scroll, 0.f, maxScroll());
                m_scroll += (target - m_scroll) * std::min(1.f, dt * 14.f);
                if (std::fabs(target - m_scroll) < 0.3f) {
                    m_scroll = target;
                    m_spring = false;
                }
                applyScrollPosition();
                refreshVisible();
            } else if (m_inertia) {
                m_scroll += m_velocity * dt;
                m_velocity *= std::exp(-3.4f * dt);

                const float limit = maxScroll();
                if (m_scroll < 0.f || m_scroll > limit) {
                    m_scroll = std::clamp(m_scroll, 0.f, limit);
                    m_velocity = 0.f;
                    m_inertia = false;
                }
                if (std::fabs(m_velocity) < 8.f) {
                    m_velocity = 0.f;
                    m_inertia = false;
                }
                applyScrollPosition();
                refreshVisible();
            }
        }

        for (auto& entry : m_entries) {
            if (entry.view) entry.control->tick(dt);
        }
    }

    void ChipStrip::release() {
        for (auto& chip : m_chips) {
            if (chip.node) chip.node->removeFromParent();
        }
        m_chips.clear();
    }

    void ChipStrip::build(CCNode* parent, Host* host, float x, float y, float w, float h, const std::vector<std::string>& labels, int selected, bool highlightSelected) {
        release();
        m_host = host;
        m_x = x;
        m_y = y;
        m_w = w;
        m_h = h;
        m_selected = selected;
        m_highlight = highlightSelected;
        m_offset = 0.f;

        const float gap = host->dp(8.f);
        float cursor = 0.f;
        for (const auto& text : labels) {
            Chip chip;
            chip.node = CCNode::create();
            chip.node->setAnchorPoint(CCPoint(0.f, 0.f));
            chip.label = makeLabel(text, host->dp(15.f), Pal::text(), CCPoint(0.5f, 0.5f));
            chip.w = std::max(chip.label->getScaledContentWidth() + host->dp(24.f), host->dp(48.f));
            chip.node->setContentSize(CCSize(chip.w, h));
            chip.bg = CCDrawNode::create();
            chip.node->addChild(chip.bg);
            chip.label->setPosition(CCPoint(chip.w * 0.5f, h * 0.5f));
            chip.node->addChild(chip.label, 1);
            chip.x = cursor;
            cursor += chip.w + gap;
            parent->addChild(chip.node);
            m_chips.push_back(chip);
        }
        m_contentW = std::max(0.f, cursor - gap);

        for (size_t i = 0; i < m_chips.size(); i++) redrawChip(i);
        layout();
    }

    void ChipStrip::redrawChip(size_t index) {
        auto& chip = m_chips[index];
        const bool on = m_highlight && static_cast<int>(index) == m_selected;
        chip.bg->clear();
        drawRound(chip.bg, 0.f, 0.f, chip.w, m_h, m_h * 0.5f, on ? fromColor(Pal::accent()) : fillColor(31, 39, 55));
        chip.label->setColor(on ? Pal::onAccent() : Pal::text());
    }

    void ChipStrip::layout() {
        for (auto& chip : m_chips) {
            const float left = chip.x - m_offset;
            const bool visible = left >= -0.5f && left + chip.w <= m_w + 0.5f;
            chip.node->setPosition(CCPoint(m_x + left, m_y));
            chip.node->setVisible(visible);
        }
    }

    void ChipStrip::setSelected(int index) {
        m_selected = index;
        for (size_t i = 0; i < m_chips.size(); i++) redrawChip(i);
    }

    void ChipStrip::setOffset(float offset) {
        m_offset = std::clamp(offset, 0.f, maxOffset());
        layout();
    }

    void ChipStrip::ensureVisible(int index) {
        if (index < 0 || index >= static_cast<int>(m_chips.size())) return;
        const auto& chip = m_chips[static_cast<size_t>(index)];
        if (chip.x < m_offset) setOffset(chip.x);
        else if (chip.x + chip.w > m_offset + m_w) setOffset(chip.x + chip.w - m_w);
    }

    bool ChipStrip::contains(const CCPoint& p) const {
        return p.x >= m_x && p.x <= m_x + m_w && p.y >= m_y && p.y <= m_y + m_h;
    }

    int ChipStrip::hit(const CCPoint& p) const {
        if (!contains(p)) return -1;
        for (size_t i = 0; i < m_chips.size(); i++) {
            const auto& chip = m_chips[i];
            const float left = m_x + chip.x - m_offset;
            if (p.x >= left && p.x <= left + chip.w) {
                if (!chip.node || !chip.node->isVisible()) return -1;
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void ColorState::setRGB(int r, int g, int b) {
        const float rf = static_cast<float>(std::clamp(r, 0, 255)) / 255.f;
        const float gf = static_cast<float>(std::clamp(g, 0, 255)) / 255.f;
        const float bf = static_cast<float>(std::clamp(b, 0, 255)) / 255.f;
        const float high = std::max({rf, gf, bf});
        const float low = std::min({rf, gf, bf});
        const float delta = high - low;

        v = high;
        s = high <= 0.f ? 0.f : delta / high;
        if (delta > 0.0001f) {
            float hue;
            if (high == rf) hue = std::fmod((gf - bf) / delta, 6.f);
            else if (high == gf) hue = (bf - rf) / delta + 2.f;
            else hue = (rf - gf) / delta + 4.f;
            hue *= 60.f;
            if (hue < 0.f) hue += 360.f;
            h = hue;
        }
    }

    ccColor3B ColorState::rgb() const {
        float hue = std::fmod(h, 360.f);
        if (hue < 0.f) hue += 360.f;
        const float c = v * s;
        const float sector = hue / 60.f;
        const float x = c * (1.f - std::fabs(std::fmod(sector, 2.f) - 1.f));
        const float m = v - c;
        float r = 0.f;
        float g = 0.f;
        float b = 0.f;
        if (sector < 1.f) { r = c; g = x; }
        else if (sector < 2.f) { r = x; g = c; }
        else if (sector < 3.f) { g = c; b = x; }
        else if (sector < 4.f) { g = x; b = c; }
        else if (sector < 5.f) { r = x; b = c; }
        else { r = c; b = x; }
        return ccc3(static_cast<GLubyte>(std::clamp(std::round((r + m) * 255.f), 0.f, 255.f)),
            static_cast<GLubyte>(std::clamp(std::round((g + m) * 255.f), 0.f, 255.f)),
            static_cast<GLubyte>(std::clamp(std::round((b + m) * 255.f), 0.f, 255.f)));
    }

    std::string ColorState::hex6() const {
        const ccColor3B c = rgb();
        return fmt::format("{:02X}{:02X}{:02X}", c.r, c.g, c.b);
    }

    std::string ColorState::hex8() const {
        const ccColor3B c = rgb();
        const int alphaByte = static_cast<int>(std::clamp(std::round(a * 255.f), 0.f, 255.f));
        return fmt::format("{:02X}{:02X}{:02X}{:02X}", c.r, c.g, c.b, alphaByte);
    }

    void ColorState::subscribe(void* owner, std::function<void(void*)> fn) {
        unsubscribe(owner);
        m_subscribers.emplace_back(owner, std::move(fn));
    }

    void ColorState::unsubscribe(void* owner) {
        m_subscribers.erase(
            std::remove_if(m_subscribers.begin(), m_subscribers.end(), [owner](const auto& entry) { return entry.first == owner; }),
            m_subscribers.end());
    }

    void ColorState::notify(void* source) {
        auto copy = m_subscribers;
        for (auto& entry : copy) {
            if (entry.first != source) entry.second(source);
        }
    }
}
