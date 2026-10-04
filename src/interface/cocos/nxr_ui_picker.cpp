#include "nxr_ui_kit.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_ui_mode.hpp"
#include "../../core/nxr_utils.hpp"

using namespace geode::prelude;

namespace NXR::Kit {
    void loadColorState(ColorState& state, const ColorSpec& spec) {
        const std::string hex = NXRConfig::get().get<std::string>(spec.key, spec.defaultHex);
        state.alpha = spec.alpha;
        if (spec.alpha) {
            const ccColor4F c = NXR::Utils::hexToColor4F(hex);
            state.setRGB(static_cast<int>(std::round(c.r * 255.f)), static_cast<int>(std::round(c.g * 255.f)), static_cast<int>(std::round(c.b * 255.f)));
            state.a = c.a;
        } else {
            const ccColor3B c = NXR::Utils::hexToColor(hex);
            state.setRGB(c.r, c.g, c.b);
            state.a = 1.f;
        }
        state.rainbow = spec.rainbowKey.empty() ? false : NXRConfig::get().get<bool>(spec.rainbowKey, false);
    }

    void saveColorState(const ColorState& state, const ColorSpec& spec) {
        NXRConfig::get().set<std::string>(spec.key, spec.alpha ? state.hex8() : state.hex6());
        if (!spec.rainbowKey.empty()) NXRConfig::get().set<bool>(spec.rainbowKey, state.rainbow);
        NXR::Ui::pushRecentColor(state.hex8());
    }

    namespace {
        void drawChecker(CCDrawNode* node, float x, float y, float w, float h, float cell) {
            const int cols = std::max(1, static_cast<int>(std::ceil(w / cell)));
            const int rows = std::max(1, static_cast<int>(std::ceil(h / cell)));
            for (int r = 0; r < rows; r++) {
                for (int c = 0; c < cols; c++) {
                    const bool light = (r + c) % 2 == 0;
                    const float cx = x + static_cast<float>(c) * cell;
                    const float cy = y + static_cast<float>(r) * cell;
                    const float cw = std::min(cell, x + w - cx);
                    const float ch = std::min(cell, y + h - cy);
                    if (cw <= 0.f || ch <= 0.f) continue;
                    drawRound(node, cx, cy, cw, ch, 0.f, light ? fillColor(200, 204, 212) : fillColor(130, 136, 148));
                }
            }
        }

        ccColor3B hueColor(float hue) {
            ColorState tmp;
            tmp.h = hue;
            tmp.s = 1.f;
            tmp.v = 1.f;
            return tmp.rgb();
        }

        class PreviewControl final : public Control {
            std::shared_ptr<ColorState> m_state;
            CCDrawNode* m_draw = nullptr;
            geode::Label* m_text = nullptr;

            void redraw() {
                if (!m_draw) return;
                m_draw->clear();
                drawRound(m_draw, dp(8.f), dp(3.f), m_width - dp(16.f), height() - dp(6.f), dp(10.f), fillColor(21, 28, 41));
                const float sw = dp(120.f);
                const float sh = height() - dp(22.f);
                const float sx = dp(18.f);
                const float sy = dp(11.f);
                drawChecker(m_draw, sx, sy, sw, sh, dp(10.f));
                const float alpha = m_state->alpha ? m_state->a : 1.f;
                drawRound(m_draw, sx, sy, sw, sh, 0.f, fromColor(m_state->rgb(), alpha));
                if (m_text) {
                    m_text->setString(("#" + (m_state->alpha ? m_state->hex8() : m_state->hex6())).c_str());
                    m_text->setScale(dp(20.f) / lineHeightUnit());
                }
            }
        public:
            PreviewControl(Host* host, std::shared_ptr<ColorState> state) : Control(host), m_state(std::move(state)) {}
            float height() override { return dp(76.f); }

            void build(CCNode* view, float) override {
                m_draw = CCDrawNode::create();
                view->addChild(m_draw);
                m_text = makeLabel("", dp(20.f), Pal::text(), CCPoint(0.f, 0.5f));
                m_text->setPosition(CCPoint(dp(18.f) + dp(120.f) + dp(18.f), height() * 0.5f));
                view->addChild(m_text, 2);
                m_state->subscribe(this, [this](void*) { redraw(); });
                redraw();
            }

