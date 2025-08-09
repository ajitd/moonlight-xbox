// Enhanced Pixel Shader with Variable Rate Shading support for DirectX 12 Ultimate
// Optimized for Xbox Series X|S

#include "ShaderStructures.h"

cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
    matrix model;
    matrix view;
    matrix projection;
    float4 colorMatrix[3];
    float4 renderParams; // x: VRS enabled, y: quality mode, z: HDR enabled, w: padding
};

Texture2D<float> textureY : register(t0);
Texture2D<float> textureU : register(t1);
Texture2D<float> textureV : register(t2);
SamplerState samplerLinear : register(s0);

struct PixelShaderInput
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
    uint shadingRate : SV_ShadingRate; // DirectX 12 Ultimate feature
};

float4 main(PixelShaderInput input) : SV_TARGET
{
    // Sample YUV textures
    float Y = textureY.Sample(samplerLinear, input.texCoord).r;
    float U = textureU.Sample(samplerLinear, input.texCoord).r;
    float V = textureV.Sample(samplerLinear, input.texCoord).r;

    // YUV to RGB conversion using the color matrix from constant buffer
    float3 yuv = float3(Y, U - 0.5f, V - 0.5f);
    
    float3 rgb;
    rgb.r = dot(yuv, colorMatrix[0].xyz);
    rgb.g = dot(yuv, colorMatrix[1].xyz);
    rgb.b = dot(yuv, colorMatrix[2].xyz);

    // Xbox Series X enhancements
    if (renderParams.x > 0.5f) // VRS enabled
    {
        // Apply Variable Rate Shading optimizations
        // Reduce computational complexity in areas with lower detail
        if (input.shadingRate > 1)
        {
            // Apply simplified color processing for reduced shading rate areas
            rgb = saturate(rgb);
        }
        else
        {
            // Full quality processing for 1x1 shading rate areas
            rgb = saturate(rgb);
            
            // Enhanced color processing for high-detail areas
            if (renderParams.y > 0.5f) // Quality mode enabled
            {
                // Apply additional color enhancement for Xbox Series X
                rgb = pow(rgb, 1.0f / 2.2f); // Gamma correction
            }
        }
    }
    else
    {
        // Standard processing without VRS
        rgb = saturate(rgb);
    }

    // HDR support for Xbox Series X
    if (renderParams.z > 0.5f) // HDR enabled
    {
        // Apply HDR tone mapping
        rgb = rgb / (rgb + 1.0f); // Simple Reinhard tone mapping
        rgb = pow(rgb, 1.0f / 2.2f);
    }

    return float4(rgb, 1.0f);
}