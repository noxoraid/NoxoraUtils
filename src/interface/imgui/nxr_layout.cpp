#include "nxr_layout.hpp"
#include "nxr_widget.hpp"
#include <imgui.h>
#include "../../core/nxr_config.hpp"

void NXR::Layout::Manager::setLayout(const std::vector<std::vector<std::string>>& layout) {
    m_layout = layout;
}

void NXR::Layout::Manager::setFixedWindowSizeInfo(const std::vector<NXR::Layout::WindowInfo>& fixedWindows) {
    m_fixedWindows = fixedWindows;
}

void NXR::Layout::Manager::startCollecting() {
    m_windows.clear();
    m_stage = Stage::Collecting;
}

void NXR::Layout::Manager::addWindowInfo(const std::string& name, float width, float height) {
    if (m_stage != Stage::Collecting) return;

    if (WindowInfo* window = findFixedWindow(name)) {
        if (window->w != 0.f) width = window->w;
        if (window->h != 0.f) height = window->h;
    }

    m_windows.push_back({
        name,
        width, height,
        0.f, 0.f
    });
}

void NXR::Layout::Manager::finishCollecting() {
    if (m_stage == Stage::Collecting) {
        m_stage = Stage::Calculating;
        calculateWindowPositions();
    }
}

bool NXR::Layout::Manager::applyWindowTransform(const std::string& name) {
    if (m_stage == Stage::Collecting) {
        ImGui::SetNextWindowPos(ImVec2(-10000.f, -10000.f), ImGuiCond_Always);
        return true;
    }

    if (m_stage == Stage::Applying) {
        WindowInfo* info = getWindowInfo(name);
        if (info) {
            ImGui::SetNextWindowPos(ImVec2(info->x, info->y), ImGuiCond_Always);

            ImGui::SetNextWindowSize(ImVec2(info->w, info->h + 3.f), ImGuiCond_Always);
            return true;
        }
    }
    return false;
}

bool NXR::Layout::Manager::isCollecting() {
    return m_stage == Stage::Collecting;
}

void NXR::Layout::Manager::startApplying() {
    m_stage = Stage::Applying;
}

bool NXR::Layout::Manager::isApplying() {
    return m_stage == Stage::Applying;
}

void NXR::Layout::Manager::finishApplying() {
    if (m_stage == Stage::Applying) {
        reset();
    }
}

NXR::Layout::WindowInfo* NXR::Layout::Manager::getWindowInfo(const std::string& name) {
    return findWindow(name);
}

void NXR::Layout::Manager::reset() {

    m_stage = Stage::Idle;
}

NXR::Layout::WindowInfo* NXR::Layout::Manager::findWindow(const std::string& name) {
    for (auto& window : m_windows) {
        if (window.window_name == name) {
            return &window;
        }
    }
    return nullptr;
}

NXR::Layout::WindowInfo* NXR::Layout::Manager::findFixedWindow(const std::string& name) {
    for (auto& window : m_fixedWindows) {
        if (window.window_name == name) {
            return &window;
        }
    }
    return nullptr;
}

float NXR::Layout::Manager::getMaxWidthInColumn(const std::vector<std::string>& column) {
    float max_width = 0.0f;
    for (const auto& name : column) {
        WindowInfo* window = findWindow(name);
        if (window && window->w > max_width) {
            max_width = window->w;
        }
    }
    return max_width;
}

float NXR::Layout::Manager::getColumnHeight(const std::vector<std::string>& column) {
    float total_height = 0.0f;
    for (size_t i = 0; i < column.size(); i++) {
        WindowInfo* window = findWindow(column[i]);
        if (window) {
            total_height += window->h;
            if (i < column.size() - 1) {
                total_height += SPACING_Y;
            }
        }
    }
    return total_height;
}

void NXR::Layout::Manager::calculateWindowPositions() {
    if (m_layout.empty()) return;

    if (!m_isBaseCollected && !m_windows.empty()) {
        m_baseWindows = m_windows;
        m_isBaseCollected = true;
    }

    if (m_isBaseCollected) {
        m_windows = m_baseWindows;
    }

    float total_width = PADDING_X;
    float max_layout_height = 0.0f;

    for (const auto& column : m_layout) {
        float col_width = getMaxWidthInColumn(column);
        float col_height = getColumnHeight(column) + (PADDING_Y * 2);
        total_width += col_width + PADDING_X;
        if (col_height > max_layout_height) max_layout_height = col_height;
    }

    ImVec2 display = ImGui::GetIO().DisplaySize;

    float fill_scale_x = (display.x > 0) ? (display.x / total_width) : 1.0f;
    float fill_scale_y = (display.y > 0) ? (display.y / max_layout_height) : 1.0f;
    float perfect_fill_scale = std::min(fill_scale_x, fill_scale_y);

    float final_scale = 1.0f;

    if (perfect_fill_scale < 1.0f) {
        final_scale = perfect_fill_scale;
    } else {
        float screen_scale_x = display.x / 1920.f;
        float screen_scale_y = display.y / 1080.f;
        float auto_hi_dpi_scale = std::min(screen_scale_x, screen_scale_y);

        final_scale = std::min({auto_hi_dpi_scale, perfect_fill_scale, 2.5f});

        if (final_scale < 1.0f) final_scale = 1.0f;
    }

    const float STEP = 0.1f;
    float discrete_scale = std::floor(final_scale / STEP) * STEP;
    discrete_scale = std::max(0.1f, discrete_scale);

    m_scale = discrete_scale;
    NXRWidget::ApplyStyle(m_scale);

    auto& config = NXRConfig::get();
    bool horizontal_center = config.get<bool>("nxr.gui::horizontal_center", false);

    float current_x = PADDING_X * m_scale;

    if (horizontal_center) {
        float total_scaled_width = PADDING_X * m_scale;
        for (const auto& column : m_layout) {
            float col_width = getMaxWidthInColumn(column);
            total_scaled_width += (col_width * m_scale) + (PADDING_X * m_scale);
        }

        if (display.x > total_scaled_width) {
            current_x = (display.x - total_scaled_width) / 2.0f + (PADDING_X * m_scale);
        }
    }

    for (const auto& column : m_layout) {
        float max_width = getMaxWidthInColumn(column);
        float current_y = PADDING_Y * m_scale;
        float scaled_max_width = max_width * m_scale;

        for (const auto& name : column) {
            WindowInfo* window = findWindow(name);
            if (window) {
                window->x = current_x;
                window->y = current_y;

                window->w = scaled_max_width;
                window->h = window->h * m_scale;

                current_y += window->h + SPACING_Y * m_scale;
            }
        }

        current_x += scaled_max_width + PADDING_X * m_scale;
    }

    m_stage = Stage::Applying;
}
