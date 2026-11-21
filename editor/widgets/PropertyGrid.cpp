#include "widgets/PropertyGrid.h"
#include "style/EditorTheme.h"
#include "style/Icons.h"
#include "widgets/Widgets.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <format>

namespace atmos::editor
{
    namespace
    {
        constexpr float LabelColumnFraction = 0.4f;

        bool NearlyEqual(float a, float b)
        {
            return std::abs(a - b) <= 1e-4f * std::max(1.0f, std::abs(b));
        }

        bool NearlyEqual(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
        {
            return NearlyEqual(a.x, b.x) && NearlyEqual(a.y, b.y) && NearlyEqual(a.z, b.z);
        }

        float GetResetWidth()
        {
            return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
        }
    }

    PropertyGrid::PropertyGrid(const char* id)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(ImGui::GetStyle().CellPadding.x, 2.0f));
        visible_ = ImGui::BeginTable(id, 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings |
                                                ImGuiTableFlags_BordersInnerV);

        if (visible_)
        {
            const float width = ImGui::GetContentRegionAvail().x;
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, width * LabelColumnFraction);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, width * (1.0f - LabelColumnFraction));
        }
    }

    PropertyGrid::~PropertyGrid()
    {
        if (visible_)
            ImGui::EndTable();

        ImGui::PopStyleVar();
    }

    float PropertyGrid::BeginRow(const char* label, const char* tooltip)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().textDim);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();

        if (tooltip)
            widgets::Tooltip(tooltip);

        ImGui::TableSetColumnIndex(1);
        ImGui::PushID(label);
        return ImGui::GetContentRegionAvail().x - GetResetWidth();
    }

    bool PropertyGrid::ResetButton(bool modified)
    {
        ImGui::SameLine();
        bool reset = false;

        if (modified)
        {
            const ThemeColors& colors = GetThemeColors();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, colors.warning);
            reset = widgets::IconButton("reset", Icon::Undo, "Reset to default");
            ImGui::PopStyleColor(2);
        }
        else
        {
            ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
        }

        ImGui::PopID();
        return reset;
    }

    bool PropertyGrid::Float(const char* label, float& value, float defaultValue, float min, float max,
                             const char* format, float speed, const char* tooltip)
    {
        ImGui::SetNextItemWidth(BeginRow(label, tooltip));
        bool changed = ImGui::DragFloat("##value", &value, speed, min, max, format, ImGuiSliderFlags_AlwaysClamp);

        if (ResetButton(!NearlyEqual(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }

        return changed;
    }

    bool PropertyGrid::Slider(const char* label, float& value, float defaultValue, float min, float max,
                              const char* format, bool logarithmic, const char* tooltip)
    {
        ImGui::SetNextItemWidth(BeginRow(label, tooltip));
        const ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | (logarithmic ? ImGuiSliderFlags_Logarithmic : 0);
        bool changed = ImGui::SliderFloat("##value", &value, min, max, format, flags);

        if (ResetButton(!NearlyEqual(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }

        return changed;
    }

    bool PropertyGrid::UInt(const char* label, uint32_t& value, uint32_t defaultValue, uint32_t min, uint32_t max,
                            const char* tooltip)
    {
        ImGui::SetNextItemWidth(BeginRow(label, tooltip));
        bool changed = ImGui::SliderScalar("##value", ImGuiDataType_U32, &value, &min, &max, "%u",
                                           ImGuiSliderFlags_AlwaysClamp);

        if (ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }

        return changed;
    }

    bool PropertyGrid::Float3(const char* label, DirectX::XMFLOAT3& value, const DirectX::XMFLOAT3& defaultValue,
                              float speed, const char* format, const char* tooltip)
    {
        const ThemeColors& colors = GetThemeColors();
        const float width = BeginRow(label, tooltip);
        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        const float fieldWidth = (width - 2.0f * spacing) / 3.0f;
        const ImVec4 axisColors[3] = { colors.axisX, colors.axisY, colors.axisZ };
        float* components = &value.x;
        bool changed = false;

        for (int axis = 0; axis < 3; ++axis)
        {
            if (axis > 0)
                ImGui::SameLine(0.0f, spacing);

            ImGui::PushID(axis);
            ImGui::SetNextItemWidth(fieldWidth);
            changed |= ImGui::DragFloat("##axis", &components[axis], speed, 0.0f, 0.0f, format);

            // Unreal-style colored strip on the left edge of each field.
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled(min, ImVec2(min.x + 3.0f, max.y), ToU32(axisColors[axis]),
                                                      ImGui::GetStyle().FrameRounding, ImDrawFlags_RoundCornersLeft);
            ImGui::PopID();
        }

        if (ResetButton(!NearlyEqual(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }

        return changed;
    }

    bool PropertyGrid::Color(const char* label, DirectX::XMFLOAT3& value, const DirectX::XMFLOAT3& defaultValue,
                             const char* tooltip)
    {
        ImGui::SetNextItemWidth(BeginRow(label, tooltip));
        bool changed = ImGui::ColorEdit3("##value", &value.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB);

        if (ResetButton(!NearlyEqual(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }

        return changed;
    }

    bool PropertyGrid::Bool(const char* label, bool& value, bool defaultValue, const char* tooltip)
    {
        BeginRow(label, tooltip);
        bool changed = ImGui::Checkbox("##value", &value);

        // Keep the reset arrow aligned with the other rows.
        ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);

        if (ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }

        return changed;
    }

    bool PropertyGrid::Enum(const char* label, uint32_t& value, std::span<const char* const> names, uint32_t defaultValue,
                            const char* tooltip)
    {
        ImGui::SetNextItemWidth(BeginRow(label, tooltip));
        bool changed = false;
        const char* preview = value < names.size() ? names[value] : "?";

        if (ImGui::BeginCombo("##value", preview))
        {
            for (uint32_t i = 0; i < names.size(); ++i)
            {
                if (ImGui::Selectable(names[i], i == value))
                {
                    changed = i != value;
                    value = i;
                }
            }

            ImGui::EndCombo();
        }

        if (ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }

        return changed;
    }

    bool PropertyGrid::Choice(const char* label, uint32_t& value, std::span<const uint32_t> values, uint32_t defaultValue,
                              const char* tooltip)
    {
        ImGui::SetNextItemWidth(BeginRow(label, tooltip));
        bool changed = false;
        const std::string preview = std::to_string(value);

        if (ImGui::BeginCombo("##value", preview.c_str()))
        {
            for (uint32_t option : values)
            {
                if (ImGui::Selectable(std::to_string(option).c_str(), option == value))
                {
                    changed = option != value;
                    value = option;
                }
            }

            ImGui::EndCombo();
        }

        if (ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }

        return changed;
    }

    bool PropertyGrid::Text(const char* label, std::string& value)
    {
        const bool committed = widgets::InputString("##value", value, BeginRow(label, nullptr));
        ResetButton(false);
        return committed;
    }

    void PropertyGrid::ReadOnly(const char* label, const char* format, ...)
    {
        BeginRow(label, nullptr);
        ImGui::AlignTextToFramePadding();

        va_list args;
        va_start(args, format);
        ImGui::TextV(format, args);
        va_end(args);

        ImGui::PopID();
    }
}
