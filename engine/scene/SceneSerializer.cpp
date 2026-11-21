#include "scene/SceneSerializer.h"
#include "core/StringUtils.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

using nlohmann::json;

namespace atmos
{
    namespace
    {
        // 2 added "active", "renderSettings" and "cameraBookmarks"; version 1 files still load.
        constexpr int FormatVersion = 2;
        constexpr int OldestReadableVersion = 1;

        json ToJson(const DirectX::XMFLOAT3& value)
        {
            return json::array({ value.x, value.y, value.z });
        }

        DirectX::XMFLOAT3 ReadFloat3(const json& node, const char* key, DirectX::XMFLOAT3 fallback)
        {
            if (!node.contains(key))
                return fallback;

            const json& value = node.at(key);
            return { value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>() };
        }

        LightComponent ReadLight(const json& node)
        {
            LightComponent light;
            light.type = node.value("type", std::string("point")) == "spot" ? LightType::Spot : LightType::Point;
            light.color = ReadFloat3(node, "color", light.color);
            light.intensity = node.value("intensity", light.intensity);
            light.range = node.value("range", light.range);
            light.innerConeDegrees = node.value("innerConeDegrees", light.innerConeDegrees);
            light.outerConeDegrees = node.value("outerConeDegrees", light.outerConeDegrees);
            light.castsShadows = node.value("castsShadows", light.castsShadows);
            return light;
        }

        json WriteLight(const LightComponent& light)
        {
            json node = {
                { "type", light.type == LightType::Spot ? "spot" : "point" },
                { "color", ToJson(light.color) },
                { "intensity", light.intensity },
                { "range", light.range },
                { "castsShadows", light.castsShadows },
            };

            if (light.type == LightType::Spot)
            {
                node["innerConeDegrees"] = light.innerConeDegrees;
                node["outerConeDegrees"] = light.outerConeDegrees;
            }

            return node;
        }

        std::string ToGenericUtf8(const std::filesystem::path& path)
        {
            return ToUtf8(path.generic_wstring());
        }

        SceneCamera ReadCamera(const json& node, SceneCamera camera)
        {
            camera.position = ReadFloat3(node, "position", camera.position);
            camera.yawDegrees = node.value("yawDegrees", camera.yawDegrees);
            camera.pitchDegrees = node.value("pitchDegrees", camera.pitchDegrees);
            camera.fovDegrees = node.value("fovDegrees", camera.fovDegrees);
            return camera;
        }

        json WriteCamera(const SceneCamera& camera)
        {
            return {
                { "position", ToJson(camera.position) },
                { "yawDegrees", camera.yawDegrees },
                { "pitchDegrees", camera.pitchDegrees },
                { "fovDegrees", camera.fovDegrees },
            };
        }

        // Missing keys keep their defaults, so settings added later never break old files.
        RenderSettings ReadRenderSettings(const json& node)
        {
            RenderSettings s;
            s.sunShadows = node.value("sunShadows", s.sunShadows);
            s.shadowDistance = node.value("shadowDistance", s.shadowDistance);
            s.cascadeSplitLambda = node.value("cascadeSplitLambda", s.cascadeSplitLambda);
            s.cascadeResolution = node.value("cascadeResolution", s.cascadeResolution);
            s.shadowFilter = node.value("shadowFilter", std::string("pcf")) == "hard" ? ShadowFilter::Hard
                                                                                      : ShadowFilter::Pcf3x3;
            s.shadowNormalOffset = node.value("shadowNormalOffset", s.shadowNormalOffset);
            s.localShadows = node.value("localShadows", s.localShadows);
            s.maxShadowedLights = node.value("maxShadowedLights", s.maxShadowedLights);
            s.imageBasedLighting = node.value("imageBasedLighting", s.imageBasedLighting);
            s.ambientIntensity = node.value("ambientIntensity", s.ambientIntensity);
            s.reflectionIntensity = node.value("reflectionIntensity", s.reflectionIntensity);
            return s;
        }

        json WriteRenderSettings(const RenderSettings& s)
        {
            return {
                { "sunShadows", s.sunShadows },
                { "shadowDistance", s.shadowDistance },
                { "cascadeSplitLambda", s.cascadeSplitLambda },
                { "cascadeResolution", s.cascadeResolution },
                { "shadowFilter", s.shadowFilter == ShadowFilter::Hard ? "hard" : "pcf" },
                { "shadowNormalOffset", s.shadowNormalOffset },
                { "localShadows", s.localShadows },
                { "maxShadowedLights", s.maxShadowedLights },
                { "imageBasedLighting", s.imageBasedLighting },
                { "ambientIntensity", s.ambientIntensity },
                { "reflectionIntensity", s.reflectionIntensity },
            };
        }
    }

