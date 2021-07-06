#pragma once

#include "common.h"

class EnvirLight
{
public:

    void initialize(const std::string &filename, const Int2 &sampleRes);
    
    void updateConstantBuffer(float intensity);
    
    void bind(Shader<CS>::RscMgr &shaderRscs);

private:

    struct EnvirLightParams
    {
        float intensity;
        int   tableWidth;
        int   tableHeight;
        int   tableSize;
    };

    ComPtr<ID3D11ShaderResourceView> envir_;
    ComPtr<ID3D11ShaderResourceView> aliasTable_;
    ComPtr<ID3D11ShaderResourceView> aliasProbs_;
    ComPtr<ID3D11SamplerState>       sampler_;

    EnvirLightParams                 envirLightData_ = {};
    ConstantBuffer<EnvirLightParams> envirLight_;
};
