#include "panels/ProfilerPanel.h"
#include "EditorContext.h"
#include "style/EditorFonts.h"
#include "style/EditorTheme.h"
#include "widgets/Widgets.h"

#include "core/StringUtils.h"
#include "graphics/Renderer.h"

#include <algorithm>
#include <format>
#include <vector>

namespace atmos::editor
{
    namespace
    {
        constexpr float PassSmoothing = 0.1f;  // exponential moving average per frame

        float Average(const std::array<float, FrameHistory::Size>& values, size_t newest, size_t count)
        {
            float sum = 0.0f;

            for (size_t i = 0; i < count; ++i)
                sum += values[(newest + FrameHistory::Size - i) % FrameHistory::Size];

            return sum / static_cast<float>(count);
        }
    }

    ProfilerPanel::ProfilerPanel() : Panel("Profiler", Icon::Profiler)
    {
    }

    void ProfilerPanel::DrawContent(EditorContext& context)
    {
        DrawSummary(context);
        DrawFrameGraph(context);
        ImGui::Spacing();

        if (ImGui::BeginTabBar("ProfilerTabs"))
        {
            if (ImGui::BeginTabItem("GPU Passes"))
            {
                DrawPassTable(context);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Counters"))
            {
                DrawCounters(context);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    void ProfilerPanel::DrawSummary(EditorContext& context)
    {
        FrameHistory& history = context.history;
        const size_t newest = (history.next + FrameHistory::Size - 1) % FrameHistory::Size;
        const float cpu = Average(history.cpuMs, newest, 30);
        const float gpu = Average(history.gpuMs, newest, 30);
        const ThemeColors& colors = GetThemeColors();

        if (widgets::IconButton("pause", history.paused ? Icon::Play : Icon::Pause,
                                history.paused ? "Resume recording" : "Pause recording", history.paused))
            history.paused = !history.paused;

        ImGui::SameLine();
        const ScopedFont mono(FontRole::Mono);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%5.0f FPS", cpu > 0.0f ? 1000.0f / cpu : 0.0f);
        ImGui::SameLine(0.0f, ImGui::GetFontSize() * 1.5f);
        ImGui::TextColored(colors.textDim, "CPU");
        ImGui::SameLine();
        ImGui::Text("%6.2f ms", cpu);
        ImGui::SameLine(0.0f, ImGui::GetFontSize() * 1.5f);
        ImGui::TextColored(colors.accent, "GPU");
        ImGui::SameLine();
        ImGui::Text("%6.2f ms", gpu);
    }

    void ProfilerPanel::DrawFrameGraph(EditorContext& context)
    {
        const FrameHistory& history = context.history;
        const ThemeColors& colors = GetThemeColors();
        const ImVec2 size(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 4.5f);
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max(min.x + size.x, min.y + size.y);
        ImGui::Dummy(size);

        float peak = 16.7f;

        for (size_t i = 0; i < FrameHistory::Size; ++i)
            peak = std::max({ peak, history.cpuMs[i] * 1.1f, history.gpuMs[i] * 1.1f });

        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(min, max, ToU32(colors.field), ImGui::GetStyle().FrameRounding);

        auto toY = [&](float ms) { return max.y - std::min(ms / peak, 1.0f) * size.y; };

        // Frame budgets for 60 and 120 Hz.
        ImFont* font = GetFont(FontRole::Small);

        for (const float budget : { 16.67f, 8.33f })
        {
            const float y = toY(budget);
            draw->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), ToU32(colors.textDim, 0.35f));
            const std::string label = std::format("{:.1f} ms", budget);
            draw->AddText(font, font->FontSize, ImVec2(min.x + 4.0f, y + 1.0f), ToU32(colors.textDim), label.c_str());
        }

        auto drawSeries = [&](const std::array<float, FrameHistory::Size>& values, ImU32 color) {
            ImVec2 points[FrameHistory::Size];

            for (size_t i = 0; i < FrameHistory::Size; ++i)
            {
                const float value = values[(history.next + i) % FrameHistory::Size];
                points[i] = ImVec2(min.x + size.x * i / (FrameHistory::Size - 1), toY(value));
            }

            draw->AddPolyline(points, static_cast<int>(FrameHistory::Size), color, ImDrawFlags_None, 1.5f);
        };

        drawSeries(history.cpuMs, ToU32(colors.textDim));
        drawSeries(history.gpuMs, ToU32(colors.accent));
    }

    void ProfilerPanel::DrawPassTable(EditorContext& context)
    {
        const RenderFrameStats& stats = context.renderer.GetFrameStats();

        struct Row
        {
            std::string name;
            float milliseconds;
        };

        std::vector<Row> rows;
        float total = 0.0f;

        for (const RenderPassInfo& pass : stats.passes)
        {
            if (pass.culled)
                continue;

            float& smoothed = smoothedPassMs_[pass.name];
            smoothed += (pass.gpuMilliseconds - smoothed) * PassSmoothing;
            rows.push_back({ ToUtf8(pass.name), smoothed });
            total += smoothed;
        }

        const ImGuiTableFlags flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                      ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

        if (!ImGui::BeginTable("Passes", 3, flags))
            return;

        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort |
                                              ImGuiTableColumnFlags_PreferSortDescending,
                                ImGui::GetFontSize() * 5.0f);
        ImGui::TableSetupColumn("Share", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoSort, 0.6f);
        ImGui::TableHeadersRow();

        if (const ImGuiTableSortSpecs* sort = ImGui::TableGetSortSpecs(); sort && sort->SpecsCount > 0)
        {
            const ImGuiTableColumnSortSpecs& spec = sort->Specs[0];
            const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;

            std::ranges::sort(rows, [&](const Row& a, const Row& b) {
                if (spec.ColumnIndex == 0)
                    return ascending ? a.name < b.name : a.name > b.name;

                return ascending ? a.milliseconds < b.milliseconds : a.milliseconds > b.milliseconds;
            });
        }

        const ThemeColors& colors = GetThemeColors();

        // Pushed inside the table's scrolling child window, so it is popped before EndTable.
        ImGui::PushFont(GetFont(FontRole::Mono));

        for (const Row& row : rows)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%7.3f", row.milliseconds);
            ImGui::TableSetColumnIndex(2);

            const float share = total > 0.0f ? row.milliseconds / total : 0.0f;
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            const float height = ImGui::GetTextLineHeight();
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(min.x, min.y + 2.0f),
                                                      ImVec2(min.x + width * share, min.y + height - 2.0f),
                                                      ToU32(colors.accent, 0.6f), 2.0f);
            ImGui::Text("%4.1f%%", share * 100.0f);
        }

