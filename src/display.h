#pragma once

#include "common.h"

class Displayer
{
public:

    void initialize();

    void setExposure(float exposure);

    void render(ComPtr<ID3D11ShaderResourceView> tex);

private:

    struct PSParams
    {
        float exposure;
        float pad0;
        float pad1;
        float pad2;
    };

    Shader<VS, PS>         shader_;
    Shader<VS, PS>::RscMgr shaderRscs_;

    ShaderResourceViewSlot<PS> *texSlot_ = nullptr;

    PSParams                 psParamsData_ = {};
    ConstantBuffer<PSParams> psParams_;
};
