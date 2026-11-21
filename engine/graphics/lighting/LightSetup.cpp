#include "graphics/lighting/LightSetup.h"
#include "graphics/RenderScene.h"
#include "graphics/RenderView.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace atmos
{
    namespace
    {
        constexpr float CascadeCasterDistance = 100.0f;  // meters behind each cascade toward the sun
        constexpr float LocalShadowNearPlane = 0.05f;

        float SmoothStep(float edge0, float edge1, float x)
        {
            const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        XMFLOAT3 Lerp(const XMFLOAT3& a, const XMFLOAT3& b, float t)
        {
            return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
        }

        // Same tint as the sky's sun disk, so lighting and sky agree.
        XMFLOAT3 GetSunColor(float sunHeight)
        {
            return Lerp({ 1.0f, 0.55f, 0.30f }, { 1.0f, 0.96f, 0.80f }, SmoothStep(0.03f, 0.65f, sunHeight));
        }

        ViewData MakeViewData(FXMMATRIX viewProjection, const XMFLOAT3& position, float texelWorldSize,
                              const XMFLOAT4& atlasScaleOffset)
        {
            ViewData view{};
            XMStoreFloat4x4(&view.viewProjection, viewProjection);
            view.atlasScaleOffset = atlasScaleOffset;
            view.position = position;
            view.texelWorldSize = texelWorldSize;
            return view;
        }

        // Practical split scheme (Zhang 2006): blend of logarithmic and uniform splits.
        float GetCascadeSplit(uint32_t index, uint32_t count, float nearDepth, float farDepth, float lambda)
        {
            const float t = static_cast<float>(index) / count;
            const float logarithmic = nearDepth * std::pow(farDepth / nearDepth, t);
            const float uniform = nearDepth + (farDepth - nearDepth) * t;
            return lambda * logarithmic + (1.0f - lambda) * uniform;
        }

        XMVECTOR GetStableUpVector(FXMVECTOR forward)
        {
            return std::abs(XMVectorGetY(forward)) > 0.99f ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
                                                            : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        }
    }

    void LightSetup::Prepare(const RenderScene& scene, const RenderView& view)
    {
        lights_.clear();
        views_.clear();
        shadowViews_.clear();
        cascadeCount_ = 0;
        localShadowCount_ = 0;

        const XMMATRIX cameraViewProjection = XMLoadFloat4x4(&view.viewMatrix) * XMLoadFloat4x4(&view.projectionMatrix);
        views_.push_back(MakeViewData(cameraViewProjection, view.position, 0.0f, { 1.0f, 1.0f, 0.0f, 0.0f }));

        SetupSun(scene, view);
        SetupLocalLights(scene, view);
    }

    // Stable cascades: each cascade is a bounding sphere of its frustum slice, so its size does not
    // change as the camera turns, and its center snaps to whole shadow texels, so edges do not shimmer.
    void LightSetup::SetupSun(const RenderScene& scene, const RenderView& view)
    {
        const RenderSettings& settings = scene.settings;
        const XMFLOAT3 sunDirection = scene.sky.GetSunDirection();
        cascadeResolution_ = std::clamp(settings.cascadeResolution, MinCascadeResolution, MaxCascadeResolution);
        const float visibility = SmoothStep(-0.08f, 0.05f, sunDirection.y);

        sunDirection_ = sunDirection;
        sunColor_ = GetSunColor(sunDirection.y);
        sunIntensity_ = scene.sky.sunIntensity * visibility;

        if (!settings.sunShadows || visibility <= 0.0f)
            return;

        const XMMATRIX inverseView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&view.viewMatrix));
        const float tanHalfX = 1.0f / view.projectionMatrix._11;
        const float tanHalfY = 1.0f / view.projectionMatrix._22;
        const float nearDepth = view.projectionMatrix._43;
        const float farDepth = settings.shadowDistance;

        const XMVECTOR lightForward = -XMVector3Normalize(XMLoadFloat3(&sunDirection));
        const XMMATRIX lightRotation = XMMatrixLookToLH(XMVectorZero(), lightForward, GetStableUpVector(lightForward));

        float splitStart = nearDepth;

        for (uint32_t cascade = 0; cascade < CascadeCount; ++cascade)
        {
            const float splitEnd = GetCascadeSplit(cascade + 1, CascadeCount, nearDepth, farDepth, settings.cascadeSplitLambda);
            (&cascadeSplits_.x)[cascade] = splitEnd;

            // Bounding sphere of the slice's eight corners.
            XMVECTOR corners[8];
            XMVECTOR center = XMVectorZero();

            for (int i = 0; i < 8; ++i)
            {
                const float depth = i < 4 ? splitStart : splitEnd;
                const float x = (i & 1 ? 1.0f : -1.0f) * tanHalfX * depth;
                const float y = (i & 2 ? 1.0f : -1.0f) * tanHalfY * depth;
                corners[i] = XMVector3TransformCoord(XMVectorSet(x, y, depth, 1.0f), inverseView);
                center += corners[i];
            }

            center /= 8.0f;
            float radius = 0.0f;

            for (const XMVECTOR& corner : corners)
                radius = std::max(radius, XMVectorGetX(XMVector3Length(corner - center)));

            radius = std::ceil(radius * 16.0f) / 16.0f;

            // Snap the light-space center to the texel grid.
            const float texelSize = 2.0f * radius / cascadeResolution_;
            XMFLOAT3 centerLight;
            XMStoreFloat3(&centerLight, XMVector3TransformCoord(center, lightRotation));
            centerLight.x = std::floor(centerLight.x / texelSize) * texelSize;
            centerLight.y = std::floor(centerLight.y / texelSize) * texelSize;

            // Reverse-Z ortho: the far bound maps to 0 and the near bound (toward the sun) to 1.
            const float nearLight = centerLight.z - radius - CascadeCasterDistance;
            const float farLight = centerLight.z + radius;
            const XMMATRIX projection = XMMatrixOrthographicOffCenterLH(centerLight.x - radius, centerLight.x + radius,
                                                                        centerLight.y - radius, centerLight.y + radius,
                                                                        farLight, nearLight);
            const XMMATRIX viewProjection = lightRotation * projection;

            XMFLOAT3 position;
            XMStoreFloat3(&position, center);

            shadowViews_.push_back({ static_cast<uint32_t>(views_.size()), Frustum::FromViewProjection(viewProjection), true,
                                     cascade, 0, 0 });
            views_.push_back(MakeViewData(viewProjection, position, texelSize, { 1.0f, 1.0f, 0.0f, 0.0f }));
            splitStart = splitEnd;
        }

        cascadeCount_ = CascadeCount;
    }

    void LightSetup::SetupLocalLights(const RenderScene& scene, const RenderView& view)
    {
        const size_t lightCount = std::min<size_t>(scene.lights.size(), MaxLights);

        for (size_t i = 0; i < lightCount; ++i)
        {
            const RenderLight& light = scene.lights[i];
            const float cosOuter = std::cos(XMConvertToRadians(light.outerConeDegrees));
            const float cosInner = std::cos(XMConvertToRadians(std::min(light.innerConeDegrees, light.outerConeDegrees)));
            const float spotScale = 1.0f / std::max(cosInner - cosOuter, 1e-4f);

            LightGpuData& gpu = lights_.emplace_back();
            gpu.position = light.position;
            gpu.range = std::max(light.range, 0.01f);
            gpu.color = { light.color.x * light.intensity, light.color.y * light.intensity, light.color.z * light.intensity };
            gpu.type = static_cast<uint32_t>(light.type);
            gpu.direction = light.direction;
            gpu.spotScale = light.type == RenderLightType::Spot ? spotScale : 0.0f;
            gpu.spotOffset = light.type == RenderLightType::Spot ? -cosOuter * spotScale : 1.0f;
            gpu.shadowViewIndex = NoShadowView;
        }

        if (!scene.settings.localShadows)
            return;

        // Nearest shadow casters get atlas tiles first.
        std::vector<uint32_t> casters;

        for (uint32_t i = 0; i < lights_.size(); ++i)
        {
            if (scene.lights[i].castsShadows)
                casters.push_back(i);
        }

        const XMVECTOR eye = XMLoadFloat3(&view.position);
        std::ranges::sort(casters, [&](uint32_t a, uint32_t b) {
            return XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&lights_[a].position) - eye)) <
                   XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&lights_[b].position) - eye));
        });

        constexpr uint32_t tilesPerRow = AtlasResolution / AtlasTileSize;
        constexpr uint32_t tileCount = tilesPerRow * tilesPerRow;
        constexpr float tileScale = static_cast<float>(AtlasTileSize) / AtlasResolution;
        uint32_t nextTile = 0;

        if (casters.size() > scene.settings.maxShadowedLights)
            casters.resize(scene.settings.maxShadowedLights);

        for (uint32_t lightIndex : casters)
        {
            LightGpuData& light = lights_[lightIndex];
            const bool isPoint = light.type == static_cast<uint32_t>(RenderLightType::Point);
            const uint32_t tilesNeeded = isPoint ? 6 : 1;

            if (nextTile + tilesNeeded > tileCount)
                break;

            // Point lights: six 90-degree faces in the order the shader expects (+X, -X, +Y, -Y, +Z, -Z).
            const XMVECTOR faceForwards[6] = { XMVectorSet(1, 0, 0, 0), XMVectorSet(-1, 0, 0, 0), XMVectorSet(0, 1, 0, 0),
                                               XMVectorSet(0, -1, 0, 0), XMVectorSet(0, 0, 1, 0), XMVectorSet(0, 0, -1, 0) };
            const float fov = isPoint ? XM_PIDIV2 : 2.0f * XMConvertToRadians(scene.lights[lightIndex].outerConeDegrees) + 0.05f;
            const float texelAngle = 2.0f * std::tan(fov * 0.5f) / AtlasTileSize;
            const XMMATRIX projection = XMMatrixPerspectiveFovLH(fov, 1.0f, light.range, LocalShadowNearPlane);
            const XMVECTOR position = XMLoadFloat3(&light.position);

            light.shadowViewIndex = static_cast<uint32_t>(views_.size());

            for (uint32_t face = 0; face < tilesNeeded; ++face)
            {
                const XMVECTOR forward = isPoint ? faceForwards[face] : XMVector3Normalize(XMLoadFloat3(&light.direction));
                const XMMATRIX viewProjection = XMMatrixLookToLH(position, forward, GetStableUpVector(forward)) * projection;

                const uint32_t tile = nextTile++;
                const uint32_t tileX = (tile % tilesPerRow) * AtlasTileSize;
                const uint32_t tileY = (tile / tilesPerRow) * AtlasTileSize;
                const XMFLOAT4 scaleOffset{ tileScale, tileScale, static_cast<float>(tileX) / AtlasResolution,
                                            static_cast<float>(tileY) / AtlasResolution };

                shadowViews_.push_back({ static_cast<uint32_t>(views_.size()), Frustum::FromViewProjection(viewProjection),
                                         false, 0, tileX, tileY });
                views_.push_back(MakeViewData(viewProjection, light.position, texelAngle, scaleOffset));
            }

            ++localShadowCount_;
        }
    }

    LightingConstants LightSetup::BuildConstants(const RenderScene& scene, uint32_t width, uint32_t height) const
    {
        const RenderSettings& settings = scene.settings;
        const float depthRange = std::log(LightGrid::FarDepth / LightGrid::NearDepth);

        LightingConstants constants{};
        constants.sunDirection = sunDirection_;
        constants.sunIntensity = sunIntensity_;
        constants.sunColor = sunColor_;
        constants.sunShadowsEnabled = cascadeCount_ > 0;

        constants.lightCount = static_cast<uint32_t>(lights_.size());
        constants.clusterGridSize[0] = LightGrid::TilesX;
        constants.clusterGridSize[1] = LightGrid::TilesY;
        constants.clusterGridSize[2] = LightGrid::Slices;
        constants.maxLightsPerCluster = LightGrid::MaxLightsPerCluster;
        constants.clusterTileSize = { std::ceil(static_cast<float>(width) / LightGrid::TilesX),
                                      std::ceil(static_cast<float>(height) / LightGrid::TilesY) };
        const float slices = static_cast<float>(LightGrid::Slices);
        constants.clusterDepthScale = slices / depthRange;
        constants.clusterDepthBias = -slices * std::log(LightGrid::NearDepth) / depthRange;

        constants.cascadeCount = std::max(cascadeCount_, 1u);
        constants.cascadeSplits = cascadeSplits_;
        constants.cascadeTexelSize = { 1.0f / cascadeResolution_, 1.0f / cascadeResolution_ };
        constants.atlasTexelSize = { 1.0f / AtlasResolution, 1.0f / AtlasResolution };

        constants.shadowNormalOffset = settings.shadowNormalOffset;
        constants.localShadowsEnabled = localShadowCount_ > 0;
        constants.imageBasedLightingEnabled = settings.imageBasedLighting;
        constants.ambientIntensity = settings.ambientIntensity;
        constants.reflectionIntensity = settings.reflectionIntensity;
        constants.shadowFilter = static_cast<uint32_t>(settings.shadowFilter);
        constants.debugView = static_cast<uint32_t>(settings.debugView);
        return constants;
    }
}
