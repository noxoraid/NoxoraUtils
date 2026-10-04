#include "nxr_ui_kit.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_ui_mode.hpp"

using namespace geode::prelude;

namespace NXR::Kit {
    namespace {
        ccColor4F cardFill(bool pressed) {
            return pressed ? fillColor(32, 43, 62) : fillColor(21, 28, 41);
        }

        void drawCard(CCDrawNode* node, Host* host, float width, float height, bool pressed) {
            node->clear();
            drawRound(node, host->dp(8.f), host->dp(3.f), width - host->dp(16.f), height - host->dp(6.f), host->dp(10.f), cardFill(pressed));
        }

        void drawSwitch(CCDrawNode* node, float x, float y, float w, float h, bool on, bool disabled) {
            node->clear();
            const float alpha = disabled ? 0.45f : 1.f;
            drawRound(node, x, y, w, h, h * 0.5f, on ? fromColor(Pal::accent(), alpha) : fillColor(58, 68, 88, alpha));
            const float radius = h * 0.5f - h * 0.14f;
            const float cx = on ? x + w - h * 0.5f : x + h * 0.5f;
            drawCircle(node, cx, y + h * 0.5f, radius, fillColor(255, 255, 255, alpha));
        }

        std::string upper(const std::string& text) {
            std::string out = text;
            for (auto& c : out) {
                if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
            }
            return out;
        }

        void defer(std::function<void()> fn) {
            if (!fn) return;
            geode::queueInMainThread([fn = std::move(fn)]() mutable { fn(); });
        }

        std::vector<std::string> splitLines(const std::string& text) {
            std::vector<std::string> out;
            std::string line;
            for (char c : text) {
                if (c == '\n') {
                    out.push_back(line);
                    line.clear();
                } else {
                    line.push_back(c);
                }
            }
            out.push_back(line);
            return out;
        }

        class SectionControl final : public Control {
            std::string m_text;
        public:
            SectionControl(Host* host, std::string text) : Control(host), m_text(upper(text)) {}
            float height() override { return dp(40.f); }
            void build(CCNode* view, float) override {
                auto* label = makeLabel(m_text, dp(14.f), Pal::accent(), CCPoint(0.f, 0.5f));
                label->setPosition(CCPoint(dp(18.f), dp(14.f)));
                view->addChild(label);
            }
        };

        class TextControl final : public Control {
            std::string m_text;
            std::string m_wrapped;
            float m_line;
            bool m_center;
            bool m_accent;
        public:
            TextControl(Host* host, std::string text, float size, bool center, bool accent)
                : Control(host), m_text(std::move(text)), m_line(size), m_center(center), m_accent(accent) {}

            float height() override {
                m_wrapped = wrapText(m_text, m_width - dp(36.f), dp(m_line));
                return countLines(m_wrapped) * dp(m_line) * 1.15f + dp(14.f);
            }

            void build(CCNode* view, float width) override {
                const auto lines = splitLines(m_wrapped);
                const float lineH = dp(m_line) * 1.15f;
                const float top = height() - dp(7.f);
                for (size_t i = 0; i < lines.size(); i++) {
                    if (lines[i].empty()) continue;
                    auto* label = makeLabel(lines[i], dp(m_line), m_accent ? Pal::accent() : Pal::muted(), CCPoint(m_center ? 0.5f : 0.f, 0.5f));
                    label->setPosition(CCPoint(m_center ? width * 0.5f : dp(18.f), top - lineH * (static_cast<float>(i) + 0.5f)));
                    view->addChild(label);
                }
            }
        };

        class SeparatorControl final : public Control {
            float m_height;
        public:
            SeparatorControl(Host* host, float height) : Control(host), m_height(height) {}
            float height() override { return dp(m_height); }
            void build(CCNode* view, float width) override {
                auto* node = CCDrawNode::create();
                drawRound(node, dp(16.f), height() * 0.5f - 0.5f, width - dp(32.f), 1.f, 0.f, fillColor(40, 52, 72));
                view->addChild(node);
            }
        };

        class SpacerControl final : public Control {
            float m_height;
        public:
            SpacerControl(Host* host, float height) : Control(host), m_height(height) {}
            float height() override { return dp(m_height); }
            void build(CCNode*, float) override {}
        };

        class ToggleControl final : public Control {
            std::string m_label;
            std::function<bool()> m_get;
            std::function<void(bool)> m_set;
            CCDrawNode* m_bg = nullptr;
            CCDrawNode* m_sw = nullptr;
            bool m_pressed = false;

            void redraw() {
                if (!m_bg || !m_sw) return;
                drawCard(m_bg, host(), m_width, height(), m_pressed);
                const float w = dp(46.f);
                const float h = dp(26.f);
                drawSwitch(m_sw, m_width - dp(8.f) - dp(14.f) - w, height() * 0.5f - h * 0.5f, w, h, m_get(), false);
            }
        public:
            ToggleControl(Host* host, std::string label, std::function<bool()> get, std::function<void(bool)> set)
                : Control(host), m_label(std::move(label)), m_get(std::move(get)), m_set(std::move(set)) {}

            float height() override { return dp(54.f); }

            void build(CCNode* view, float) override {
                m_bg = CCDrawNode::create();
                m_sw = CCDrawNode::create();
                view->addChild(m_bg);
                view->addChild(m_sw);
                auto* label = makeLabel(m_label, dp(17.f), Pal::text(), CCPoint(0.f, 0.5f));
                label->setPosition(CCPoint(dp(22.f), height() * 0.5f));
                fitLabel(label, m_width - dp(22.f) - dp(90.f));
                view->addChild(label);
                redraw();
            }

            void release() override {
                m_bg = nullptr;
                m_sw = nullptr;
            }

            Touch touchBegan(const CCPoint&) override {
                m_pressed = true;
                redraw();
                return Touch::Tap;
            }

            void touchEnded(const CCPoint&, bool tap) override {
                m_pressed = false;
                if (tap) m_set(!m_get());
                redraw();
            }

            void touchCancelled() override {
                m_pressed = false;
                redraw();
            }

            void tick(float) override {
                if (!m_sw) return;
                redraw();
            }
        };

        class ChoiceControl final : public Control {
            std::string m_label;
            std::vector<std::string> m_options;
            std::function<int()> m_get;
            std::function<int(int)> m_select;
            bool m_segmented = false;
            ChipStrip m_strip;
            CCDrawNode* m_bg = nullptr;
            CCDrawNode* m_seg = nullptr;
            std::vector<geode::Label*> m_segLabels;
            float m_x0 = 0.f;
            float m_x1 = 0.f;
            float m_y0 = 0.f;
            float m_bandH = 0.f;
            float m_lastX = 0.f;
            int m_pending = -1;
            int m_current = 0;