            void release() override {
                m_state->unsubscribe(this);
                m_draw = nullptr;
                m_text = nullptr;
            }
        };

        class SVControl final : public Control {
            std::shared_ptr<ColorState> m_state;
            CCDrawNode* m_cursor = nullptr;
            CCLayerGradient* m_hue = nullptr;
            CCDrawNode* m_frame = nullptr;
            float m_x0 = 0.f;
            float m_y0 = 0.f;
            float m_w = 0.f;
            float m_h = 0.f;

            void redraw() {
                if (!m_cursor) return;
                if (m_hue) m_hue->setEndColor(hueColor(m_state->h));
                m_cursor->clear();
                const float cx = m_x0 + m_w * m_state->s;
                const float cy = m_y0 + m_h * m_state->v;
                drawCircle(m_cursor, cx, cy, dp(12.f), fillColor(0, 0, 0, 0.55f));
                drawCircle(m_cursor, cx, cy, dp(10.5f), fillColor(255, 255, 255));
                drawCircle(m_cursor, cx, cy, dp(8.f), fromColor(m_state->rgb()));
            }

            void setFrom(const CCPoint& p) {
                m_state->s = std::clamp((p.x - m_x0) / m_w, 0.f, 1.f);
                m_state->v = std::clamp((p.y - m_y0) / m_h, 0.f, 1.f);
                redraw();
                m_state->notify(this);
            }
        public:
            SVControl(Host* host, std::shared_ptr<ColorState> state) : Control(host), m_state(std::move(state)) {}
            float height() override { return dp(176.f); }

            void build(CCNode* view, float width) override {
                m_x0 = dp(18.f);
                m_y0 = dp(12.f);
                m_w = width - dp(36.f);
                m_h = height() - dp(24.f);

                m_frame = CCDrawNode::create();
                drawRound(m_frame, m_x0 - 1.5f, m_y0 - 1.5f, m_w + 3.f, m_h + 3.f, dp(4.f), fillColor(70, 84, 110));
                view->addChild(m_frame);

                m_hue = CCLayerGradient::create(ccc4(255, 255, 255, 255), ccc4(255, 0, 0, 255), CCPoint(1.f, 0.f));
                m_hue->setContentSize(CCSize(m_w, m_h));
                m_hue->setPosition(CCPoint(m_x0, m_y0));
                view->addChild(m_hue, 1);

                auto* shade = CCLayerGradient::create(ccc4(0, 0, 0, 0), ccc4(0, 0, 0, 255), CCPoint(0.f, -1.f));
                shade->setContentSize(CCSize(m_w, m_h));
                shade->setPosition(CCPoint(m_x0, m_y0));
                view->addChild(shade, 2);

                m_cursor = CCDrawNode::create();
                view->addChild(m_cursor, 3);
                m_state->subscribe(this, [this](void*) { redraw(); });
                redraw();
            }

            void release() override {
                m_state->unsubscribe(this);
                m_cursor = nullptr;
                m_hue = nullptr;
                m_frame = nullptr;
            }

            Touch touchBegan(const CCPoint& p) override {
                if (p.x < m_x0 - dp(10.f) || p.x > m_x0 + m_w + dp(10.f) || p.y < m_y0 - dp(10.f) || p.y > m_y0 + m_h + dp(10.f)) return Touch::None;
                setFrom(p);
                return Touch::Drag;
            }

            void touchMoved(const CCPoint& p) override { setFrom(p); }
            void touchEnded(const CCPoint&, bool) override {}
            bool passthrough(const CCPoint&) override { return false; }
        };

        class BarControl final : public Control {
            std::shared_ptr<ColorState> m_state;
            bool m_isAlpha;
            CCDrawNode* m_thumb = nullptr;
            CCLayerGradient* m_alphaGrad = nullptr;
            float m_x0 = 0.f;
            float m_w = 0.f;

            float value() const { return m_isAlpha ? m_state->a : m_state->h / 360.f; }

