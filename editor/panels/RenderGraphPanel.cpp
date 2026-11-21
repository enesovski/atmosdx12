#include "panels/RenderGraphPanel.h"
#include "EditorContext.h"
#include "style/EditorFonts.h"
#include "style/EditorTheme.h"
#include "widgets/Widgets.h"

#include "core/StringUtils.h"
#include "graphics/Renderer.h"


namespace atmos::editor
{
    RenderGraphPanel::RenderGraphPanel() : Panel("Render Graph", Icon::RenderGraph)
    {
    }

    void RenderGraphPanel::DrawContent(EditorContext& context)
    {
        const RenderFrameStats& stats = context.renderer.GetFrameStats();
        const ThemeColors& colors = GetThemeColors();

        size_t livePasses = 0;

        for (const RenderPassInfo& pass : stats.passes)
            livePasses += !pass.culled;

        widgets::TextDim("%zu passes (%zu live), %zu resources", stats.passes.size(), livePasses, stats.resources.size());
        ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight());

        if (widgets::IconButton("dump", Icon::Console, "Log the next frame's graph to the console"))
            context.renderer.RequestFrameDump();

        const float listWidth = ImGui::GetContentRegionAvail().x * 0.45f;
        const ScopedFont mono(FontRole::Mono);

        // Passes
        ImGui::BeginChild("Passes", ImVec2(listWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
        const RenderPassInfo* selected = nullptr;

        if (ImGui::BeginTable("PassList", 2, ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("GPU", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("00.000 ms").x);

            for (const RenderPassInfo& pass : stats.passes)
            {
                const bool isSelected = pass.name == selectedPass_;

                if (isSelected)
                    selected = &pass;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, pass.culled ? colors.textDim : colors.text);

                if (ImGui::Selectable(ToUtf8(pass.name).c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
                    selectedPass_ = pass.name;

                ImGui::PopStyleColor();
                ImGui::TableSetColumnIndex(1);

                if (pass.culled)
                    widgets::TextDim("culled");
                else
                    widgets::TextDim("%.3f ms", pass.gpuMilliseconds);
            }

            ImGui::EndTable();
        }

        ImGui::EndChild();
        ImGui::SameLine();

        // Details of the selected pass, then the resource list.
        ImGui::BeginChild("Details", ImVec2(0, 0), ImGuiChildFlags_Borders);

        if (selected)
        {
            ImGui::TextUnformatted(ToUtf8(selected->name).c_str());
            widgets::TextDim(selected->culled ? "Culled: nothing reads its output" : "Live");
            ImGui::Spacing();

            for (const std::string& access : selected->accesses)
                ImGui::BulletText("%s", access.c_str());
        }
        else
        {
            widgets::TextDim("Select a pass to see what it reads and writes.");
        }

        ImGui::Spacing();
        ImGui::SeparatorText("Resources");

        if (ImGui::BeginTable("Resources", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 0.45f);
            ImGui::TableSetupColumn("Lifetime", ImGuiTableColumnFlags_WidthStretch, 0.15f);
            ImGui::TableHeadersRow();

            for (const RenderResourceInfo& resource : stats.resources)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(ToUtf8(resource.name).c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(resource.description.c_str());
                ImGui::TableSetColumnIndex(2);
                widgets::TextDim(resource.imported ? "imported" : "transient");
            }

            ImGui::EndTable();
        }

        ImGui::EndChild();
    }
}
