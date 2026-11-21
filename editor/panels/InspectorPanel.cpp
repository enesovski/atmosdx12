#include "panels/InspectorPanel.h"
#include "EditorContext.h"
#include "style/EditorFonts.h"
#include "style/EditorTheme.h"
#include "tools/FileDialogs.h"
#include "widgets/PropertyGrid.h"
#include "widgets/Widgets.h"

#include "core/StringUtils.h"
#include "graphics/Material.h"
#include "graphics/Mesh.h"
#include "runtime/AssetManager.h"
#include "runtime/RenderSceneBuilder.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

using namespace DirectX;

namespace atmos::editor
{
    namespace
    {
        // Blackbody color for a temperature (Tanner Helland's fit, normalized to the brightest channel).
        XMFLOAT3 KelvinToColor(float kelvin)
        {
            const float t = std::clamp(kelvin, 1000.0f, 40000.0f) / 100.0f;
            float r, g, b;

            if (t <= 66.0f)
            {
                r = 255.0f;
                g = 99.4708025861f * std::log(t) - 161.1195681661f;
                b = t <= 19.0f ? 0.0f : 138.5177312231f * std::log(t - 10.0f) - 305.0447927307f;
            }
            else
            {
                r = 329.698727446f * std::pow(t - 60.0f, -0.1332047592f);
                g = 288.1221695283f * std::pow(t - 60.0f, -0.0755148492f);
                b = 255.0f;
            }

            r = std::clamp(r, 0.0f, 255.0f) / 255.0f;
            g = std::clamp(g, 0.0f, 255.0f) / 255.0f;
            b = std::clamp(b, 0.0f, 255.0f) / 255.0f;

            // The fit is in display (sRGB) space; light colors are linear.
            auto toLinear = [](float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); };
            return { toLinear(r), toLinear(g), toLinear(b) };
        }

        const IconGlyph& GetEntityIcon(const Entity& entity)
        {
            if (entity.light)
                return entity.light->type == LightType::Spot ? Icon::SpotLight : Icon::PointLight;

            return entity.model ? Icon::Model : Icon::Entity;
        }

