#pragma once

#include "common.h"

class Volume
{
public:

    void initialize();

    void loadDensity(const std::string &filename);

    void loadAlbedo(const std::string &filename);

    void setBoundingBox(const Float3 &lower, const Float3 &upper);

    void setDensityScale(float scale);

    void setG(float g);

    void updateConstantBuffer();

    void bind(Shader<CS>::RscMgr &shaderRscs);

private:

    struct VolumeParams
    {
        Float3 lower;        float maxDensity;
        Float3 upper;        float invDensity;
        Float3 invExtent;    float phaseG;
        float  densityScale; float phaseG2;
        float  pad0, pad1;
    };

    float rawMaxDensity_ = 0;

    ComPtr<ID3D11ShaderResourceView> densitySRV_;
    ComPtr<ID3D11ShaderResourceView> albedoSRV_;

    ComPtr<ID3D11SamplerState> sampler_;

    VolumeParams                 volParamsData_ = {};
    ConstantBuffer<VolumeParams> volParams_;
};
