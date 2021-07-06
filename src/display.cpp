#include "display.h"

void Displayer::initialize()
{
    shader_.initializeStageFromFile<VS>("./asset/display.hlsl", nullptr, "VSMain");
    shader_.initializeStageFromFile<PS>("./asset/display.hlsl", nullptr, "PSMain");

    shaderRscs_ = shader_.createResourceManager();

    texSlot_ = shaderRscs_.getShaderResourceViewSlot<PS>("Tex");

    psParams_.initialize();
    shaderRscs_.getConstantBufferSlot<PS>("PSParams")
        ->setBuffer(psParams_);

    auto sampler = device.createSampler(
        D3D11_FILTER_MIN_MAG_MIP_POINT,
        D3D11_TEXTURE_ADDRESS_CLAMP,
        D3D11_TEXTURE_ADDRESS_CLAMP,
        D3D11_TEXTURE_ADDRESS_CLAMP);
    shaderRscs_.getSamplerSlot<PS>("TexSampler")->setSampler(std::move(sampler));
}

void Displayer::setExposure(float exposure)
{
    psParamsData_.exposure = exposure;
}

void Displayer::render(ComPtr<ID3D11ShaderResourceView> tex)
{
    texSlot_->setShaderResourceView(std::move(tex));
    psParams_.update(psParamsData_);

    shader_.bind();
    shaderRscs_.bind();

    deviceContext.setPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext.draw(3, 0);

    shaderRscs_.unbind();
    shader_.unbind();
}