            void redrawSegments() {
                if (!m_seg) return;
                m_seg->clear();
                const float count = static_cast<float>(m_options.size());
                const float segW = (m_x1 - m_x0) / count;
                drawRound(m_seg, m_x0, m_y0, m_x1 - m_x0, m_bandH, m_bandH * 0.5f, fillColor(31, 39, 55));
                if (m_current >= 0 && m_current < static_cast<int>(m_options.size())) {
                    drawAccent(m_seg, m_x0 + segW * static_cast<float>(m_current) + dp(3.f), m_y0 + dp(3.f), segW - dp(6.f), m_bandH - dp(6.f), (m_bandH - dp(6.f)) * 0.5f);
                }
                for (size_t i = 0; i < m_segLabels.size(); i++) {
                    m_segLabels[i]->setColor(static_cast<int>(i) == m_current ? Pal::onAccent() : Pal::text());
                }
            }
        public:
            ChoiceControl(Host* host, std::string label, std::vector<std::string> options, std::function<int()> get, std::function<int(int)> select)
                : Control(host), m_label(std::move(label)), m_options(std::move(options)), m_get(std::move(get)), m_select(std::move(select)) {
                m_segmented = m_options.size() <= 4;
            }

            float height() override { return dp(94.f); }

            void build(CCNode* view, float width) override {
                m_bg = CCDrawNode::create();
                view->addChild(m_bg);
                drawCard(m_bg, host(), width, height(), false);

                auto* title = makeLabel(m_label, dp(16.f), Pal::text(), CCPoint(0.f, 0.5f));
                title->setPosition(CCPoint(dp(22.f), height() - dp(22.f)));
                fitLabel(title, width - dp(44.f));
                view->addChild(title);

                m_x0 = dp(18.f);
                m_x1 = width - dp(18.f);
                m_y0 = dp(12.f);
                m_bandH = dp(44.f);
                m_current = m_get();

                if (m_segmented) {
                    m_seg = CCDrawNode::create();
                    view->addChild(m_seg);
                    m_segLabels.clear();
                    const float segW = (m_x1 - m_x0) / static_cast<float>(std::max<size_t>(m_options.size(), 1));
                    for (size_t i = 0; i < m_options.size(); i++) {
                        auto* label = makeLabel(m_options[i], dp(16.f), Pal::text(), CCPoint(0.5f, 0.5f));
                        label->setPosition(CCPoint(m_x0 + segW * (static_cast<float>(i) + 0.5f), m_y0 + m_bandH * 0.5f));
                        fitLabel(label, segW - dp(10.f));
                        view->addChild(label, 2);
                        m_segLabels.push_back(label);
                    }
                    redrawSegments();
                } else {
                    m_strip.build(view, host(), m_x0, m_y0, m_x1 - m_x0, m_bandH, m_options, m_current, true);
                    m_strip.ensureVisible(m_current);
                }
            }

            void release() override {
                m_strip.release();
                m_segLabels.clear();
                m_bg = nullptr;
                m_seg = nullptr;
            }

            int indexAt(const CCPoint& p) const {
                if (m_segmented) {
                    if (p.y < m_y0 || p.y > m_y0 + m_bandH || p.x < m_x0 || p.x > m_x1) return -1;
                    const float segW = (m_x1 - m_x0) / static_cast<float>(std::max<size_t>(m_options.size(), 1));
                    return std::clamp(static_cast<int>((p.x - m_x0) / segW), 0, static_cast<int>(m_options.size()) - 1);
                }
                return m_strip.hit(p);
            }

            Touch touchBegan(const CCPoint& p) override {
                m_pending = indexAt(p);
                m_lastX = p.x;
                if (m_pending >= 0) return Touch::Tap;
                if (!m_segmented && m_strip.contains(p)) return Touch::Tap;
                return Touch::None;
            }

            bool wantsDrag(const CCPoint& start, const CCPoint& now) override {
                if (m_segmented || !m_strip.contains(start) || !m_strip.scrollable()) return false;
                return std::fabs(now.x - start.x) > std::fabs(now.y - start.y) * 1.2f;
            }

            void touchMoved(const CCPoint& p) override {
                m_strip.setOffset(m_strip.offset() + (m_lastX - p.x));
                m_lastX = p.x;
            }

            void touchEnded(const CCPoint& p, bool tap) override {
                if (!tap) return;
                const int index = indexAt(p);
                if (index < 0 || index != m_pending) return;
                const int applied = m_select(index);
                m_current = applied;
                if (m_segmented) redrawSegments();
                else m_strip.setSelected(applied);
            }
        };

        class SliderControl final : public Control, public std::enable_shared_from_this<SliderControl> {
            SliderSpec m_spec;
            ChipStrip m_strip;
            CCDrawNode* m_bg = nullptr;
            CCDrawNode* m_track = nullptr;
            geode::Label* m_number = nullptr;
            float m_value = 0.f;
            float m_x0 = 0.f;
            float m_x1 = 0.f;
            float m_base = 0.f;
            float m_lastX = 0.f;
            int m_mode = 0;
            int m_pending = -1;

            float shift() const { return m_spec.min <= 0.f ? 1.f - m_spec.min : 0.f; }

            float toT(float v) const {
                const float range = m_spec.max - m_spec.min;
                if (range <= 0.f) return 0.f;
                if (m_spec.scale == SliderScale::Log) {
                    const float s = shift();
                    const float low = std::log(m_spec.min + s);
                    const float high = std::log(m_spec.max + s);
                    if (high <= low) return 0.f;
                    return std::clamp((std::log(std::max(v, m_spec.min) + s) - low) / (high - low), 0.f, 1.f);
                }
                return std::clamp((v - m_spec.min) / range, 0.f, 1.f);
            }

            float fromT(float t) const {
                t = std::clamp(t, 0.f, 1.f);
                if (m_spec.scale == SliderScale::Log) {
                    const float s = shift();
                    const float low = std::log(m_spec.min + s);
                    const float high = std::log(m_spec.max + s);
                    return std::exp(low + (high - low) * t) - s;
                }
                return m_spec.min + (m_spec.max - m_spec.min) * t;
            }

            float snap(float v) const {
                if (m_spec.integer) v = std::round(v);
                else if (m_spec.step > 0.f) v = std::round(v / m_spec.step) * m_spec.step;
                v = std::clamp(v, m_spec.min, m_spec.max);
                if (std::fabs(v) < 1e-6f) v = 0.f;
                return v;
            }

            float load() const {
                float v;
                if (m_spec.getter) v = m_spec.getter();
                else if (m_spec.integer) v = static_cast<float>(NXRConfig::get().get<int>(m_spec.key, static_cast<int>(m_spec.def)));
                else v = NXRConfig::get().get<float>(m_spec.key, m_spec.def);
                return std::clamp(v, m_spec.min, m_spec.max);
            }

