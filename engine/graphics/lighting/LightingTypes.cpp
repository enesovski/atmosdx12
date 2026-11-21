#include "graphics/lighting/LightingTypes.h"

namespace atmos
{
    void DeclareLightingReads(RGBuilder& builder, const LightingResources& resources)
    {
        if (resources.cascadeShadowMap.IsValid())
            builder.ReadTexture(resources.cascadeShadowMap);

        if (resources.shadowAtlas.IsValid())
            builder.ReadTexture(resources.shadowAtlas);

        builder.ReadBuffer(resources.clusterLightCounts);
        builder.ReadBuffer(resources.clusterLightIndices);
        builder.ReadBuffer(resources.skyLight.irradianceSH);
        builder.ReadTexture(resources.skyLight.specularCube);
        builder.ReadTexture(resources.skyLight.brdfLut);
    }

    LightingConstants ResolveLightingConstants(const RGContext& context, const LightingResources& resources,
                                               LightingConstants constants)
    {
        constants.clusterLightCountsIndex = context.GetBuffer(resources.clusterLightCounts).GetSrvIndex();
        constants.clusterLightIndicesIndex = context.GetBuffer(resources.clusterLightIndices).GetSrvIndex();
        constants.irradianceSHIndex = context.GetBuffer(resources.skyLight.irradianceSH).GetSrvIndex();
        constants.specularCubeIndex = context.GetTexture(resources.skyLight.specularCube).GetSrvIndex();
        constants.brdfLutIndex = context.GetTexture(resources.skyLight.brdfLut).GetSrvIndex();
        constants.specularMipCount = static_cast<float>(resources.skyLight.specularMipCount);

        if (resources.cascadeShadowMap.IsValid())
            constants.cascadeShadowMapIndex = context.GetTexture(resources.cascadeShadowMap).GetSrvIndex();
        else
            constants.sunShadowsEnabled = 0;

        if (resources.shadowAtlas.IsValid())
            constants.shadowAtlasIndex = context.GetTexture(resources.shadowAtlas).GetSrvIndex();
        else
            constants.localShadowsEnabled = 0;

        return constants;
    }
}