    Scene LoadScene(const std::filesystem::path& path)
    {
        std::ifstream stream(path);

        if (!stream)
            throw std::runtime_error("Failed to open scene " + ToUtf8(path.wstring()));

        const json root = json::parse(stream);

        const int version = root.value("version", 0);

        if (version < OldestReadableVersion || version > FormatVersion)
            throw std::runtime_error("Unsupported scene version in " + ToUtf8(path.wstring()));

        Scene scene;
        const std::filesystem::path baseDirectory = path.parent_path();

        if (root.contains("environment"))
        {
            const json& environment = root.at("environment");
            SceneEnvironment& target = scene.environment;
            target.sunAzimuthDegrees = environment.value("sunAzimuthDegrees", target.sunAzimuthDegrees);
            target.sunElevationDegrees = environment.value("sunElevationDegrees", target.sunElevationDegrees);
            target.sunIntensity = environment.value("sunIntensity", target.sunIntensity);
            target.exposure = environment.value("exposure", target.exposure);
        }

        if (root.contains("camera"))
            scene.camera = ReadCamera(root.at("camera"), scene.camera);

        if (root.contains("renderSettings"))
            scene.renderSettings = ReadRenderSettings(root.at("renderSettings"));

        for (const json& bookmark : root.value("cameraBookmarks", json::array()))
        {
            const size_t slot = bookmark.value("slot", size_t{ 0 });

            if (slot >= 1 && slot <= Scene::BookmarkCount)
                scene.cameraBookmarks[slot - 1] = ReadCamera(bookmark, {});
        }

        for (const json& node : root.value("entities", json::array()))
        {
            const int parent = node.value("parent", -1);

            if (parent >= static_cast<int>(scene.GetEntities().size()))
                throw std::runtime_error("Entity parent must come before the entity");

            const EntityId id = scene.CreateEntity(node.value("name", std::string("Entity")),
                                                   parent < 0 ? InvalidEntity : static_cast<EntityId>(parent));
            Entity& entity = scene.GetEntity(id);
            entity.transform.position = ReadFloat3(node, "position", entity.transform.position);
            entity.transform.rotationDegrees = ReadFloat3(node, "rotationDegrees", entity.transform.rotationDegrees);
            entity.transform.scale = ReadFloat3(node, "scale", entity.transform.scale);
            entity.active = node.value("active", true);

            if (node.contains("model"))
            {
                const std::filesystem::path relative = ToWide(node.at("model").get<std::string>());
                entity.model = ModelComponent{ .path = (baseDirectory / relative).lexically_normal() };
            }

            if (node.contains("light"))
                entity.light = ReadLight(node.at("light"));
        }

        return scene;
    }

    void SaveScene(const Scene& scene, const std::filesystem::path& path)
    {
        const std::filesystem::path baseDirectory = std::filesystem::absolute(path).parent_path();

        json root;
        root["version"] = FormatVersion;
        root["environment"] = {
            { "sunAzimuthDegrees", scene.environment.sunAzimuthDegrees },
            { "sunElevationDegrees", scene.environment.sunElevationDegrees },
            { "sunIntensity", scene.environment.sunIntensity },
            { "exposure", scene.environment.exposure },
        };
        root["camera"] = WriteCamera(scene.camera);
        root["renderSettings"] = WriteRenderSettings(scene.renderSettings);

        json bookmarks = json::array();

        for (size_t i = 0; i < Scene::BookmarkCount; ++i)
        {
            if (!scene.cameraBookmarks[i])
                continue;

            json bookmark = WriteCamera(*scene.cameraBookmarks[i]);
            bookmark["slot"] = i + 1;
            bookmarks.push_back(std::move(bookmark));
        }

        if (!bookmarks.empty())
            root["cameraBookmarks"] = std::move(bookmarks);

        json entities = json::array();

        for (const Entity& entity : scene.GetEntities())
        {
            json node = {
                { "name", entity.name },
                { "parent", entity.parent == InvalidEntity ? -1 : static_cast<int>(entity.parent) },
                { "position", ToJson(entity.transform.position) },
                { "rotationDegrees", ToJson(entity.transform.rotationDegrees) },
                { "scale", ToJson(entity.transform.scale) },
            };

            if (!entity.active)
                node["active"] = false;

            if (entity.light)
                node["light"] = WriteLight(*entity.light);

            if (entity.model)
            {
                // relative() is empty across drives; fall back to the absolute path then.
                std::filesystem::path modelPath = std::filesystem::relative(entity.model->path, baseDirectory);
                node["model"] = ToGenericUtf8(modelPath.empty() ? entity.model->path : modelPath);
            }

            entities.push_back(std::move(node));
        }

        root["entities"] = std::move(entities);

        std::ofstream stream(path);

        if (!stream)
            throw std::runtime_error("Failed to write scene " + ToUtf8(path.wstring()));

        stream << root.dump(2) << '\n';
    }
}