            void redraw() {
                if (!m_thumb) return;
                if (m_alphaGrad) {
                    const ccColor3B c = m_state->rgb();
                    m_alphaGrad->setStartColor(c);
                    m_alphaGrad->setEndColor(c);
                }
                m_thumb->clear();
                const float cx = m_x0 + m_w * std::clamp(value(), 0.f, 1.f);
                const float cy = height() * 0.5f - dp(4.f);
                drawCircle(m_thumb, cx, cy, dp(13.f), fillColor(255, 255, 255));
                drawCircle(m_thumb, cx, cy, dp(9.f), m_isAlpha ? fromColor(m_state->rgb(), std::max(m_state->a, 0.15f)) : fromColor(hueColor(m_state->h)));
            }

            void setFrom(const CCPoint& p) {
                const float t = std::clamp((p.x - m_x0) / m_w, 0.f, 1.f);
                if (m_isAlpha) m_state->a = t;
                else m_state->h = t * 360.f;
                redraw();
                m_state->notify(this);
            }
        public:
            BarControl(Host* host, std::shared_ptr<ColorState> state, bool isAlpha) : Control(host), m_state(std::move(state)), m_isAlpha(isAlpha) {}
            float height() override { return dp(54.f); }

            void build(CCNode* view, float width) override {
                m_x0 = dp(18.f);
                m_w = width - dp(36.f);
                const float barH = dp(18.f);
                const float barY = height() * 0.5f - dp(4.f) - barH * 0.5f;

                auto* frame = CCDrawNode::create();
                drawRound(frame, m_x0 - 1.5f, barY - 1.5f, m_w + 3.f, barH + 3.f, dp(4.f), fillColor(70, 84, 110));
                view->addChild(frame);

                if (m_isAlpha) {
                    auto* checker = CCDrawNode::create();
                    drawChecker(checker, m_x0, barY, m_w, barH, dp(9.f));
                    view->addChild(checker, 1);
                    const ccColor3B c = m_state->rgb();
                    m_alphaGrad = CCLayerGradient::create(ccc4(c.r, c.g, c.b, 0), ccc4(c.r, c.g, c.b, 255), CCPoint(1.f, 0.f));
                    m_alphaGrad->setContentSize(CCSize(m_w, barH));
                    m_alphaGrad->setPosition(CCPoint(m_x0, barY));
                    view->addChild(m_alphaGrad, 2);
                } else {
                    const float seg = m_w / 6.f;
                    for (int i = 0; i < 6; i++) {
                        auto* grad = CCLayerGradient::create(ccc4(hueColor(static_cast<float>(i) * 60.f).r, hueColor(static_cast<float>(i) * 60.f).g, hueColor(static_cast<float>(i) * 60.f).b, 255),
                            ccc4(hueColor(static_cast<float>(i + 1) * 60.f).r, hueColor(static_cast<float>(i + 1) * 60.f).g, hueColor(static_cast<float>(i + 1) * 60.f).b, 255), CCPoint(1.f, 0.f));
                        grad->setContentSize(CCSize(seg + 0.5f, barH));
                        grad->setPosition(CCPoint(m_x0 + seg * static_cast<float>(i), barY));
                        view->addChild(grad, 1);
                    }
                }

                auto* title = makeLabel(m_isAlpha ? "Alpha" : "Hue", dp(12.f), Pal::muted(), CCPoint(0.f, 0.5f));
                title->setPosition(CCPoint(m_x0, height() - dp(8.f)));
                view->addChild(title, 3);

                m_thumb = CCDrawNode::create();
                view->addChild(m_thumb, 4);
                m_state->subscribe(this, [this](void*) { redraw(); });
                redraw();
            }

            void release() override {
                m_state->unsubscribe(this);
                m_thumb = nullptr;
                m_alphaGrad = nullptr;
            }

            Touch touchBegan(const CCPoint& p) override {
                if (p.x < m_x0 - dp(16.f) || p.x > m_x0 + m_w + dp(16.f)) return Touch::None;
                setFrom(p);
                return Touch::Drag;
            }

            void touchMoved(const CCPoint& p) override { setFrom(p); }
            void touchEnded(const CCPoint&, bool) override {}
        };

        class ValuesControl final : public Control {
            std::shared_ptr<ColorState> m_state;
            CCDrawNode* m_bg = nullptr;
            std::vector<geode::Label*> m_values;
            int m_pressed = -1;

