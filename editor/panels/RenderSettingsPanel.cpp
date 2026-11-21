#include "panels/RenderSettingsPanel.h"
#include "EditorContext.h"
#include "widgets/PropertyGrid.h"
#include "widgets/Widgets.h"

#include "graphics/Renderer.h"

#include <array>

namespace atmos::editor
{
    namespace
    {
        struct QualityPreset
        {
            const char* name;
            uint32_t cascadeResolution;
            ShadowFilter shadowFilter;
            float shadowDistance;
            uint32_t maxShadowedLights;
        };

        // "High" matches the RenderSettings defaults.
        constexpr QualityPreset Presets[] = {
            { "Low", 1024, ShadowFilter::Hard, 30.0f, 2 },
            { "Medium", 2048, ShadowFilter::Pcf3x3, 45.0f, 4 },
            { "High", 2048, ShadowFilter::Pcf3x3, 60.0f, 8 },
            { "Ultra", 4096, ShadowFilter::Pcf3x3, 100.0f, 10 },
        };

        constexpr uint32_t CascadeResolutions[] = { 1024, 2048, 4096 };
        constexpr const char* ShadowFilterNames[] = { "Hard", "PCF 3x3" };

        // The preset the settings match exactly, or none ("Custom").
        int FindPreset(const RenderSettings& settings)
        {
            for (int i = 0; i < static_cast<int>(std::size(Presets)); ++i)
            {
                const QualityPreset& preset = Presets[i];

                if (settings.cascadeResolution == preset.cascadeResolution && settings.shadowFilter == preset.shadowFilter &&
                    settings.shadowDistance == preset.shadowDistance && settings.maxShadowedLights == preset.maxShadowedLights)
                    return i;
            }

            return -1;
        }

        void ApplyPreset(RenderSettings& settings, const QualityPreset& preset)
        {
            settings.cascadeResolution = preset.cascadeResolution;
            settings.shadowFilter = preset.shadowFilter;
            settings.shadowDistance = preset.shadowDistance;
            settings.maxShadowedLights = preset.maxShadowedLights;
        }

        // Right-click menu of a category header; returns true when the category was reset.
        bool ResetMenu(const char* id)
        {
            bool reset = false;

            if (ImGui::BeginPopup(id))
            {
                reset = ImGui::MenuItem("Reset to Defaults");
                ImGui::EndPopup();
            }

            return reset;
        }
    }

    RenderSettingsPanel::RenderSettingsPanel() : Panel("Render Settings", Icon::RenderSettings)
    {
    }