            void store(float v) {
                if (m_spec.setter) m_spec.setter(v);
                else if (m_spec.integer) NXRConfig::get().set<int>(m_spec.key, static_cast<int>(std::round(v)));
                else NXRConfig::get().set<float>(m_spec.key, v);
                if (m_spec.callback) m_spec.callback(v);
            }

            int presetIndex() const {
                for (size_t i = 0; i < m_spec.presets.size(); i++) {
                    if (std::fabs(toT(m_spec.presets[i].value) - toT(m_value)) < 0.0015f) return static_cast<int>(i);
                }
                return -1;
            }

            void redraw() {
                if (!m_track) return;
                m_track->clear();
                const float cy = m_base + dp(21.f);
                const float t = toT(m_value);
                const float tx = m_x0 + (m_x1 - m_x0) * t;
                const float th = dp(6.f);
                drawRound(m_track, m_x0, cy - th * 0.5f, m_x1 - m_x0, th, th * 0.5f, fillColor(44, 56, 78));
                if (tx > m_x0 + 0.5f) drawAccent(m_track, m_x0, cy - th * 0.5f, tx - m_x0, th, th * 0.5f);
                drawCircle(m_track, tx, cy, dp(12.f), fillColor(255, 255, 255));
                drawCircle(m_track, tx, cy, dp(7.f), fromColor(Pal::accent()));
                if (m_number) {
                    m_number->setString((formatNumber(m_value, m_spec.integer, m_spec.step) + m_spec.suffix).c_str());
                    m_number->setScale(dp(25.f) / lineHeightUnit());
                    fitLabel(m_number, dp(78.f));
                }
                if (!m_spec.presets.empty()) m_strip.setSelected(presetIndex());
            }

            void apply(float v) {
                v = snap(v);
                for (const auto& preset : m_spec.presets) {
                    if (std::fabs(toT(preset.value) - toT(v)) < 0.012f) {
                        v = snap(preset.value);
                        break;
                    }
                }
                m_value = v;
                store(v);
                redraw();
            }

            void fromX(float x) {
                apply(fromT((x - m_x0) / (m_x1 - m_x0)));
            }

            void commit() {
                if (m_spec.onCommit) m_spec.onCommit(m_value);
            }

            float presetsHeight() const { return m_spec.presets.empty() ? 0.f : dp(50.f); }
        public:
            SliderControl(Host* host, SliderSpec spec) : Control(host), m_spec(std::move(spec)) {}

            float height() override { return dp(96.f) + presetsHeight(); }

            void build(CCNode* view, float width) override {
                m_bg = CCDrawNode::create();
                m_track = CCDrawNode::create();
                view->addChild(m_bg);
                view->addChild(m_track, 2);
                drawCard(m_bg, host(), width, height(), false);

                m_value = load();
                m_x0 = dp(26.f);
                m_x1 = width - dp(26.f);
                m_base = presetsHeight() + dp(6.f);

                const float top = height();
                auto* title = makeLabel(m_spec.label, dp(16.f), Pal::muted(), CCPoint(0.f, 0.5f));
                title->setPosition(CCPoint(dp(22.f), top - dp(26.f)));
                fitLabel(title, width * 0.42f);
                view->addChild(title);

                const float fieldW = dp(112.f);
                const float fieldH = dp(38.f);
                const float fieldX = width - dp(18.f) - fieldW;
                const float fieldY = top - dp(26.f) - fieldH * 0.5f;
                drawRound(m_bg, fieldX - 1.5f, fieldY - 1.5f, fieldW + 3.f, fieldH + 3.f, dp(11.f) + 1.5f, fromColor(Pal::accent(), 0.55f));
                drawRound(m_bg, fieldX, fieldY, fieldW, fieldH, dp(11.f), fillColor(11, 16, 26));

                auto* pencil = makeIcon(Icon::Edit, dp(15.f), Pal::muted());
                pencil->setPosition(CCPoint(fieldX + dp(15.f), top - dp(26.f)));
                view->addChild(pencil, 3);

                m_number = makeLabel("", dp(25.f), Pal::text(), CCPoint(0.5f, 0.5f));
                m_number->setPosition(CCPoint(fieldX + fieldW * 0.5f + dp(8.f), top - dp(26.f)));
                view->addChild(m_number, 3);

                if (!m_spec.presets.empty()) {
                    std::vector<std::string> labels;
                    for (const auto& preset : m_spec.presets) labels.push_back(preset.label);
                    m_strip.build(view, host(), dp(18.f), dp(8.f), width - dp(36.f), dp(38.f), labels, presetIndex(), true);
                }
                redraw();
            }

            void release() override {
                m_strip.release();
                m_bg = nullptr;
                m_track = nullptr;
                m_number = nullptr;
            }

            Touch touchBegan(const CCPoint& p) override {
                m_lastX = p.x;
                m_mode = 0;
                m_pending = -1;
                if (!m_spec.presets.empty() && m_strip.contains(p)) {
                    m_mode = 2;
                    m_pending = m_strip.hit(p);
                    return Touch::Tap;
                }
                if (p.y >= m_base && p.y <= m_base + dp(44.f)) {
                    m_mode = 1;
                    fromX(p.x);
                    return Touch::Drag;
                }
                if (p.y > m_base + dp(44.f) && p.x > m_width - dp(140.f)) {
                    m_mode = 3;
                    return Touch::Tap;
                }
                return Touch::None;
            }

            bool wantsDrag(const CCPoint& start, const CCPoint& now) override {
                if (m_mode != 2 || !m_strip.scrollable()) return false;
                return std::fabs(now.x - start.x) > std::fabs(now.y - start.y) * 1.2f;
            }

            void touchMoved(const CCPoint& p) override {
                if (m_mode == 1) {
                    fromX(p.x);
                } else if (m_mode == 2) {
                    m_strip.setOffset(m_strip.offset() + (m_lastX - p.x));
                    m_lastX = p.x;
                }
            }

            void touchEnded(const CCPoint& p, bool tap) override {
                if (m_mode == 1) {
                    commit();
                    return;
                }
                if (!tap) return;
                if (m_mode == 2) {
                    const int index = m_strip.hit(p);
                    if (index >= 0 && index == m_pending) {
                        m_value = snap(m_spec.presets[static_cast<size_t>(index)].value);
                        store(m_value);
                        redraw();
                        commit();
                    }
                } else if (m_mode == 3) {
                    const std::string hint = fmt::format("{} to {}", formatNumber(m_spec.min, m_spec.integer, m_spec.step), formatNumber(m_spec.max, m_spec.integer, m_spec.step));
                    std::weak_ptr<SliderControl> weak = shared_from_this();
                    host()->openNumberInput(m_spec.label, hint, formatNumber(m_value, m_spec.integer, m_spec.step), m_spec.integer ? NumberKind::Integer : NumberKind::Decimal, [weak](const std::string& text) {
                        auto self = weak.lock();
                        if (!self) return;
                        auto parsed = geode::utils::numFromString<float>(text);
                        if (parsed.isErr()) return;
                        self->m_value = self->snap(parsed.unwrap());
                        self->store(self->m_value);
                        self->redraw();
                        self->commit();
                    });
                }
            }
        };