            int fieldCount() const { return m_state->alpha ? 5 : 4; }
            float fieldX(int index) const {
                const float gap = dp(8.f);
                const float hexW = dp(112.f);
                const float count = static_cast<float>(fieldCount() - 1);
                const float rest = (m_width - dp(28.f) - hexW - gap * count) / count;
                if (index == 0) return dp(14.f);
                return dp(14.f) + hexW + gap + (rest + gap) * static_cast<float>(index - 1);
            }
            float fieldW(int index) const {
                const float gap = dp(8.f);
                const float hexW = dp(112.f);
                const float count = static_cast<float>(fieldCount() - 1);
                const float rest = (m_width - dp(28.f) - hexW - gap * count) / count;
                return index == 0 ? hexW : rest;
            }
            int fieldAt(const CCPoint& p) const {
                for (int i = 0; i < fieldCount(); i++) {
                    if (p.x >= fieldX(i) - dp(4.f) && p.x <= fieldX(i) + fieldW(i) + dp(4.f)) return i;
                }
                return -1;
            }

            std::string textFor(int index) const {
                const ccColor3B c = m_state->rgb();
                switch (index) {
                    case 0: return m_state->hex6();
                    case 1: return fmt::format("{}", c.r);
                    case 2: return fmt::format("{}", c.g);
                    case 3: return fmt::format("{}", c.b);
                    default: return fmt::format("{}", static_cast<int>(std::round(m_state->a * 255.f)));
                }
            }

            void redraw() {
                if (!m_bg) return;
                m_bg->clear();
                const float y = dp(8.f);
                const float h = height() - dp(16.f) - dp(14.f);
                for (int i = 0; i < fieldCount(); i++) {
                    drawRound(m_bg, fieldX(i), y, fieldW(i), h, dp(10.f), i == m_pressed ? fromColor(Pal::accent(), 0.3f) : fillColor(11, 16, 26));
                    if (i < static_cast<int>(m_values.size())) {
                        m_values[static_cast<size_t>(i)]->setString(textFor(i).c_str());
                        m_values[static_cast<size_t>(i)]->setScale(dp(16.f) / lineHeightUnit());
                        fitLabel(m_values[static_cast<size_t>(i)], fieldW(i) - dp(8.f));
                    }
                }
            }

            void edit(int index) {
                const bool hex = index == 0;
                const std::string title = hex ? std::string("HEX") : std::string(index == 1 ? "Red" : (index == 2 ? "Green" : (index == 3 ? "Blue" : "Alpha")));
                auto state = m_state;
                host()->openNumberInput(title, hex ? "RRGGBB" : "0 to 255", textFor(index), hex ? NumberKind::Hex : NumberKind::Integer, [state, index](const std::string& text) {
                    if (index == 0) {
                        std::string hex = text;
                        while (hex.size() < 6) hex.insert(hex.begin(), '0');
                        if (hex.size() > 6) hex.resize(6);
                        const ccColor3B c = NXR::Utils::hexToColor(hex);
                        state->setRGB(c.r, c.g, c.b);
                    } else {
                        auto parsed = geode::utils::numFromString<int>(text);
                        if (parsed.isErr()) return;
                        const int value = std::clamp(parsed.unwrap(), 0, 255);
                        const ccColor3B c = state->rgb();
                        if (index == 1) state->setRGB(value, c.g, c.b);
                        else if (index == 2) state->setRGB(c.r, value, c.b);
                        else if (index == 3) state->setRGB(c.r, c.g, value);
                        else state->a = static_cast<float>(value) / 255.f;
                    }
                    state->notify(nullptr);
                });
            }
        public:
            ValuesControl(Host* host, std::shared_ptr<ColorState> state) : Control(host), m_state(std::move(state)) {}
            float height() override { return dp(76.f); }