        ImGui::PopFont();
        ImGui::EndTable();
    }

    void ProfilerPanel::DrawCounters(EditorContext& context)
    {
        const RenderFrameStats& stats = context.renderer.GetFrameStats();

        // VRAM changes slowly and the query is not free, so refresh it twice a second.
        videoMemoryAge_ += context.deltaSeconds;

        if (videoMemoryAge_ > 0.5f)
        {
            videoMemory_ = context.renderer.GetDevice().QueryVideoMemory();
            videoMemoryAge_ = 0.0f;
        }

        if (!ImGui::BeginTable("Counters", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
            return;

        ImGui::TableSetupColumn("Counter", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.5f);

        auto row = [](const char* name, const std::string& value) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            widgets::TextDim("%s", name);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(value.c_str());
        };

        constexpr double MB = 1024.0 * 1024.0;
        row("Resolution", std::format("{} x {}", stats.renderWidth, stats.renderHeight));
        row("Draws (camera)", std::format("{} of {}", stats.scene.visibleDraws, stats.scene.scene.draws));
        row("Instances", std::to_string(stats.scene.scene.instances));
        row("Materials", std::to_string(stats.scene.scene.materials));
        row("Lights", std::format("{} ({} with shadows)", stats.scene.lights, stats.scene.shadowedLights));
        row("Shadow views", std::format("{} ({} draws)", stats.scene.shadowViews, stats.scene.shadowDraws));
        row("Pooled resources", std::format("{} textures, {} buffers", stats.pooledTextures, stats.pooledBuffers));
        row("Video memory", std::format("{:.0f} / {:.0f} MB", videoMemory_.usedBytes / MB, videoMemory_.budgetBytes / MB));
        row("GPU", ToUtf8(context.renderer.GetDevice().GetAdapterName()));

        ImGui::EndTable();
    }
}