        class ColorRowControl final : public Control {
            ColorSpec m_spec;
            CCDrawNode* m_bg = nullptr;
            bool m_pressed = false;

            void redraw() {
                if (!m_bg) return;
                drawCard(m_bg, host(), m_width, height(), m_pressed);
                ColorState state;
                loadColorState(state, m_spec);
                const float w = dp(58.f);
                const float h = dp(30.f);
                const float x = m_width - dp(22.f) - w;
                const float y = height() * 0.5f - h * 0.5f;
                drawRound(m_bg, x - 1.5f, y - 1.5f, w + 3.f, h + 3.f, dp(8.f) + 1.5f, fillColor(70, 84, 110));
                drawRound(m_bg, x, y, w, h, dp(8.f), fromColor(state.rgb()));
            }
        public:
            ColorRowControl(Host* host, ColorSpec spec) : Control(host), m_spec(std::move(spec)) {}
            float height() override { return dp(56.f); }

            void build(CCNode* view, float) override {
                m_bg = CCDrawNode::create();
                view->addChild(m_bg);
                auto* label = makeLabel(m_spec.label, dp(17.f), Pal::text(), CCPoint(0.f, 0.5f));
                label->setPosition(CCPoint(dp(22.f), height() * 0.5f));
                fitLabel(label, m_width - dp(130.f));
                view->addChild(label);

                ColorState state;
                loadColorState(state, m_spec);
                auto* hex = makeLabel("#" + (m_spec.alpha ? state.hex8() : state.hex6()), dp(13.f), Pal::muted(), CCPoint(1.f, 0.5f));
                hex->setPosition(CCPoint(m_width - dp(22.f) - dp(58.f) - dp(10.f), height() * 0.5f));
                view->addChild(hex);
                if (hex->getPositionX() - hex->getScaledContentWidth() < label->getPositionX() + label->getScaledContentWidth() + dp(6.f)) hex->setVisible(false);
                redraw();
            }

            void release() override { m_bg = nullptr; }

            Touch touchBegan(const CCPoint&) override {
                m_pressed = true;
                redraw();
                return Touch::Tap;
            }

            void touchEnded(const CCPoint&, bool tap) override {
                m_pressed = false;
                redraw();
                if (tap) host()->openColorPicker(m_spec, m_spec.onChange);
            }

            void touchCancelled() override {
                m_pressed = false;
                redraw();
            }
        };

        class NumberRowControl final : public Control, public std::enable_shared_from_this<NumberRowControl> {
            std::string m_label;
            bool m_integer;
            float m_min;
            float m_max;
            std::function<float()> m_get;
            std::function<void(float)> m_set;
            CCDrawNode* m_bg = nullptr;
            geode::Label* m_value = nullptr;
            int m_zone = 0;
            bool m_pressed = false;

            float step() const {
                const float range = m_max - m_min;
                if (m_integer) return range <= 1000.f ? 1.f : (range <= 100000.f ? 10.f : 100.f);
                return range > 100.f ? 1.f : (range > 10.f ? 0.1f : 0.01f);
            }

            float current() const { return std::clamp(m_get(), m_min, m_max); }

            void write(float v) {
                v = std::clamp(v, m_min, m_max);
                if (m_integer) v = std::round(v);
                m_set(v);
                refresh();
            }

            void refresh() {
                if (m_value) {
                    m_value->setString(formatNumber(current(), m_integer, step()).c_str());
                    m_value->setScale(dp(17.f) / lineHeightUnit());
                    fitLabel(m_value, dp(78.f));
                }
            }

            float plusX0() const { return m_width - dp(8.f) - dp(48.f); }
            float valueX0() const { return plusX0() - dp(92.f); }
            float minusX0() const { return valueX0() - dp(48.f); }

            int zoneAt(float x) const {
                if (x >= plusX0()) return 3;
                if (x >= valueX0()) return 2;
                if (x >= minusX0()) return 1;
                return 0;
            }

            void redraw() {
                if (!m_bg) return;
                drawCard(m_bg, host(), m_width, height(), m_pressed && m_zone == 0);
                const float h = height();
                const float py = h * 0.5f - dp(18.f);
                drawRound(m_bg, valueX0() + dp(4.f), py, dp(84.f), dp(36.f), dp(10.f), fillColor(11, 16, 26));
                drawCircle(m_bg, minusX0() + dp(24.f), h * 0.5f, dp(15.f), m_pressed && m_zone == 1 ? fromColor(Pal::accent(), 0.5f) : fillColor(36, 48, 68));
                drawCircle(m_bg, plusX0() + dp(24.f), h * 0.5f, dp(15.f), m_pressed && m_zone == 3 ? fromColor(Pal::accent(), 0.5f) : fillColor(36, 48, 68));
                drawRound(m_bg, minusX0() + dp(24.f) - dp(7.f), h * 0.5f - 1.25f, dp(14.f), 2.5f, 0.f, fillColor(232, 238, 246));
                drawRound(m_bg, plusX0() + dp(24.f) - dp(7.f), h * 0.5f - 1.25f, dp(14.f), 2.5f, 0.f, fillColor(232, 238, 246));
                drawRound(m_bg, plusX0() + dp(24.f) - 1.25f, h * 0.5f - dp(7.f), 2.5f, dp(14.f), 0.f, fillColor(232, 238, 246));
            }
        public:
            NumberRowControl(Host* host, std::string label, bool integer, float min, float max, std::function<float()> get, std::function<void(float)> set)
                : Control(host), m_label(std::move(label)), m_integer(integer), m_min(min), m_max(max), m_get(std::move(get)), m_set(std::move(set)) {}

            float height() override { return dp(58.f); }

            void build(CCNode* view, float) override {
                m_bg = CCDrawNode::create();
                view->addChild(m_bg);
                auto* label = makeLabel(m_label, dp(16.f), Pal::text(), CCPoint(0.f, 0.5f));
                label->setPosition(CCPoint(dp(22.f), height() * 0.5f));
                fitLabel(label, minusX0() - dp(30.f));
                view->addChild(label);
                m_value = makeLabel("", dp(17.f), Pal::accent(), CCPoint(0.5f, 0.5f));
                m_value->setPosition(CCPoint(valueX0() + dp(46.f), height() * 0.5f));
                view->addChild(m_value, 2);
                refresh();
                redraw();
            }