            void build(CCNode* view, float) override {
                m_bg = CCDrawNode::create();
                view->addChild(m_bg);
                m_values.clear();
                static const char* names[] = {"HEX", "R", "G", "B", "A"};
                for (int i = 0; i < fieldCount(); i++) {
                    auto* value = makeLabel("", dp(16.f), Pal::text(), CCPoint(0.5f, 0.5f));
                    value->setPosition(CCPoint(fieldX(i) + fieldW(i) * 0.5f, dp(8.f) + (height() - dp(16.f) - dp(14.f)) * 0.5f));
                    view->addChild(value, 2);
                    m_values.push_back(value);
                    auto* name = makeLabel(names[i], dp(12.f), Pal::muted(), CCPoint(0.5f, 0.5f));
                    name->setPosition(CCPoint(fieldX(i) + fieldW(i) * 0.5f, height() - dp(10.f)));
                    view->addChild(name, 2);
                }
                m_state->subscribe(this, [this](void*) { redraw(); });
                redraw();
            }

            void release() override {
                m_state->unsubscribe(this);
                m_bg = nullptr;
                m_values.clear();
            }

            Touch touchBegan(const CCPoint& p) override {
                if (p.y > height() - dp(22.f)) return Touch::None;
                m_pressed = fieldAt(p);
                redraw();
                return m_pressed >= 0 ? Touch::Tap : Touch::None;
            }

            void touchEnded(const CCPoint& p, bool tap) override {
                const int pressed = m_pressed;
                m_pressed = -1;
                redraw();
                if (tap && pressed >= 0 && fieldAt(p) == pressed) edit(pressed);
            }

            void touchCancelled() override {
                m_pressed = -1;
                redraw();
            }
        };

        class SwatchControl final : public Control {
            std::shared_ptr<ColorState> m_state;
            std::string m_title;
            std::vector<std::string> m_colors;
            std::string m_empty;
            int m_pressed = -1;

            int columns() const { return std::max(1, static_cast<int>((m_width - dp(28.f)) / dp(48.f))); }
            int rows() const { return std::max(1, (static_cast<int>(m_colors.size()) + columns() - 1) / columns()); }
            float cell() const { return (m_width - dp(28.f)) / static_cast<float>(columns()); }

            int indexAt(const CCPoint& p) {
                if (m_colors.empty()) return -1;
                const int col = static_cast<int>((p.x - dp(14.f)) / cell());
                const float topY = height() - dp(34.f);
                const int row = static_cast<int>((topY - p.y) / dp(48.f));
                if (col < 0 || col >= columns() || row < 0 || p.y > topY) return -1;
                const int index = row * columns() + col;
                return index < static_cast<int>(m_colors.size()) ? index : -1;
            }
        public:
            SwatchControl(Host* host, std::shared_ptr<ColorState> state, std::string title, std::vector<std::string> colors, std::string empty)
                : Control(host), m_state(std::move(state)), m_title(std::move(title)), m_colors(std::move(colors)), m_empty(std::move(empty)) {}

            float height() override {
                if (m_colors.empty()) return dp(34.f) + dp(36.f);
                return dp(34.f) + dp(48.f) * static_cast<float>(rows()) + dp(6.f);
            }

            void build(CCNode* view, float) override {
                auto* title = makeLabel(m_title, dp(13.f), Pal::muted(), CCPoint(0.f, 0.5f));
                title->setPosition(CCPoint(dp(18.f), height() - dp(14.f)));
                view->addChild(title);

                if (m_colors.empty()) {
                    auto* empty = makeLabel(m_empty, dp(13.f), Pal::muted(), CCPoint(0.f, 0.5f));
                    empty->setPosition(CCPoint(dp(18.f), height() - dp(46.f)));
                    view->addChild(empty);
                    return;
                }

                auto* draw = CCDrawNode::create();
                view->addChild(draw);
                const float topY = height() - dp(34.f);
                for (size_t i = 0; i < m_colors.size(); i++) {
                    const int col = static_cast<int>(i) % columns();
                    const int row = static_cast<int>(i) / columns();
                    const float cx = dp(14.f) + cell() * (static_cast<float>(col) + 0.5f);
                    const float cy = topY - dp(48.f) * (static_cast<float>(row) + 0.5f);
                    const ccColor4F c = NXR::Utils::hexToColor4F(m_colors[i]);
                    drawCircle(draw, cx, cy, dp(17.f), fillColor(70, 84, 110));
                    drawCircle(draw, cx, cy, dp(15.5f), fillColor(static_cast<int>(c.r * 255.f), static_cast<int>(c.g * 255.f), static_cast<int>(c.b * 255.f)));
                }
            }

