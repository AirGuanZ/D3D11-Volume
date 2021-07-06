#pragma once

#include "camera.h"
#include "envir.h"
#include "volume.h"

class RawVolumeRenderer
{
public:

    void initilalize(const Int2 &size);

    void resize(const Int2 &size);

    void setTracer(int maxDepth);

    void setCamera(const Camera &camera);

    void setEnvir(EnvirLight &envir);

    void setVolume(Volume &volume);

    void discardHistory();

    ComPtr<ID3D11ShaderResourceView> getOutput() const;

    void render();

private:

    struct CSParams
    {
        Float3 eye;      int   maxTraceDepth;
        Float3 frustumA; int   outputWidth;
        Float3 frustumB; int   outputHeight;
        Float3 frustumC; int   discardHistory;
        Float3 frustumD; float pad0;
    };

    void generateRandomSeeds(const Int2 &size);

    ComPtr<ID3D11ShaderResourceView>  outputSRV1_;
    ComPtr<ID3D11UnorderedAccessView> outputUAV1_;

    ComPtr<ID3D11ShaderResourceView>  outputSRV2_;
    ComPtr<ID3D11UnorderedAccessView> outputUAV2_;

    Shader<CS>         shader_;
    Shader<CS>::RscMgr shaderRscs_;
    
    ShaderResourceViewSlot<CS> *densitySlot_ = nullptr;
    ShaderResourceViewSlot<CS> *albedoSlot_  = nullptr;

    ShaderResourceViewSlot<CS>  *oldRandomSeedsSlot_ = nullptr;
    UnorderedAccessViewSlot<CS> *newRandomSeedsSlot_ = nullptr;

    ShaderResourceViewSlot<CS>  *historySlot_ = nullptr;
    UnorderedAccessViewSlot<CS> *outputSlot_  = nullptr;

    ComPtr<ID3D11ShaderResourceView> randomSeedsSRV1_;
    ComPtr<ID3D11ShaderResourceView> randomSeedsSRV2_;

    ComPtr<ID3D11UnorderedAccessView> randomSeedsUAV1_;
    ComPtr<ID3D11UnorderedAccessView> randomSeedsUAV2_;

    CSParams                 csParamsData_ = {};
    ConstantBuffer<CSParams> csParams_;
};