            void release() override {
                m_bg = nullptr;
                m_value = nullptr;
            }

            Touch touchBegan(const CCPoint& p) override {
                m_zone = zoneAt(p.x);
                if (m_zone == 0) return Touch::None;
                m_pressed = true;
                redraw();
                return Touch::Tap;
            }

            void touchEnded(const CCPoint& p, bool tap) override {
                m_pressed = false;
                redraw();
                if (!tap || zoneAt(p.x) != m_zone) return;
                if (m_zone == 1) write(current() - step());
                else if (m_zone == 3) write(current() + step());
                else if (m_zone == 2) {
                    const std::string hint = fmt::format("{} to {}", formatNumber(m_min, m_integer, step()), formatNumber(m_max, m_integer, step()));
                    std::weak_ptr<NumberRowControl> weak = shared_from_this();
                    host()->openNumberInput(m_label, hint, formatNumber(current(), m_integer, step()), m_integer ? NumberKind::Integer : NumberKind::Decimal, [weak](const std::string& text) {
                        auto self = weak.lock();
                        if (!self) return;
                        auto parsed = geode::utils::numFromString<float>(text);
                        if (parsed.isErr()) return;
                        self->write(parsed.unwrap());
                    });
                }
            }

            void touchCancelled() override {
                m_pressed = false;
                redraw();
            }
        };

        class ButtonsControl final : public Control {
            std::vector<std::pair<std::string, std::function<void()>>> m_buttons;
            bool m_danger;
            CCDrawNode* m_bg = nullptr;
            int m_down = -1;

            float x0() const { return dp(14.f); }
            float gap() const { return dp(10.f); }
            float bw() const {
                const float count = static_cast<float>(std::max<size_t>(m_buttons.size(), 1));
                return (m_width - dp(28.f) - gap() * (count - 1.f)) / count;
            }
            int indexAt(const CCPoint& p) const {
                for (size_t i = 0; i < m_buttons.size(); i++) {
                    const float left = x0() + (bw() + gap()) * static_cast<float>(i);
                    if (p.x >= left - gap() * 0.5f && p.x <= left + bw() + gap() * 0.5f) return static_cast<int>(i);
                }
                return -1;
            }

            void redraw() {
                if (!m_bg) return;
                m_bg->clear();
                const float h = dp(44.f);
                const float y = height() * 0.5f - h * 0.5f;
                for (size_t i = 0; i < m_buttons.size(); i++) {
                    const float left = x0() + (bw() + gap()) * static_cast<float>(i);
                    const bool down = static_cast<int>(i) == m_down;
                    drawRound(m_bg, left, y, bw(), h, dp(12.f), down ? fromColor(Pal::accent(), 0.35f) : fillColor(30, 41, 60));
                }
            }
        public:
            ButtonsControl(Host* host, std::vector<std::pair<std::string, std::function<void()>>> buttons, bool danger)
                : Control(host), m_buttons(std::move(buttons)), m_danger(danger) {}

            float height() override { return dp(60.f); }

            void build(CCNode* view, float) override {
                m_bg = CCDrawNode::create();
                view->addChild(m_bg);
                for (size_t i = 0; i < m_buttons.size(); i++) {
                    auto* label = makeLabel(m_buttons[i].first, dp(16.f), m_danger ? Pal::danger() : Pal::text(), CCPoint(0.5f, 0.5f));
                    label->setPosition(CCPoint(x0() + (bw() + gap()) * (static_cast<float>(i) + 0.f) + bw() * 0.5f, height() * 0.5f));
                    fitLabel(label, bw() - dp(12.f));
                    view->addChild(label, 2);
                }
                redraw();
            }

            void release() override { m_bg = nullptr; }

            Touch touchBegan(const CCPoint& p) override {
                m_down = indexAt(p);
                redraw();
                return m_down >= 0 ? Touch::Tap : Touch::None;
            }

            void touchEnded(const CCPoint& p, bool tap) override {
                const int pressed = m_down;
                m_down = -1;
                redraw();
                if (!tap || pressed < 0 || indexAt(p) != pressed) return;
                defer(m_buttons[static_cast<size_t>(pressed)].second);
            }

            void touchCancelled() override {
                m_down = -1;
                redraw();
            }
        };

        class SelectorControl final : public Control, public std::enable_shared_from_this<SelectorControl> {
            std::string m_label;
            std::function<std::string()> m_text;
            std::function<void(std::function<void()>)> m_open;
            CCDrawNode* m_bg = nullptr;
            geode::Label* m_value = nullptr;
            bool m_pressed = false;

            float btnW() const { return std::min(m_width * 0.5f, dp(260.f)); }
            float btnX() const { return m_width - dp(18.f) - btnW(); }

            void redraw() {
                if (!m_bg) return;
                drawCard(m_bg, host(), m_width, height(), false);
                drawRound(m_bg, btnX(), height() * 0.5f - dp(20.f), btnW(), dp(40.f), dp(12.f), m_pressed ? fromColor(Pal::accent(), 0.35f) : fillColor(30, 41, 60));
            }
        public:
            SelectorControl(Host* host, std::string label, std::function<std::string()> text, std::function<void(std::function<void()>)> open)
                : Control(host), m_label(std::move(label)), m_text(std::move(text)), m_open(std::move(open)) {}

            float height() override { return dp(60.f); }

            void updateText() {
                if (!m_value) return;
                m_value->setString(m_text().c_str());
                m_value->setScale(dp(15.f) / lineHeightUnit());
                fitLabel(m_value, btnW() - dp(16.f));
            }

            void build(CCNode* view, float) override {
                m_bg = CCDrawNode::create();
                view->addChild(m_bg);
                auto* label = makeLabel(m_label, dp(16.f), Pal::text(), CCPoint(0.f, 0.5f));
                label->setPosition(CCPoint(dp(22.f), height() * 0.5f));
                fitLabel(label, btnX() - dp(40.f));
                view->addChild(label);
                m_value = makeLabel("", dp(15.f), Pal::accent(), CCPoint(0.5f, 0.5f));
                m_value->setPosition(CCPoint(btnX() + btnW() * 0.5f, height() * 0.5f));
                view->addChild(m_value, 2);
                updateText();
                redraw();
            }

            void release() override {
                m_bg = nullptr;
                m_value = nullptr;
            }

            Touch touchBegan(const CCPoint& p) override {
                if (p.x < btnX()) return Touch::None;
                m_pressed = true;
                redraw();
                return Touch::Tap;
            }

