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
        constexpr int FormatVersion = 1;

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

        std::string ToGenericUtf8(const std::filesystem::path& path)
        {
            return ToUtf8(path.generic_wstring());
        }
    }

    Scene LoadScene(const std::filesystem::path& path)
    {
        std::ifstream stream(path);

        if (!stream)
            throw std::runtime_error("Failed to open scene " + ToUtf8(path.wstring()));

        const json root = json::parse(stream);

        if (root.value("version", 0) != FormatVersion)
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
        {
            const json& camera = root.at("camera");
            SceneCamera& target = scene.camera;
            target.position = ReadFloat3(camera, "position", target.position);
            target.yawDegrees = camera.value("yawDegrees", target.yawDegrees);
            target.pitchDegrees = camera.value("pitchDegrees", target.pitchDegrees);
            target.fovDegrees = camera.value("fovDegrees", target.fovDegrees);
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

            if (node.contains("model"))
            {
                const std::filesystem::path relative = ToWide(node.at("model").get<std::string>());
                entity.model = ModelComponent{ .path = (baseDirectory / relative).lexically_normal() };
            }
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
        root["camera"] = {
            { "position", ToJson(scene.camera.position) },
            { "yawDegrees", scene.camera.yawDegrees },
            { "pitchDegrees", scene.camera.pitchDegrees },
            { "fovDegrees", scene.camera.fovDegrees },
        };

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
