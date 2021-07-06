#include <fstream>

#include "volume.h"

void Volume::initialize()
{
    volParams_.initialize();

    sampler_ = device.createSampler(
        D3D11_FILTER_MIN_MAG_MIP_LINEAR,
        D3D11_TEXTURE_ADDRESS_CLAMP,
        D3D11_TEXTURE_ADDRESS_CLAMP,
        D3D11_TEXTURE_ADDRESS_CLAMP);
}

void Volume::loadDensity(const std::string &filename)
{
    std::ifstream fin(filename);
    if(!fin)
        throw std::runtime_error("failed to open file: " + filename);
    
    int width = 1, height = 1, depth = 1;
    fin >> width >> height >> depth;
    
    const int voxelCount = width * height * depth;
    std::vector<float> data(voxelCount);

    rawMaxDensity_ = 0;
    for(int i = 0; i < voxelCount; ++i)
    {
        fin >> data[i];
        rawMaxDensity_ = (std::max)(rawMaxDensity_, data[i]);
    }

    D3D11_TEXTURE3D_DESC texDesc;
    texDesc.Width          = width;
    texDesc.Height         = height;
    texDesc.Depth          = depth;
    texDesc.MipLevels      = 1;
    texDesc.Format         = DXGI_FORMAT_R32_FLOAT;
    texDesc.Usage          = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags      = 0;
    
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format                    = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MipLevels       = 1;
    srvDesc.Texture3D.MostDetailedMip = 0;
    
    D3D11_SUBRESOURCE_DATA texData;
    texData.pSysMem          = data.data();
    texData.SysMemPitch      = width * sizeof(float);
    texData.SysMemSlicePitch = height * texData.SysMemPitch;
    
    auto tex = device.createTex3D(texDesc, &texData);
    densitySRV_ = device.createSRV(tex, srvDesc);
}

void Volume::loadAlbedo(const std::string &filename)
{
    std::ifstream fin(filename);
    if(!fin)
        throw std::runtime_error("failed to open file: " + filename);
    
    int width = 1, height = 1, depth = 1;
    fin >> width >> height >> depth;
    
    const int voxelCount = width * height * depth;
    std::vector<Float4> data(voxelCount);
    
    for(int i = 0; i < voxelCount; ++i)
        fin >> data[i].x >> data[i].y >> data[i].z;
    
    D3D11_TEXTURE3D_DESC texDesc;
    texDesc.Width          = width;
    texDesc.Height         = height;
    texDesc.Depth          = depth;
    texDesc.MipLevels      = 1;
    texDesc.Format         = DXGI_FORMAT_R32G32B32A32_FLOAT;
    texDesc.Usage          = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags      = 0;
    
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format                    = DXGI_FORMAT_R32G32B32A32_FLOAT;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MipLevels       = 1;
    srvDesc.Texture3D.MostDetailedMip = 0;
    
    D3D11_SUBRESOURCE_DATA texData;
    texData.pSysMem          = data.data();
    texData.SysMemPitch      = width * sizeof(Float4);
    texData.SysMemSlicePitch = height * texData.SysMemPitch;
    
    auto tex = device.createTex3D(texDesc, &texData);
    albedoSRV_ = device.createSRV(tex, srvDesc);
}

void Volume::setBoundingBox(const Float3 &lower, const Float3 &upper)
{
    volParamsData_.lower     = lower;
    volParamsData_.upper     = upper;
    volParamsData_.invExtent = Float3(1) / (upper - lower);
}

void Volume::setDensityScale(float scale)
{
    volParamsData_.densityScale = scale;
}

void Volume::setG(float g)
{
    volParamsData_.phaseG  = g;
    volParamsData_.phaseG2 = g * g;
}

void Volume::updateConstantBuffer()
{
    volParamsData_.maxDensity = rawMaxDensity_ * volParamsData_.densityScale;
    volParamsData_.invDensity = 1 / (std::max)(0.001f, volParamsData_.maxDensity);

    volParams_.update(volParamsData_);
}

void Volume::bind(Shader<CS>::RscMgr &shaderRscs)
{
    shaderRscs.getShaderResourceViewSlot<CS>("Albedo")
        ->setShaderResourceView(albedoSRV_);
    shaderRscs.getShaderResourceViewSlot<CS>("Density")
        ->setShaderResourceView(densitySRV_);
    shaderRscs.getConstantBufferSlot<CS>("VolumeParams")
        ->setBuffer(volParams_);
    shaderRscs.getSamplerSlot<CS>("VolumeSampler")
        ->setSampler(sampler_);
}