            void touchEnded(const CCPoint&, bool tap) override {
                m_pressed = false;
                redraw();
                if (!tap) return;
                std::weak_ptr<SelectorControl> weak = shared_from_this();
                auto open = m_open;
                defer([open, weak]() {
                    if (!open) return;
                    open([weak]() {
                        if (auto self = weak.lock()) self->updateText();
                    });
                });
            }

            void touchCancelled() override {
                m_pressed = false;
                redraw();
            }
        };

        class KeybindControl final : public Control {
            std::string m_label;
            std::function<std::string()> m_get;
            std::function<void()> m_set;
            std::function<void()> m_clear;
            CCDrawNode* m_bg = nullptr;
            geode::Label* m_value = nullptr;
            std::string m_shown;
            int m_down = 0;

            float clearX() const { return m_width - dp(18.f) - dp(66.f); }
            float keyX() const { return clearX() - dp(10.f) - dp(116.f); }

            void redraw() {
                if (!m_bg) return;
                drawCard(m_bg, host(), m_width, height(), false);
                const float y = height() * 0.5f - dp(20.f);
                drawRound(m_bg, keyX(), y, dp(116.f), dp(40.f), dp(12.f), m_down == 1 ? fromColor(Pal::accent(), 0.35f) : fillColor(30, 41, 60));
                drawRound(m_bg, clearX(), y, dp(66.f), dp(40.f), dp(12.f), m_down == 2 ? fromColor(Pal::danger(), 0.3f) : fillColor(30, 41, 60));
            }

            void setShown(const std::string& text) {
                m_shown = text;
                if (!m_value) return;
                m_value->setString(text.c_str());
                m_value->setScale(dp(15.f) / lineHeightUnit());
                fitLabel(m_value, dp(104.f));
            }
        public:
            KeybindControl(Host* host, std::string label, std::function<std::string()> get, std::function<void()> set, std::function<void()> clear)
                : Control(host), m_label(std::move(label)), m_get(std::move(get)), m_set(std::move(set)), m_clear(std::move(clear)) {}

            float height() override { return dp(58.f); }

            void build(CCNode* view, float) override {
                m_bg = CCDrawNode::create();
                view->addChild(m_bg);
                auto* label = makeLabel(m_label, dp(16.f), Pal::text(), CCPoint(0.f, 0.5f));
                label->setPosition(CCPoint(dp(22.f), height() * 0.5f));
                fitLabel(label, keyX() - dp(30.f));
                view->addChild(label);
                m_value = makeLabel("", dp(15.f), Pal::accent(), CCPoint(0.5f, 0.5f));
                m_value->setPosition(CCPoint(keyX() + dp(58.f), height() * 0.5f));
                view->addChild(m_value, 2);
                auto* clear = makeLabel("Clear", dp(14.f), Pal::danger(), CCPoint(0.5f, 0.5f));
                clear->setPosition(CCPoint(clearX() + dp(33.f), height() * 0.5f));
                view->addChild(clear, 2);
                setShown(m_get());
                redraw();
            }

            void release() override {
                m_bg = nullptr;
                m_value = nullptr;
            }

            void tick(float) override {
                if (!m_value) return;
                const std::string now = m_get();
                if (now != m_shown) setShown(now);
            }

            Touch touchBegan(const CCPoint& p) override {
                m_down = p.x >= clearX() ? 2 : (p.x >= keyX() ? 1 : 0);
                redraw();
                return m_down == 0 ? Touch::None : Touch::Tap;
            }

            void touchEnded(const CCPoint& p, bool tap) override {
                const int pressed = m_down;
                m_down = 0;
                redraw();
                if (!tap || pressed == 0) return;
                const int now = p.x >= clearX() ? 2 : (p.x >= keyX() ? 1 : 0);
                if (now != pressed) return;
                if (pressed == 1) m_set();
                else m_clear();
            }

            void touchCancelled() override {
                m_down = 0;
                redraw();
            }
        };

        class LinkControl final : public Control {
            std::string m_title;
            std::function<std::string()> m_subtitle;
            std::function<void()> m_tap;
            CCDrawNode* m_bg = nullptr;
            bool m_pressed = false;
            bool m_hasSub = false;
        public:
            LinkControl(Host* host, std::string title, std::function<std::string()> subtitle, std::function<void()> tap)
                : Control(host), m_title(std::move(title)), m_subtitle(std::move(subtitle)), m_tap(std::move(tap)) {
                m_hasSub = m_subtitle != nullptr;
            }

            float height() override { return dp(m_hasSub ? 62.f : 54.f); }

            void build(CCNode* view, float width) override {
                m_bg = CCDrawNode::create();
                view->addChild(m_bg);
                drawCard(m_bg, host(), width, height(), m_pressed);
                const std::string sub = m_hasSub ? m_subtitle() : std::string();
                auto* title = makeLabel(m_title, dp(17.f), Pal::text(), CCPoint(0.f, 0.5f));
                title->setPosition(CCPoint(dp(22.f), sub.empty() ? height() * 0.5f : height() * 0.5f + dp(9.f)));
                fitLabel(title, width - dp(90.f));
                view->addChild(title);
                if (!sub.empty()) {
                    auto* label = makeLabel(sub, dp(13.f), Pal::muted(), CCPoint(0.f, 0.5f));
                    label->setPosition(CCPoint(dp(22.f), height() * 0.5f - dp(11.f)));
                    fitLabel(label, width - dp(90.f));
                    view->addChild(label);
                }
                auto* icon = makeIcon(Icon::Chevron, dp(16.f), Pal::muted());
                icon->setPosition(CCPoint(width - dp(30.f), height() * 0.5f));
                view->addChild(icon);
            }

            void release() override { m_bg = nullptr; }

            Touch touchBegan(const CCPoint&) override {
                m_pressed = true;
                if (m_bg) drawCard(m_bg, host(), m_width, height(), true);
                return Touch::Tap;
            }

            void touchEnded(const CCPoint&, bool tap) override {
                m_pressed = false;
                if (m_bg) drawCard(m_bg, host(), m_width, height(), false);
                if (tap) defer(m_tap);
            }

            void touchCancelled() override {
                m_pressed = false;
                if (m_bg) drawCard(m_bg, host(), m_width, height(), false);
            }
        };

        class NodeRowControl final : public Control {
            geode::Ref<CCNode> m_holder;
            float m_height;
        public:
            NodeRowControl(Host* host, CCNode* holder, float height) : Control(host), m_holder(holder), m_height(height) {}
            float height() override { return dp(m_height); }
            void build(CCNode* view, float width) override {
                if (!m_holder) return;
                m_holder->removeFromParent();
                m_holder->setAnchorPoint(CCPoint(0.5f, 0.5f));
                m_holder->setPosition(CCPoint(width * 0.5f, height() * 0.5f));
                view->addChild(m_holder);
            }
            void release() override {
                if (m_holder) m_holder->removeFromParent();
            }
        };