            Touch touchBegan(const CCPoint& p) override {
                m_pressed = indexAt(p);
                return m_pressed >= 0 ? Touch::Tap : Touch::None;
            }

            void touchEnded(const CCPoint& p, bool tap) override {
                const int pressed = m_pressed;
                m_pressed = -1;
                if (!tap || pressed < 0 || indexAt(p) != pressed) return;
                const ccColor4F c = NXR::Utils::hexToColor4F(m_colors[static_cast<size_t>(pressed)]);
                m_state->setRGB(static_cast<int>(std::round(c.r * 255.f)), static_cast<int>(std::round(c.g * 255.f)), static_cast<int>(std::round(c.b * 255.f)));
                if (m_state->alpha) m_state->a = c.a;
                m_state->notify(nullptr);
            }
        };

        class KeypadControl final : public Control {
            NumberKind m_kind;
            std::shared_ptr<std::string> m_text;
            CCDrawNode* m_bg = nullptr;
            geode::Label* m_display = nullptr;
            std::vector<std::string> m_keys;
            int m_cols = 4;
            int m_pressed = -1;
            bool m_fresh = true;

            int rows() const { return static_cast<int>((m_keys.size() + static_cast<size_t>(m_cols) - 1) / static_cast<size_t>(m_cols)); }
            float cellW() const { return (m_width - dp(28.f)) / static_cast<float>(m_cols); }
            float keyH() const { return dp(54.f); }

            int indexAt(const CCPoint& p) {
                const float gridTop = height() - dp(74.f);
                if (p.y > gridTop) return -1;
                const int col = static_cast<int>((p.x - dp(14.f)) / cellW());
                const int row = static_cast<int>((gridTop - p.y) / keyH());
                if (col < 0 || col >= m_cols || row < 0 || row >= rows()) return -1;
                const int index = row * m_cols + col;
                if (index >= static_cast<int>(m_keys.size())) return -1;
                return m_keys[static_cast<size_t>(index)].empty() ? -1 : index;
            }

            size_t maxLen() const {
                if (m_kind == NumberKind::Hex) return 8;
                return m_kind == NumberKind::Decimal ? 12 : 10;
            }

            void refreshDisplay() {
                if (!m_display) return;
                const std::string shown = m_text->empty() ? std::string("0") : *m_text;
                m_display->setString(shown.c_str());
                m_display->setScale(dp(30.f) / lineHeightUnit());
                fitLabel(m_display, m_width - dp(60.f));
            }

            void press(const std::string& key) {
                if (key == "DEL") {
                    if (m_fresh) m_text->clear();
                    else if (!m_text->empty()) m_text->pop_back();
                } else if (key == "CLR") {
                    m_text->clear();
                } else if (key == "NEG") {
                    if (!m_text->empty() && m_text->front() == '-') m_text->erase(m_text->begin());
                    else m_text->insert(m_text->begin(), '-');
                } else {
                    if (m_fresh) m_text->clear();
                    if (key == "." && m_text->find('.') != std::string::npos) {
                        refreshDisplay();
                        return;
                    }
                    if (m_text->size() < maxLen()) m_text->append(key);
                }
                m_fresh = false;
                refreshDisplay();
            }

            std::string labelFor(const std::string& key) const {
                if (key == "DEL") return "Del";
                if (key == "CLR") return "Clear";
                if (key == "NEG") return "+/-";
                return key;
            }
        public:
            KeypadControl(Host* host, NumberKind kind, std::shared_ptr<std::string> text) : Control(host), m_kind(kind), m_text(std::move(text)) {
                if (m_kind == NumberKind::Hex) {
                    m_cols = 5;
                    m_keys = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "A", "B", "C", "D", "E", "F", "DEL", "CLR", "", ""};
                } else {
                    m_cols = 4;
                    m_keys = {"7", "8", "9", "DEL", "4", "5", "6", "CLR", "1", "2", "3", "NEG", "0", m_kind == NumberKind::Decimal ? "." : "", "", ""};
                }
            }

            float height() override { return dp(74.f) + keyH() * static_cast<float>(rows()) + dp(10.f); }

