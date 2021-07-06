#include <agz-utils/image.h>
#include <agz-utils/thread.h>

#include "envir.h"

void EnvirLight::initialize(const std::string &filename, const Int2 &sampleRes)
{
    const auto data =
        agz::texture::texture2d_t(agz::img::load_rgb_from_hdr_file(filename));

    const int width = data.width(), height = data.height();
    const int newWidth  = (std::min)(width, sampleRes.x);
    const int newHeight = (std::min)(height, sampleRes.y);

    std::atomic<float> lum_sum = 0;
    agz::texture::texture2d_t<float> probs(newHeight, newWidth);
    
    agz::thread::parallel_forrange(0, newHeight, [&](int, int y)
    {
        const float y0 = float(y)     / float(newHeight);
        const float y1 = float(y + 1) / float(newHeight);

        const float y0_src = y0 * height;
        const float y1_src = y1 * height;

        const int y_src_beg = (std::max)(0, int(std::floor(y0_src) - 1));
        const int y_src_lst = (std::min)(height - 1, int(std::floor(y1_src) + 1));

        for(int x = 0; x < newWidth; ++x)
        {
            const float x0 = float(x)     / newWidth;
            const float x1 = float(x + 1) / newWidth;

            const float x0_src = x0 * width;
            const float x1_src = x1 * width;

            const int x_src_beg = (std::max)(0, int(std::floor(x0_src) - 1));
            const int x_src_lst = (std::min)(width - 1, int(std::floor(x1_src) + 1));

            float pixel_lum = 0;
            for(int y_src = y_src_beg; y_src <= y_src_lst; ++y_src)
            {
                for(int x_src = x_src_beg; x_src <= x_src_lst; ++x_src)
                {
                    const float src_u = (float(x_src) + 0.5f) / float(width);
                    const float src_v = (float(y_src) + 0.5f) / float(height);

                    const auto texel = agz::texture::linear_sample2d(
                        Float2(src_u, src_v),
                        [&](int xi, int yi) { return data(yi, xi); },
                        width, height);

                    pixel_lum += texel.lum();
                }
            }

            const float delta_area = std::abs(
                2 * PI * (x1 - x0) * (std::cos(PI * y1)
              - std::cos(PI * y0)));
              
            const float area_lum = pixel_lum * delta_area;
            probs(y, x) = area_lum;
            agz::math::atomic_add(lum_sum, area_lum);
        }
    });

    if(lum_sum > 0.001f)
    {
        const float ratio = 1 / lum_sum;
        for(int y = 0; y < newHeight; ++y)
        {
            for(int x = 0; x < newWidth; ++x)
                probs(y, x) *= ratio;
        }
    }

    using AliasTable = agz::math::distribution::alias_sampler_t<float>;

    AliasTable aliasTable;
    aliasTable.initialize(probs.raw_data(), probs.size().product());
    auto &aliasTableData = aliasTable.get_table();

    auto envirSRV = Texture2DLoader::loadFromMemory(
        DXGI_FORMAT_R32G32B32A32_FLOAT, width, height, 1, 3, &data.raw_data()->r);

    D3D11_BUFFER_DESC aliasTableBufDesc;
    aliasTableBufDesc.ByteWidth = static_cast<UINT>(
        sizeof(AliasTable::table_unit) * aliasTableData.size());
    aliasTableBufDesc.Usage               = D3D11_USAGE_IMMUTABLE;
    aliasTableBufDesc.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
    aliasTableBufDesc.CPUAccessFlags      = 0;
    aliasTableBufDesc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    aliasTableBufDesc.StructureByteStride = sizeof(AliasTable::table_unit);

    D3D11_SUBRESOURCE_DATA aliasTableBufSubrscData;
    aliasTableBufSubrscData.pSysMem          = aliasTableData.data();
    aliasTableBufSubrscData.SysMemPitch      = 0;
    aliasTableBufSubrscData.SysMemSlicePitch = 0;

    auto aliasTableBuf = device.createBuffer(
        aliasTableBufDesc, &aliasTableBufSubrscData);

    D3D11_SHADER_RESOURCE_VIEW_DESC aliasTableSRVDesc;
    aliasTableSRVDesc.Format              = DXGI_FORMAT_UNKNOWN;
    aliasTableSRVDesc.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
    aliasTableSRVDesc.Buffer.FirstElement = 0;
    aliasTableSRVDesc.Buffer.NumElements  = static_cast<UINT>(aliasTableData.size());

    auto aliasTableSRV = device.createSRV(aliasTableBuf, aliasTableSRVDesc);

    auto probSRV = Texture2DLoader::loadFromMemory(
        DXGI_FORMAT_R32_FLOAT, newWidth, newHeight, 1, 1, probs.raw_data());

    auto sampler = device.createSampler(
        D3D11_FILTER_MIN_MAG_MIP_LINEAR,
        D3D11_TEXTURE_ADDRESS_WRAP,
        D3D11_TEXTURE_ADDRESS_WRAP,
        D3D11_TEXTURE_ADDRESS_WRAP);

    envirLight_.initialize();

    envir_      = std::move(envirSRV);
    aliasTable_ = std::move(aliasTableSRV);
    aliasProbs_ = std::move(probSRV);
    sampler_    = std::move(sampler);

    envirLightData_.tableWidth  = newWidth;
    envirLightData_.tableHeight = newHeight;
    envirLightData_.tableSize   = static_cast<int>(aliasTableData.size());
}

void EnvirLight::updateConstantBuffer(float intensity)
{
    envirLightData_.intensity = intensity;
    envirLight_.update(envirLightData_);
}

void EnvirLight::bind(Shader<CS>::RscMgr &shaderRscs)
{
    shaderRscs.getShaderResourceViewSlot<CS>("EnvirLight")
        ->setShaderResourceView(envir_);
    shaderRscs.getShaderResourceViewSlot<CS>("EnvirAliasTable")
        ->setShaderResourceView(aliasTable_);
    shaderRscs.getShaderResourceViewSlot<CS>("EnvirAliasProbs")
        ->setShaderResourceView(aliasProbs_);
    shaderRscs.getSamplerSlot<CS>("EnvirSampler")
        ->setSampler(sampler_);

    shaderRscs.getConstantBufferSlot<CS>("EnvirLightParams")
        ->setBuffer(envirLight_);
}