        class RailTabControl final : public Control {
            std::string m_name;
            Icon m_icon;
            bool m_active;
            std::function<void()> m_tap;
        public:
            RailTabControl(Host* host, std::string name, Icon icon, bool active, std::function<void()> tap)
                : Control(host), m_name(std::move(name)), m_icon(icon), m_active(active), m_tap(std::move(tap)) {}

            float height() override { return dp(66.f); }

            void build(CCNode* view, float width) override {
                const float h = height();
                if (m_active) {
                    auto* bg = CCDrawNode::create();
                    drawRound(bg, dp(7.f), dp(4.f), width - dp(14.f), h - dp(8.f), dp(14.f), fromColor(Pal::accent(), 0.16f));
                    drawRound(bg, dp(1.f), h * 0.5f - dp(14.f), dp(3.f), dp(28.f), dp(1.5f), fromColor(Pal::accent()));
                    view->addChild(bg);
                }
                const ccColor3B color = m_active ? Pal::accent() : Pal::muted();
                auto* icon = makeIcon(m_icon, dp(24.f), color);
                icon->setPosition(CCPoint(width * 0.5f, h * 0.5f + dp(9.f)));
                view->addChild(icon);
                auto* label = makeLabel(m_name, dp(13.f), color, CCPoint(0.5f, 0.5f));
                label->setPosition(CCPoint(width * 0.5f, h * 0.5f - dp(15.f)));
                fitLabel(label, width - dp(8.f));
                view->addChild(label);
            }

            Touch touchBegan(const CCPoint&) override { return Touch::Tap; }

            void touchEnded(const CCPoint&, bool tap) override {
                if (tap) defer(m_tap);
            }
        };

        class HackRowControl final : public Control {
            std::string m_id;
            std::string m_window;
            CCDrawNode* m_bg = nullptr;
            CCDrawNode* m_sw = nullptr;
            CCDrawNode* m_chipBg = nullptr;
            geode::Label* m_name = nullptr;
            geode::Label* m_chip = nullptr;
            CCSprite* m_starOn = nullptr;
            CCSprite* m_starOff = nullptr;
            std::string m_lastChip;
            bool m_lastEnabled = false;
            bool m_pressed = false;
            bool m_hasTrail = false;
            float m_nameBase = 1.f;
            float m_nameX = 0.f;
            float m_swX = 0.f;
            float m_trailX0 = 0.f;
            float m_acc = 0.f;
            int m_zone = 0;

            NXR::Hack* find() const { return NXR::Gui::get().findHackByIDGlobal(m_id); }

            float starX1() const { return dp(8.f) + dp(46.f); }

            int zoneAt(float x) const {
                if (x < starX1()) return 1;
                if (m_hasTrail && x >= m_trailX0) return 3;
                return 2;
            }

            void redrawBg() {
                auto* hack = find();
                if (!m_bg || !hack) return;
                drawCard(m_bg, host(), m_width, height(), m_pressed);
                if (hack->getEnabled()) {
                    drawRound(m_bg, dp(8.f), height() * 0.5f - dp(12.f), dp(3.f), dp(24.f), dp(1.5f), fromColor(Pal::accent()));
                }
            }

            void redrawSwitch() {
                auto* hack = find();
                if (!m_sw || !hack) return;
                m_lastEnabled = hack->getEnabled();
                drawSwitch(m_sw, m_swX, height() * 0.5f - dp(13.f), dp(46.f), dp(26.f), m_lastEnabled, hack->getDisabled());
            }

            void updateStar() {
                const bool fav = NXR::Ui::isFavorite(m_id);
                if (m_starOn) m_starOn->setVisible(fav);
                if (m_starOff) m_starOff->setVisible(!fav);
            }

            void updateChip(bool force) {
                auto* hack = find();
                if (!hack || !m_chipBg || !m_chip) return;
                const std::string text = hack->hasSummary() ? hack->getSummary() : std::string();
                if (!force && text == m_lastChip) return;
                m_lastChip = text;

                m_chipBg->clear();
                float chipLeft = m_swX - dp(10.f);
                if (text.empty()) {
                    m_chip->setVisible(false);
                } else {
                    m_chip->setVisible(true);
                    m_chip->setString(text.c_str());
                    m_chip->setScale(dp(14.f) / lineHeightUnit());
                    fitLabel(m_chip, dp(96.f));
                    const float w = m_chip->getScaledContentWidth() + dp(20.f);
                    const float h = dp(26.f);
                    const float right = m_swX - dp(10.f);
                    drawRound(m_chipBg, right - w, height() * 0.5f - h * 0.5f, w, h, h * 0.5f, fromColor(Pal::accent(), 0.16f));
                    m_chip->setPosition(CCPoint(right - w * 0.5f, height() * 0.5f));
                    chipLeft = right - w - dp(10.f);
                }

                if (m_name) {
                    m_name->setScale(m_nameBase);
                    fitLabel(m_name, chipLeft - m_nameX);
                }
            }
        public:
            HackRowControl(Host* host, std::string id, std::string window)
                : Control(host), m_id(std::move(id)), m_window(std::move(window)) {}

            float height() override { return dp(m_window.empty() ? 54.f : 62.f); }

