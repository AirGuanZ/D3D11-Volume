#include "raw.h"

namespace
{
    std::pair<ComPtr<ID3D11ShaderResourceView>,
              ComPtr<ID3D11UnorderedAccessView>>
        createOutput(const Int2 &size)
    {
        std::vector<float> initData(4 * size.product());

        D3D11_TEXTURE2D_DESC texDesc;
        texDesc.Width          = static_cast<UINT>(size.x);
        texDesc.Height         = static_cast<UINT>(size.y);
        texDesc.MipLevels      = 1;
        texDesc.ArraySize      = 1;
        texDesc.Format         = DXGI_FORMAT_R32G32B32A32_FLOAT;
        texDesc.SampleDesc     = { 1, 0 };
        texDesc.Usage          = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags      = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags      = 0;

        D3D11_SUBRESOURCE_DATA texData;
        texData.pSysMem          = initData.data();
        texData.SysMemPitch      = 4 * sizeof(float) * size.x;
        texData.SysMemSlicePitch = size.y * texData.SysMemPitch;

        auto tex = device.createTex2D(texDesc, &texData);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format                    = DXGI_FORMAT_R32G32B32A32_FLOAT;
        srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels       = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        auto srv = device.createSRV(tex, srvDesc);

        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
        uavDesc.Format             = DXGI_FORMAT_R32G32B32A32_FLOAT;
        uavDesc.ViewDimension      = D3D11_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = 0;

        auto uav = device.createUAV(tex, uavDesc);

        return { srv, uav };
    }
}

void RawVolumeRenderer::initilalize(const Int2 &size)
{
    shader_.initializeStageFromFile<CS>("./asset/raw.hlsl", nullptr, "CSMain");

    shaderRscs_ = shader_.createResourceManager();

    densitySlot_ = shaderRscs_.getShaderResourceViewSlot<CS>("Density");
    albedoSlot_  = shaderRscs_.getShaderResourceViewSlot<CS>("Albedo");

    oldRandomSeedsSlot_ = shaderRscs_.getShaderResourceViewSlot<CS>("OldRandomSeeds");
    newRandomSeedsSlot_ = shaderRscs_.getUnorderedAccessViewSlot<CS>("NewRandomSeeds");

    historySlot_ = shaderRscs_.getShaderResourceViewSlot<CS>("History");
    outputSlot_  = shaderRscs_.getUnorderedAccessViewSlot<CS>("Output");

    resize(size);

    csParams_.initialize();
    shaderRscs_.getConstantBufferSlot<CS>("CSParams")
        ->setBuffer(csParams_);
}

void RawVolumeRenderer::resize(const Int2 &size)
{
    auto output1 = createOutput(size);
    auto output2 = createOutput(size);

    generateRandomSeeds(size);

    outputSRV1_ = std::move(output1.first);
    outputUAV1_ = std::move(output1.second);

    outputSRV2_ = std::move(output2.first);
    outputUAV2_ = std::move(output2.second);

    csParamsData_.outputWidth  = size.x;
    csParamsData_.outputHeight = size.y;
}

void RawVolumeRenderer::setEnvir(EnvirLight &envir)
{
    envir.bind(shaderRscs_);
}

void RawVolumeRenderer::setVolume(Volume &volume)
{
    volume.bind(shaderRscs_);
}

void RawVolumeRenderer::setTracer(int maxDepth)
{
    csParamsData_.maxTraceDepth = maxDepth;
}

void RawVolumeRenderer::setCamera(const Camera &camera)
{
    const auto fD = camera.getFrustumDirections();

    csParamsData_.discardHistory |=
        csParamsData_.eye != camera.getPosition() ||
        csParamsData_.frustumA != fD.frustumA ||
        csParamsData_.frustumB != fD.frustumB ||
        csParamsData_.frustumC != fD.frustumC ||
        csParamsData_.frustumD != fD.frustumD;

    csParamsData_.eye = camera.getPosition();
    csParamsData_.frustumA = fD.frustumA;
    csParamsData_.frustumB = fD.frustumB;
    csParamsData_.frustumC = fD.frustumC;
    csParamsData_.frustumD = fD.frustumD;
}

void RawVolumeRenderer::discardHistory()
{
    csParamsData_.discardHistory = true;
}

ComPtr<ID3D11ShaderResourceView> RawVolumeRenderer::getOutput() const
{
    return outputSRV1_;
}

void RawVolumeRenderer::render()
{
    oldRandomSeedsSlot_->setShaderResourceView(randomSeedsSRV1_);
    newRandomSeedsSlot_->setUnorderedAccessView(randomSeedsUAV2_);

    std::swap(randomSeedsSRV1_, randomSeedsSRV2_);
    std::swap(randomSeedsUAV1_, randomSeedsUAV2_);

    historySlot_->setShaderResourceView(outputSRV1_);
    outputSlot_->setUnorderedAccessView(outputUAV2_);

    std::swap(outputSRV1_, outputSRV2_);
    std::swap(outputUAV1_, outputUAV2_);

    csParams_.update(csParamsData_);
    csParamsData_.discardHistory = false;

    shader_.bind();
    shaderRscs_.bind();

    const int GROUP_SIZE_X = 16, GROUP_SIZE_Y = 16;
    const int groupCountX =
        (csParamsData_.outputWidth + GROUP_SIZE_X - 1) / GROUP_SIZE_X;
    const int groupCountY =
        (csParamsData_.outputHeight + GROUP_SIZE_Y - 1) / GROUP_SIZE_Y;

    deviceContext.dispatch(groupCountX, groupCountY, 1);

    shaderRscs_.unbind();
    shader_.unbind();
}

void RawVolumeRenderer::generateRandomSeeds(const Int2 &size)
{
    const int count = size.product();

    std::vector<uint32_t> data(count);
    for(int i = 0; i < count; ++i)
        data[i] = static_cast<uint32_t>(i + 1);

    D3D11_TEXTURE2D_DESC texDesc;
    texDesc.Width          = static_cast<UINT>(size.x);
    texDesc.Height         = static_cast<UINT>(size.y);
    texDesc.MipLevels      = 1;
    texDesc.ArraySize      = 1;
    texDesc.Format         = DXGI_FORMAT_R32_UINT;
    texDesc.SampleDesc     = { 1, 0 };
    texDesc.Usage          = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags      = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags      = 0;

    D3D11_SUBRESOURCE_DATA texData;
    texData.pSysMem          = data.data();
    texData.SysMemPitch      = sizeof(uint32_t) * size.x;
    texData.SysMemSlicePitch = texData.SysMemPitch * size.y;

    auto tex1 = device.createTex2D(texDesc, &texData);
    auto tex2 = device.createTex2D(texDesc, nullptr);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format                    = DXGI_FORMAT_R32_UINT;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels       = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    auto srv1 = device.createSRV(tex1, srvDesc);
    auto srv2 = device.createSRV(tex2, srvDesc);

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
    uavDesc.Format             = DXGI_FORMAT_R32_UINT;
    uavDesc.ViewDimension      = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;

    auto uav1 = device.createUAV(tex1, uavDesc);
    auto uav2 = device.createUAV(tex2, uavDesc);

    randomSeedsSRV1_ = std::move(srv1);
    randomSeedsSRV2_ = std::move(srv2);

    randomSeedsUAV1_ = std::move(uav1);
    randomSeedsUAV2_ = std::move(uav2);
}