    void RenderSettingsPanel::DrawContent(EditorContext& context)
    {
        RenderSettings& settings = context.document.GetScene().renderSettings;
        const RenderSettings defaults;
        bool changed = false;

        // Quality preset
        {
            const int preset = FindPreset(settings);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Quality");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

            if (ImGui::BeginCombo("##Quality", preset >= 0 ? Presets[preset].name : "Custom"))
            {
                for (int i = 0; i < static_cast<int>(std::size(Presets)); ++i)
                {
                    if (ImGui::Selectable(Presets[i].name, i == preset))
                    {
                        ApplyPreset(settings, Presets[i]);
                        changed = true;
                    }
                }

                ImGui::EndCombo();
            }

            widgets::Tooltip("Sets shadow resolution, filtering, distance and shadowed-light count together");
        }

        ImGui::Spacing();

        // Sun shadows
        {
            const widgets::SectionState section =
                widgets::SectionHeader("Sun Shadows", Icon::Sun, &settings.sunShadows, "SunShadowsMenu");
            changed |= section.toggled;

            if (ResetMenu("SunShadowsMenu"))
            {
                settings.sunShadows = defaults.sunShadows;
                settings.shadowDistance = defaults.shadowDistance;
                settings.cascadeSplitLambda = defaults.cascadeSplitLambda;
                settings.cascadeResolution = defaults.cascadeResolution;
                settings.shadowFilter = defaults.shadowFilter;
                settings.shadowNormalOffset = defaults.shadowNormalOffset;
                changed = true;
            }

            if (section.open)
            {
                ImGui::BeginDisabled(!settings.sunShadows);
                {
                    PropertyGrid grid("SunShadows");
                    changed |= grid.Slider("Distance", settings.shadowDistance, defaults.shadowDistance, 10.0f, 300.0f,
                                           "%.0f m", false, "How far from the camera the cascades reach");
                    changed |= grid.Slider("Cascade split", settings.cascadeSplitLambda, defaults.cascadeSplitLambda, 0.0f,
                                           1.0f, "%.2f", false, "0 = even splits, 1 = logarithmic (sharper near the camera)");
                    changed |= grid.Choice("Resolution", settings.cascadeResolution, CascadeResolutions,
                                           defaults.cascadeResolution, "Pixels per side of each of the 4 cascades");

                    auto filter = static_cast<uint32_t>(settings.shadowFilter);

                    if (grid.Enum("Filter", filter, ShadowFilterNames, static_cast<uint32_t>(defaults.shadowFilter)))
                    {
                        settings.shadowFilter = static_cast<ShadowFilter>(filter);
                        changed = true;
                    }

                    changed |= grid.Slider("Normal offset", settings.shadowNormalOffset, defaults.shadowNormalOffset, 0.0f,
                                           5.0f, "%.1f texels", false, "Pushes receivers along their normal to avoid acne");
                }

                ImGui::EndDisabled();
            }
        }

        // Local light shadows
        {
            const widgets::SectionState section =
                widgets::SectionHeader("Local Light Shadows", Icon::SpotLight, &settings.localShadows, "LocalShadowsMenu");
            changed |= section.toggled;

            if (ResetMenu("LocalShadowsMenu"))
            {
                settings.localShadows = defaults.localShadows;
                settings.maxShadowedLights = defaults.maxShadowedLights;
                changed = true;
            }

            if (section.open)
            {
                ImGui::BeginDisabled(!settings.localShadows);
                {
                    PropertyGrid grid("LocalShadows");
                    changed |= grid.UInt("Max shadowed lights", settings.maxShadowedLights, defaults.maxShadowedLights, 0, 16,
                                         "Shadow-casting lights nearest the camera get atlas tiles first");
                    grid.ReadOnly("Atlas", "4096 px, 512 px tiles");
                }

                ImGui::EndDisabled();
            }
        }

        // Sky lighting
        {
            const widgets::SectionState section =
                widgets::SectionHeader("Sky Lighting", Icon::Globe, &settings.imageBasedLighting, "SkyLightingMenu");
            changed |= section.toggled;

            if (ResetMenu("SkyLightingMenu"))
            {
                settings.imageBasedLighting = defaults.imageBasedLighting;
                settings.ambientIntensity = defaults.ambientIntensity;
                settings.reflectionIntensity = defaults.reflectionIntensity;
                changed = true;
            }

            if (section.open)
            {
                ImGui::BeginDisabled(!settings.imageBasedLighting);
                {
                    PropertyGrid grid("SkyLighting");
                    changed |= grid.Slider("Diffuse intensity", settings.ambientIntensity, defaults.ambientIntensity, 0.0f,
                                           3.0f, "%.2f", false, "Scales the sky's diffuse (irradiance) lighting");
                    changed |= grid.Slider("Reflection intensity", settings.reflectionIntensity, defaults.reflectionIntensity,
                                           0.0f, 3.0f, "%.2f", false, "Scales the sky's specular reflections");
                }

                ImGui::EndDisabled();

                if (ImGui::Button((std::string(Icon::Refresh) + "  Recapture Sky").c_str()))
                    context.renderer.RequestSkyLightRecapture();

                widgets::Tooltip("The sky is captured automatically when the sun moves");
            }
        }

        // Light statistics
        if (widgets::SectionHeader("Lights", Icon::PointLight, nullptr, nullptr, false).open)
        {
            const RenderFrameStats& stats = context.renderer.GetFrameStats();
            PropertyGrid grid("Lights");
            grid.ReadOnly("Lights", "%u", stats.scene.lights);
            grid.ReadOnly("With shadows", "%u", stats.scene.shadowedLights);
            grid.ReadOnly("Shadow views", "%u (%u draws)", stats.scene.shadowViews, stats.scene.shadowDraws);
            grid.ReadOnly("Light grid", "16 x 9 x 24, 128 per cluster");
        }

        if (changed)
            context.document.MarkEdited("Render Settings");
    }
}