            void build(CCNode* view, float width) override {
                auto* hack = find();
                if (!hack) return;

                const float h = height();
                const float cardR = width - dp(8.f);
                m_hasTrail = hack->hasForm() || !hack->getDesc().empty();
                m_trailX0 = cardR - dp(46.f);
                const float rightEdge = m_hasTrail ? m_trailX0 : cardR - dp(8.f);
                m_swX = rightEdge - dp(8.f) - dp(46.f);

                m_bg = CCDrawNode::create();
                m_sw = CCDrawNode::create();
                m_chipBg = CCDrawNode::create();
                view->addChild(m_bg);
                view->addChild(m_sw);
                view->addChild(m_chipBg);

                m_starOn = makeIcon(Icon::StarOn, dp(22.f), ccc3(255, 196, 61));
                m_starOff = makeIcon(Icon::StarOff, dp(22.f), Pal::muted());
                m_starOn->setPosition(CCPoint(dp(8.f) + dp(23.f), h * 0.5f));
                m_starOff->setPosition(m_starOn->getPosition());
                view->addChild(m_starOn, 2);
                view->addChild(m_starOff, 2);
                updateStar();

                m_nameX = starX1() + dp(4.f);
                ccColor3B nameColor = Pal::text();
                if (hack->isCheating()) nameColor = Pal::danger();
                if (hack->getDisabled()) nameColor = Pal::muted();
                m_name = makeLabel(hack->getName(), dp(17.f), nameColor, CCPoint(0.f, 0.5f));
                m_name->setPosition(CCPoint(m_nameX, m_window.empty() ? h * 0.5f : h * 0.5f + dp(9.f)));
                m_nameBase = m_name->getScale();
                view->addChild(m_name, 2);

                if (!m_window.empty()) {
                    auto* sub = makeLabel(m_window, dp(12.f), Pal::muted(), CCPoint(0.f, 0.5f));
                    sub->setPosition(CCPoint(m_nameX, h * 0.5f - dp(12.f)));
                    view->addChild(sub, 2);
                }

                m_chip = makeLabel("", dp(14.f), Pal::accent(), CCPoint(0.5f, 0.5f));
                view->addChild(m_chip, 3);

                if (hack->hasForm()) {
                    auto* icon = makeIcon(Icon::Chevron, dp(18.f), Pal::muted());
                    icon->setPosition(CCPoint(cardR - dp(23.f), h * 0.5f));
                    view->addChild(icon, 2);
                } else if (!hack->getDesc().empty()) {
                    auto* circle = CCDrawNode::create();
                    drawCircle(circle, cardR - dp(23.f), h * 0.5f, dp(11.f), fillColor(40, 54, 78));
                    view->addChild(circle, 2);
                    auto* label = makeLabel("i", dp(15.f), Pal::text(), CCPoint(0.5f, 0.5f));
                    label->setPosition(CCPoint(cardR - dp(23.f), h * 0.5f));
                    view->addChild(label, 3);
                }

                m_lastChip = "\x01";
                updateChip(true);
                redrawBg();
                redrawSwitch();
            }

            void release() override {
                m_bg = nullptr;
                m_sw = nullptr;
                m_chipBg = nullptr;
                m_name = nullptr;
                m_chip = nullptr;
                m_starOn = nullptr;
                m_starOff = nullptr;
            }

            void tick(float dt) override {
                m_acc += dt;
                if (m_acc < 0.25f) return;
                m_acc = 0.f;
                auto* hack = find();
                if (!hack || !m_bg) return;
                updateChip(false);
                if (hack->getEnabled() != m_lastEnabled) {
                    redrawBg();
                    redrawSwitch();
                }
            }

            Touch touchBegan(const CCPoint& p) override {
                m_zone = zoneAt(p.x);
                m_pressed = true;
                redrawBg();
                return Touch::Tap;
            }

            void touchEnded(const CCPoint& p, bool tap) override {
                m_pressed = false;
                redrawBg();
                if (!tap || zoneAt(p.x) != m_zone) return;
                auto* hack = find();
                if (!hack) return;

                if (m_zone == 1) {
                    NXR::Ui::setFavorite(m_id, !NXR::Ui::isFavorite(m_id));
                    updateStar();
                    host()->favoritesChanged();
                } else if (m_zone == 3) {
                    if (hack->hasForm()) host()->openHackSheet(*hack);
                    else host()->openInfoSheet(*hack);
                } else {
                    if (hack->getDisabled()) return;
                    hack->toggle();
                    redrawBg();
                    redrawSwitch();
                    host()->statusChanged();
                }
            }

            void touchCancelled() override {
                m_pressed = false;
                redrawBg();
            }
        };
    }

    std::string formatNumber(float value, bool integer, float step) {
        if (integer) return fmt::format("{}", static_cast<long long>(std::llround(value)));
        const float a = std::fabs(value);
        if (a >= 100.f) return fmt::format("{:.0f}", value);
        if (a >= 10.f) return fmt::format("{:.1f}", value);
        if (a >= 1.f) return fmt::format("{:.2f}", value);
        return step < 0.01f ? fmt::format("{:.3f}", value) : fmt::format("{:.2f}", value);
    }

    ControlPtr makeSection(Host* host, const std::string& text) { return std::make_shared<SectionControl>(host, text); }
    ControlPtr makeText(Host* host, const std::string& text, float sizeDp, bool center, bool accent) { return std::make_shared<TextControl>(host, text, sizeDp, center, accent); }
    ControlPtr makeSeparator(Host* host, float heightDp) { return std::make_shared<SeparatorControl>(host, heightDp); }
    ControlPtr makeSpacer(Host* host, float heightDp) { return std::make_shared<SpacerControl>(host, heightDp); }
    ControlPtr makeToggle(Host* host, const std::string& label, std::function<bool()> get, std::function<void(bool)> set) { return std::make_shared<ToggleControl>(host, label, std::move(get), std::move(set)); }
    ControlPtr makeChoice(Host* host, const std::string& label, std::vector<std::string> options, std::function<int()> get, std::function<int(int)> select) { return std::make_shared<ChoiceControl>(host, label, std::move(options), std::move(get), std::move(select)); }
    ControlPtr makeSlider(Host* host, SliderSpec spec) { return std::make_shared<SliderControl>(host, std::move(spec)); }
    ControlPtr makeColorRow(Host* host, ColorSpec spec) { return std::make_shared<ColorRowControl>(host, std::move(spec)); }
    ControlPtr makeNumberRow(Host* host, const std::string& label, bool integer, float min, float max, std::function<float()> get, std::function<void(float)> set) { return std::make_shared<NumberRowControl>(host, label, integer, min, max, std::move(get), std::move(set)); }
    ControlPtr makeButtons(Host* host, std::vector<std::pair<std::string, std::function<void()>>> buttons, bool danger) { return std::make_shared<ButtonsControl>(host, std::move(buttons), danger); }
    ControlPtr makeSelector(Host* host, const std::string& label, std::function<std::string()> text, std::function<void(std::function<void()>)> open) { return std::make_shared<SelectorControl>(host, label, std::move(text), std::move(open)); }
    ControlPtr makeKeybind(Host* host, const std::string& label, std::function<std::string()> get, std::function<void()> onSet, std::function<void()> onClear) { return std::make_shared<KeybindControl>(host, label, std::move(get), std::move(onSet), std::move(onClear)); }
    ControlPtr makeLink(Host* host, const std::string& title, std::function<std::string()> subtitle, std::function<void()> tap) { return std::make_shared<LinkControl>(host, title, std::move(subtitle), std::move(tap)); }
    ControlPtr makeNodeRow(Host* host, CCNode* holder, float heightDp) { return std::make_shared<NodeRowControl>(host, holder, heightDp); }
    ControlPtr makeHackRow(Host* host, const std::string& hackId, const std::string& windowName) { return std::make_shared<HackRowControl>(host, hackId, windowName); }
    ControlPtr makeRailTab(Host* host, const std::string& name, Icon icon, bool active, std::function<void()> tap) { return std::make_shared<RailTabControl>(host, name, icon, active, std::move(tap)); }
}
