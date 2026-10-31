#pragma once

#include <cstdint>

namespace Fish {
    // 不用 vk::Extent2D:这个头会给应用层看,不把 vulkan 带出去。
    struct ImageExtent
    {
        uint32_t width  = 0;
        uint32_t height = 0;
    };

    enum class TextureFormat
    {
        RGBA8_SRGB,
        RGBA8_UNORM,
        R8_UNORM
    };

    enum class ShadowFormat
    {
        D24_UNORM_S8,
        D32_SFLOAT
    };

    // 采样用的色彩图。usage / tiling / 采样数 / 内存属性由 Image 按类型写死。
    //
    // TODO: mipLevels > 1 现在没有填充手段 —— 带 mip 的导入和运行期生成都没做,
    // 上传只写第 0 层(见 Image::copyFrom)。
    struct TextureDesc
    {
        ImageExtent   extent;
        TextureFormat format;          // 不给默认值:默认成 sRGB 会让法线图悄悄按 sRGB 解码
        uint32_t      mipLevels = 1;
    };

    // 阴影用的深度图。mip 固定 1 层,只有层数可配。
    struct ShadowMapDesc
    {
        ImageExtent  extent;
        ShadowFormat format;
        uint32_t     layerCount = 1;
    };
}
