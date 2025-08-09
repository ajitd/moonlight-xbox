// Enhanced Vertex Shader for DirectX 12 Ultimate
// Optimized for Xbox Series X|S with Variable Rate Shading support

#include "ShaderStructures.h"

cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
    matrix model;
    matrix view;
    matrix projection;
    float4 colorMatrix[3];
    float4 renderParams; // x: VRS enabled, y: quality mode, z: HDR enabled, w: padding
};

struct VertexShaderInput
{
    float3 pos : POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
};

struct VertexShaderOutput
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
    uint shadingRate : SV_ShadingRate; // DirectX 12 Ultimate Variable Rate Shading
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    float4 pos = float4(input.pos, 1.0f);

    // Transform vertex position
    pos = mul(pos, model);
    pos = mul(pos, view);
    pos = mul(pos, projection);
    output.pos = pos;

    // Pass through texture coordinates and color
    output.texCoord = input.texCoord;
    output.color = input.color;

    // Variable Rate Shading logic for Xbox Series X optimization
    if (renderParams.x > 0.5f) // VRS enabled
    {
        // Determine shading rate based on various factors
        float2 screenPos = output.pos.xy / output.pos.w;
        float2 center = float2(0.0f, 0.0f);
        float distanceFromCenter = length(screenPos - center);
        
        // Use higher shading rates in peripheral areas for performance
        if (distanceFromCenter > 0.7f)
        {
            output.shadingRate = 4; // 2x2 shading rate for edges
        }
        else if (distanceFromCenter > 0.5f)
        {
            output.shadingRate = 2; // 2x1 or 1x2 shading rate for mid areas
        }
        else
        {
            output.shadingRate = 1; // 1x1 full rate for center
        }

        // Performance mode adjustments for Xbox Series S
        if (renderParams.y < 0.5f) // Performance mode
        {
            // More aggressive VRS for better performance
            if (distanceFromCenter > 0.5f)
            {
                output.shadingRate = 4; // 2x2 for larger area
            }
        }
    }
    else
    {
        // Standard 1x1 shading rate when VRS is disabled
        output.shadingRate = 1;
    }

    return output;
}