            void build(CCNode* view, float width) override {
                m_bg = CCDrawNode::create();
                view->addChild(m_bg);
                drawRound(m_bg, dp(14.f), height() - dp(64.f), width - dp(28.f), dp(54.f), dp(12.f), fillColor(11, 16, 26));
                m_display = makeLabel("", dp(30.f), Pal::text(), CCPoint(1.f, 0.5f));
                m_display->setPosition(CCPoint(width - dp(30.f), height() - dp(37.f)));
                view->addChild(m_display, 2);
                refreshDisplay();

                const float gridTop = height() - dp(74.f);
                for (size_t i = 0; i < m_keys.size(); i++) {
                    if (m_keys[i].empty()) continue;
                    const int col = static_cast<int>(i) % m_cols;
                    const int row = static_cast<int>(i) / m_cols;
                    const float x = dp(14.f) + cellW() * static_cast<float>(col);
                    const float y = gridTop - keyH() * static_cast<float>(row + 1);
                    const bool special = m_keys[i].size() > 1;
                    drawRound(m_bg, x + dp(3.f), y + dp(3.f), cellW() - dp(6.f), keyH() - dp(6.f), dp(12.f), special ? fillColor(36, 50, 74) : fillColor(26, 36, 54));
                    auto* label = makeLabel(labelFor(m_keys[i]), dp(m_keys[i].size() > 1 ? 16.f : 22.f), special ? Pal::accent() : Pal::text(), CCPoint(0.5f, 0.5f));
                    label->setPosition(CCPoint(x + cellW() * 0.5f, y + keyH() * 0.5f));
                    view->addChild(label, 2);
                }
            }

            void release() override {
                m_bg = nullptr;
                m_display = nullptr;
            }

            Touch touchBegan(const CCPoint& p) override {
                m_pressed = indexAt(p);
                return m_pressed >= 0 ? Touch::Tap : Touch::None;
            }

            void touchEnded(const CCPoint& p, bool tap) override {
                const int pressed = m_pressed;
                m_pressed = -1;
                if (!tap || pressed < 0 || indexAt(p) != pressed) return;
                press(m_keys[static_cast<size_t>(pressed)]);
            }
        };
    }

    std::vector<ControlPtr> makePickerItems(Host* host, std::shared_ptr<ColorState> state, const std::string& defaultHex, bool showRainbow) {
        std::vector<ControlPtr> items;
        items.push_back(std::make_shared<PreviewControl>(host, state));
        items.push_back(std::make_shared<SVControl>(host, state));
        items.push_back(std::make_shared<BarControl>(host, state, false));
        if (state->alpha) items.push_back(std::make_shared<BarControl>(host, state, true));
        items.push_back(std::make_shared<ValuesControl>(host, state));

        std::vector<std::string> recent = NXR::Ui::recentColors();
        items.push_back(std::make_shared<SwatchControl>(host, state, "Recent", recent, "No recent colors yet"));

        std::vector<std::string> presets = {"FFFFFFFF", "FF4D4DFF", "FF9F43FF", "FFD84DFF", "39FF6EFF", "22D3EEFF", "4D7CFFFF", "C84DFFFF"};
        items.push_back(std::make_shared<SwatchControl>(host, state, "Presets", presets, ""));

        if (showRainbow) {
            items.push_back(makeToggle(host, "Rainbow", [state] { return state->rainbow; }, [state](bool value) { state->rainbow = value; }));
        }

        items.push_back(makeButtons(host, {{"Reset to default", [state, defaultHex] {
            if (state->alpha) {
                const ccColor4F c = NXR::Utils::hexToColor4F(defaultHex);
                state->setRGB(static_cast<int>(std::round(c.r * 255.f)), static_cast<int>(std::round(c.g * 255.f)), static_cast<int>(std::round(c.b * 255.f)));
                state->a = c.a;
            } else {
                const ccColor3B c = NXR::Utils::hexToColor(defaultHex);
                state->setRGB(c.r, c.g, c.b);
                state->a = 1.f;
            }
            state->notify(nullptr);
        }}}));
        return items;
    }

    ControlPtr makeKeypad(Host* host, NumberKind kind, std::shared_ptr<std::string> text) {
        return std::make_shared<KeypadControl>(host, kind, std::move(text));
    }
}