        // Compass for the sun: the rim is the horizon, the center is straight up. Drag to move the sun.
        bool SunCompass(SceneEnvironment& environment)
        {
            const ThemeColors& colors = GetThemeColors();
            const float radius = ImGui::GetFontSize() * 4.0f;
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const ImVec2 center(origin.x + ImGui::GetContentRegionAvail().x * 0.5f, origin.y + radius + 4.0f);

            ImGui::InvisibleButton("SunCompass", ImVec2(ImGui::GetContentRegionAvail().x, radius * 2.0f + 8.0f));
            bool changed = false;

            if (ImGui::IsItemActive())
            {
                const ImVec2 mouse = ImGui::GetMousePos();
                const float dx = mouse.x - center.x;
                const float dy = mouse.y - center.y;
                const float distance = std::min(std::hypot(dx, dy) / radius, 1.0f);

                // Up on screen is +Z (azimuth 0), right is +X (azimuth 90).
                float azimuth = std::atan2(dx, -dy) * 180.0f / std::numbers::pi_v<float>;
                environment.sunAzimuthDegrees = azimuth < 0.0f ? azimuth + 360.0f : azimuth;
                environment.sunElevationDegrees = 90.0f * (1.0f - distance);
                changed = true;
            }

            widgets::Tooltip("Drag to move the sun: the rim is the horizon, the center is overhead");

            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddCircleFilled(center, radius, ToU32(colors.field));
            draw->AddCircle(center, radius, ToU32(colors.border), 0, 1.0f);
            draw->AddCircle(center, radius * 0.5f, ToU32(colors.textDim, 0.25f), 0, 1.0f);
            draw->AddLine(ImVec2(center.x - radius, center.y), ImVec2(center.x + radius, center.y), ToU32(colors.textDim, 0.25f));
            draw->AddLine(ImVec2(center.x, center.y - radius), ImVec2(center.x, center.y + radius), ToU32(colors.textDim, 0.25f));

            ImFont* font = GetFont(FontRole::Small);
            draw->AddText(font, font->FontSize, ImVec2(center.x - font->FontSize * 0.3f, center.y - radius - font->FontSize - 1.0f),
                          ToU32(colors.textDim), "Z");
            draw->AddText(font, font->FontSize, ImVec2(center.x + radius + 3.0f, center.y - font->FontSize * 0.5f),
                          ToU32(colors.textDim), "X");

            const float azimuth = environment.sunAzimuthDegrees * std::numbers::pi_v<float> / 180.0f;
            const float distance = std::clamp(1.0f - environment.sunElevationDegrees / 90.0f, 0.0f, 1.0f) * radius;
            const ImVec2 sun(center.x + std::sin(azimuth) * distance, center.y - std::cos(azimuth) * distance);
            draw->AddCircleFilled(sun, ImGui::GetFontSize() * 0.4f, ToU32(colors.warning));
            return changed;
        }
    }

    InspectorPanel::InspectorPanel() : Panel("Inspector", Icon::Inspector)
    {
    }

    void InspectorPanel::DrawContent(EditorContext& context)
    {
        EditorSelection& selection = context.document.GetSelection();

        if (selection.IsEnvironmentSelected())
        {
            DrawEnvironment(context);
            return;
        }

        const EntityId id = selection.GetPrimary();
        Scene& scene = context.document.GetScene();

        if (!scene.IsValid(id))
        {
            widgets::EmptyState(Icon::Inspector, "Select an entity to see its details");
            return;
        }

        Entity& entity = scene.GetEntity(id);
        DrawHeader(context, id);
        DrawTransform(context, entity);

        if (entity.light)
            DrawLight(context, entity);

        if (entity.model)
            DrawModel(context, entity);

        DrawAddComponent(context, entity);
    }

    void InspectorPanel::DrawHeader(EditorContext& context, EntityId id)
    {
        Entity& entity = context.document.GetScene().GetEntity(id);
        const size_t selectedCount = context.document.GetSelection().GetEntities().size();

        if (ImGui::Checkbox("##active", &entity.active))
            context.document.MarkEdited(entity.active ? "Activate" : "Deactivate");

        widgets::Tooltip("Active: shown in the scene, with its children");
        ImGui::SameLine();

        {
            const ScopedFont bold(FontRole::Bold);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(GetEntityIcon(entity));
        }

        ImGui::SameLine();
        std::string name = entity.name;

        if (widgets::InputString("##name", name, ImGui::GetContentRegionAvail().x))
            context.document.Rename(id, name);

        if (selectedCount > 1)
            widgets::TextDim("%zu selected; editing the last one", selectedCount);

        ImGui::Spacing();
    }

    void InspectorPanel::DrawTransform(EditorContext& context, Entity& entity)
    {
        if (!widgets::SectionHeader("Transform", Icon::Move).open)
            return;

        PropertyGrid grid("Transform");
        Transform& transform = entity.transform;

        if (grid.Float3("Position", transform.position, { 0, 0, 0 }, 0.01f, "%.2f"))
            context.document.MarkEdited("Move");

        if (grid.Float3("Rotation", transform.rotationDegrees, { 0, 0, 0 }, 0.25f, "%.1f", "Degrees: pitch (X), yaw (Y), roll (Z)"))
            context.document.MarkEdited("Rotate");

        const XMFLOAT3 previousScale = transform.scale;

        if (grid.Float3("Scale", transform.scale, { 1, 1, 1 }, 0.005f, "%.3f"))
        {
            // Uniform: the edited axis scales the others by the same factor.
            if (uniformScale_)
            {
                const float* before = &previousScale.x;
                float* after = &transform.scale.x;

                for (int axis = 0; axis < 3; ++axis)
                {
                    if (after[axis] != before[axis] && before[axis] != 0.0f)
                    {
                        const float factor = after[axis] / before[axis];
                        transform.scale = { previousScale.x * factor, previousScale.y * factor, previousScale.z * factor };
                        break;
                    }
                }
            }

            context.document.MarkEdited("Scale");
        }

        grid.Bool("Uniform scale", uniformScale_, true, "Scale all axes together");
    }

    void InspectorPanel::DrawLight(EditorContext& context, Entity& entity)
    {
        const IconGlyph& icon = entity.light->type == LightType::Spot ? Icon::SpotLight : Icon::PointLight;
        const bool open = widgets::SectionHeader("Light", icon, nullptr, "LightMenu").open;

        if (ImGui::BeginPopup("LightMenu"))
        {
            if (ImGui::MenuItem("Remove Component"))
            {
                entity.light.reset();
                context.document.MarkEdited("Remove Light");
            }

            ImGui::EndPopup();
        }

        if (!open || !entity.light)
            return;

        LightComponent& light = *entity.light;
        const LightComponent defaults;
        PropertyGrid grid("Light");
        bool changed = false;

        constexpr const char* typeNames[] = { "Point", "Spot" };
        auto type = static_cast<uint32_t>(light.type);

        if (grid.Enum("Type", type, typeNames, 0))
        {
            light.type = static_cast<LightType>(type);
            changed = true;
        }

        changed |= grid.Color("Color", light.color, defaults.color);

        if (grid.Slider("Temperature", temperatureKelvin_, 6500.0f, 1500.0f, 12000.0f, "%.0f K", false,
                        "Sets the color from a blackbody temperature: candle 1900 K, tungsten 2700 K, daylight 6500 K"))
        {
            light.color = KelvinToColor(temperatureKelvin_);
            changed = true;
        }

        changed |= grid.Float("Intensity", light.intensity, defaults.intensity, 0.0f, 10000.0f, "%.1f cd", 0.1f,
                              "Luminous intensity, on the same scale as the sun's illuminance");
        changed |= grid.Float("Range", light.range, defaults.range, 0.1f, 200.0f, "%.1f m", 0.05f,
                              "The light fades to zero at this distance");

        if (light.type == LightType::Spot)
        {
            changed |= grid.Slider("Outer cone", light.outerConeDegrees, defaults.outerConeDegrees, 1.0f, 80.0f, "%.0f deg");
            changed |= grid.Slider("Inner cone", light.innerConeDegrees, defaults.innerConeDegrees, 0.0f,
                                   light.outerConeDegrees, "%.0f deg", false, "Full intensity inside this angle");
            light.innerConeDegrees = std::min(light.innerConeDegrees, light.outerConeDegrees);
        }

        changed |= grid.Bool("Cast shadows", light.castsShadows, defaults.castsShadows,
                             "Uses shadow-atlas tiles: 1 for a spot light, 6 for a point light");

        if (changed)
            context.document.MarkEdited("Edit Light");
    }

    void InspectorPanel::DrawModel(EditorContext& context, Entity& entity)
    {
        const bool open = widgets::SectionHeader("Model", Icon::Model, nullptr, "ModelMenu").open;

        if (ImGui::BeginPopup("ModelMenu"))
        {
            if (ImGui::MenuItem("Remove Component"))
            {
                entity.model.reset();
                context.document.MarkEdited("Remove Model");
            }

            ImGui::EndPopup();
        }

        if (!open || !entity.model)
            return;

        ModelComponent& model = *entity.model;
        const LoadedModel* loaded = context.assets.GetModel(model.handle);

        {
            PropertyGrid grid("Model");
            grid.ReadOnly("File", "%s", ToUtf8(model.path.filename().wstring()).c_str());
            widgets::Tooltip(ToUtf8(model.path.wstring()).c_str());

            if (!loaded)
            {
                grid.ReadOnly("Status", "Loading...");
                return;
            }

            size_t submeshes = 0;
            size_t triangles = 0;

            for (const auto& mesh : loaded->meshes)
            {
                if (!mesh)
                    continue;

                for (const Submesh& submesh : mesh->GetSubmeshes())
                {
                    ++submeshes;
                    triangles += submesh.indexCount / 3;
                }
            }

            grid.ReadOnly("Meshes", "%zu (%zu submeshes)", loaded->meshes.size(), submeshes);
            grid.ReadOnly("Triangles", "%zu", triangles);
            grid.ReadOnly("Instances", "%zu", loaded->instances.size());
        }

        if (ImGui::Button((std::string(Icon::OpenFile) + "  Change Model...").c_str()))
        {
            if (const auto path = ShowOpenDialog(GetActiveWindow(), FileKind::Model, model.path.parent_path()))
            {
                model = ModelComponent{ .path = std::filesystem::absolute(*path) };
                RequestSceneAssets(context.document.GetScene(), context.assets);
                context.document.MarkEdited("Change Model");
                return;
            }
        }

        const std::string header = std::format("Materials ({})", loaded->materials.size());

        if (!ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
            return;

        constexpr const char* alphaModes[] = { "Opaque", "Masked", "Blended" };
        constexpr const char* slotNames[] = { "Base color", "Normal", "Metal/rough", "Occlusion", "Emissive" };

        for (size_t i = 0; i < loaded->materials.size(); ++i)
        {
            const MaterialData& material = loaded->materials[i].data;
            ImGui::PushID(static_cast<int>(i));

            const XMFLOAT4& factor = material.baseColorFactor;
            ImGui::ColorButton("##color", ImVec4(factor.x, factor.y, factor.z, 1.0f), ImGuiColorEditFlags_NoTooltip,
                               ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()));
            ImGui::SameLine();
            const std::string name = material.name.empty() ? std::format("Material {}", i) : material.name;

            if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
            {
                widgets::TextDim("%s%s, metallic %.2f, roughness %.2f", alphaModes[static_cast<size_t>(material.alphaMode)],
                                 material.doubleSided ? ", double-sided" : "", material.metallicFactor,
                                 material.roughnessFactor);

                for (size_t slot = 0; slot < std::size(slotNames); ++slot)
                {
                    if (!material.textures[slot].empty())
                        widgets::TextDim("%-12s %s", slotNames[slot],
                                         ToUtf8(material.textures[slot].filename().wstring()).c_str());
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
    }

    void InspectorPanel::DrawAddComponent(EditorContext& context, Entity& entity)
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float width = std::min(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 14.0f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - width) * 0.5f);

        if (ImGui::Button((std::string(Icon::Add) + "  Add Component").c_str(), ImVec2(width, 0)))
            ImGui::OpenPopup("AddComponent");

        if (!ImGui::BeginPopup("AddComponent"))
            return;

        if (ImGui::MenuItem((std::string(Icon::PointLight) + "  Point Light").c_str(), nullptr, false, !entity.light))
        {
            entity.light = LightComponent{ .intensity = 10.0f, .range = 8.0f };
            context.document.MarkEdited("Add Light");
        }

        if (ImGui::MenuItem((std::string(Icon::SpotLight) + "  Spot Light").c_str(), nullptr, false, !entity.light))
        {
            entity.light = LightComponent{ .type = LightType::Spot, .intensity = 40.0f, .range = 12.0f };
            context.document.MarkEdited("Add Light");
        }

        if (ImGui::MenuItem((std::string(Icon::Model) + "  Model...").c_str(), nullptr, false, !entity.model))
        {
            if (const auto path = ShowOpenDialog(GetActiveWindow(), FileKind::Model, context.paths.assets))
            {
                entity.model = ModelComponent{ .path = std::filesystem::absolute(*path) };
                RequestSceneAssets(context.document.GetScene(), context.assets);
                context.document.MarkEdited("Add Model");
            }
        }

        ImGui::EndPopup();
    }

    void InspectorPanel::DrawEnvironment(EditorContext& context)
    {
        SceneEnvironment& environment = context.document.GetScene().environment;
        const SceneEnvironment defaults;

        {
            const ScopedFont bold(FontRole::Bold);
            ImGui::Text("%s  Sky & Sun", Icon::Sun.utf8);
        }

        widgets::TextDim("Sun light, sky lighting and exposure for the scene.");
        ImGui::Spacing();

        bool changed = false;

        if (widgets::SectionHeader("Sun", Icon::Sun).open)
        {
            changed |= SunCompass(environment);

            PropertyGrid grid("Sun");
            changed |= grid.Slider("Azimuth", environment.sunAzimuthDegrees, defaults.sunAzimuthDegrees, 0.0f, 360.0f,
                                   "%.0f deg", false, "Compass direction: 0 = +Z, 90 = +X");
            changed |= grid.Slider("Elevation", environment.sunElevationDegrees, defaults.sunElevationDegrees, -10.0f,
                                   90.0f, "%.1f deg", false, "Height above the horizon");
            changed |= grid.Float("Intensity", environment.sunIntensity, defaults.sunIntensity, 0.0f, 200.0f, "%.2f lx",
                                  0.02f, "Illuminance at noon; lights use the same scale");
        }

        if (widgets::SectionHeader("Camera Exposure", Icon::Exposure).open)
        {
            PropertyGrid grid("Exposure");
            changed |= grid.Slider("Exposure", environment.exposure, defaults.exposure, 0.05f, 20.0f, "%.2f x", true,
                                   "Linear scale applied before tonemapping");
        }

        if (changed)
            context.document.MarkEdited("Edit Sky & Sun");
    }
}
