#include "pch.h"
#include "VideoRendererD3D12.h"
#include "../Common/DirectXHelper.h"
#include <State/MoonlightClient.h>
#include <Utils.hpp>

extern "C" {
#include "libgamestream/client.h"
#include <Limelight.h>
#include <libavcodec/avcodec.h>
}

using namespace moonlight_xbox_dx;
using namespace DirectX;
using namespace Windows::Foundation;
using namespace Microsoft::WRL;

VideoRendererD3D12::VideoRendererD3D12(const std::shared_ptr<DX::DeviceResourcesD3D12>& deviceResources,
                                       MoonlightClient* client,
                                       StreamConfiguration^ sConfig) :
    VideoRenderer(std::static_pointer_cast<DX::DeviceResources>(deviceResources), client, sConfig),
    m_deviceResourcesD3D12(deviceResources),
    m_currentShadingRate(D3D12_SHADING_RATE_1X1),
    m_vrsEnabled(false),
    m_rayTracingEnabled(false),
    m_xboxSeriesXOptimized(false),
    m_performanceMode(false),
    m_srvDescriptorSize(0)
{
    // Check if this is Xbox Series X and enable optimizations
    if (m_deviceResourcesD3D12->IsXboxSeriesX())
    {
        OptimizeForXboxSeriesX();
    }
}

VideoRendererD3D12::~VideoRendererD3D12()
{
    ReleaseDeviceDependentResources();
}

void VideoRendererD3D12::CreateDeviceDependentResources()
{
    if (m_deviceResourcesD3D12->SupportsD3D12Ultimate())
    {
        CreateD3D12Resources();
    }
    else
    {
        // Fall back to base D3D11 implementation
        VideoRenderer::CreateDeviceDependentResources();
    }
}

void VideoRendererD3D12::CreateD3D12Resources()
{
    auto device = m_deviceResourcesD3D12->GetD3D12Device();
    if (!device)
        return;

    CreateD3D12RootSignature();
    CreateD3D12PipelineState();
    CreateD3D12CommandList();

    // Create SRV descriptor heap for textures
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 4; // Y, U, V textures + enhanced texture
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    DX::ThrowIfFailed(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvDescriptorHeap)));

    m_srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Enable Xbox Series X specific features if available
    if (m_deviceResourcesD3D12->IsXboxSeriesX())
    {
        if (m_deviceResourcesD3D12->CheckVariableRateShadingSupport())
        {
            CreateD3D12VRSResources();
            EnableVariableRateShading(true);
        }

        if (m_deviceResourcesD3D12->CheckRayTracingSupport())
        {
            CreateD3D12RayTracingResources();
            EnableRayTracing(false); // Optional enhancement, disabled by default
        }
    }
}

void VideoRendererD3D12::CreateD3D12RootSignature()
{
    auto device = m_deviceResourcesD3D12->GetD3D12Device();

    // Create root signature for video rendering
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    // Define root parameters
    D3D12_DESCRIPTOR_RANGE1 ranges[1];
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 3; // Y, U, V textures
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER1 rootParameters[2];
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = ranges;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 0;
    rootParameters[1].Descriptor.RegisterSpace = 0;
    rootParameters[1].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

    // Static sampler for texture sampling
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootSignatureDesc.Desc_1_1.NumParameters = _countof(rootParameters);
    rootSignatureDesc.Desc_1_1.pParameters = rootParameters;
    rootSignatureDesc.Desc_1_1.NumStaticSamplers = 1;
    rootSignatureDesc.Desc_1_1.pStaticSamplers = &sampler;
    rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    DX::ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
    DX::ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}

void VideoRendererD3D12::CreateD3D12PipelineState()
{
    auto device = m_deviceResourcesD3D12->GetD3D12Device();

    // Load shaders (using same HLSL shaders as D3D11 version)
    // In a real implementation, you'd load D3D12-compatible shaders
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;

    // For now, use placeholder pipeline state
    // TODO: Load actual shaders compiled for D3D12

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = {}; // Define input layout for vertex data
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = {}; // Vertex shader bytecode
    psoDesc.PS = {}; // Pixel shader bytecode
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.RasterizerState.MultisampleEnable = FALSE;
    psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    psoDesc.RasterizerState.ForcedSampleCount = 0;
    psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
        FALSE, FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL,
    };
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        psoDesc.BlendState.RenderTarget[i] = defaultRenderTargetBlendDesc;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = m_deviceResourcesD3D12->GetBackBufferFormat();
    psoDesc.SampleDesc.Count = 1;

    // Enable Variable Rate Shading if supported
    if (m_vrsEnabled && m_deviceResourcesD3D12->CheckVariableRateShadingSupport())
    {
        psoDesc.ViewInstancingDesc.Flags = D3D12_VIEW_INSTANCING_FLAG_ENABLE_VIEW_INSTANCE_MASKING;
    }

    // Note: Pipeline state creation will be completed when actual shaders are loaded
    // DX::ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
}

void VideoRendererD3D12::CreateD3D12CommandList()
{
    auto device = m_deviceResourcesD3D12->GetD3D12Device();

    DX::ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
    DX::ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
    DX::ThrowIfFailed(m_commandList->Close());
}

void VideoRendererD3D12::CreateD3D12VRSResources()
{
    if (!m_deviceResourcesD3D12->CheckVariableRateShadingSupport())
        return;

    auto device = m_deviceResourcesD3D12->GetD3D12Device();
    auto outputSize = m_deviceResourcesD3D12->GetOutputSize();

    // Create VRS texture for per-region shading rate control
    D3D12_RESOURCE_DESC vrsTextureDesc = {};
    vrsTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    vrsTextureDesc.Width = static_cast<UINT>(outputSize.Width / 8); // VRS tile size is typically 8x8
    vrsTextureDesc.Height = static_cast<UINT>(outputSize.Height / 8);
    vrsTextureDesc.DepthOrArraySize = 1;
    vrsTextureDesc.MipLevels = 1;
    vrsTextureDesc.Format = DXGI_FORMAT_R8_UINT;
    vrsTextureDesc.SampleDesc.Count = 1;
    vrsTextureDesc.SampleDesc.Quality = 0;
    vrsTextureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    vrsTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    DX::ThrowIfFailed(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &vrsTextureDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_vrsTexture)));
}

void VideoRendererD3D12::CreateD3D12RayTracingResources()
{
    if (!m_deviceResourcesD3D12->CheckRayTracingSupport())
        return;

    auto device = m_deviceResourcesD3D12->GetD3D12Device();
    if (!device)
        return;

    // Create ray tracing helper
    m_rayTracingHelper = std::make_unique<RayTracingHelper>(device);
    
    auto outputSize = m_deviceResourcesD3D12->GetOutputSize();
    if (m_rayTracingHelper->Initialize(static_cast<UINT>(outputSize.Width), static_cast<UINT>(outputSize.Height)))
    {
        // Ray tracing successfully initialized
        m_rayTracingHelper->SetEnabled(false); // Start disabled for performance
    }
}

void VideoRendererD3D12::EnableVariableRateShading(bool enable)
{
    if (m_deviceResourcesD3D12->CheckVariableRateShadingSupport())
    {
        m_vrsEnabled = enable;
        m_deviceResourcesD3D12->EnableVariableRateShading(enable);
    }
}

void VideoRendererD3D12::EnableRayTracing(bool enable)
{
    if (m_deviceResourcesD3D12->CheckRayTracingSupport() && m_rayTracingHelper)
    {
        m_rayTracingEnabled = enable;
        m_rayTracingHelper->SetEnabled(enable);
        m_deviceResourcesD3D12->EnableRayTracing(enable);
    }
}

void VideoRendererD3D12::SetShadingRate(D3D12_SHADING_RATE rate)
{
    if (m_vrsEnabled)
    {
        m_currentShadingRate = rate;
    }
}

void VideoRendererD3D12::OptimizeForXboxSeriesX()
{
    m_xboxSeriesXOptimized = true;
    
    // Enable performance optimizations for Xbox Series X
    if (m_deviceResourcesD3D12->IsXboxSeriesX())
    {
        // Enable Variable Rate Shading for performance
        EnableVariableRateShading(true);
        SetShadingRate(D3D12_SHADING_RATE_2X2); // Reduce shading rate for better performance
        
        // Configure for optimal memory usage on Xbox Series X
        ConfigurePerformanceMode(true);
    }
}

void VideoRendererD3D12::ConfigurePerformanceMode(bool prioritizePerformance)
{
    m_performanceMode = prioritizePerformance;
    
    if (prioritizePerformance && m_vrsEnabled)
    {
        // Use more aggressive VRS settings for better performance
        SetShadingRate(D3D12_SHADING_RATE_2X2);
    }
    else
    {
        // Use higher quality settings
        SetShadingRate(D3D12_SHADING_RATE_1X1);
    }
}

void VideoRendererD3D12::CreateWindowSizeDependentResources()
{
    if (m_deviceResourcesD3D12->SupportsD3D12Ultimate())
    {
        // Recreate VRS resources with new window size
        if (m_vrsEnabled)
        {
            CreateD3D12VRSResources();
        }
    }
    else
    {
        VideoRenderer::CreateWindowSizeDependentResources();
    }
}

void VideoRendererD3D12::ReleaseDeviceDependentResources()
{
    m_rootSignature.Reset();
    m_pipelineState.Reset();
    m_commandList.Reset();
    m_commandAllocator.Reset();
    m_vrsTexture.Reset();
    m_rayTracingPipelineState.Reset();
    m_rayTracingOutput.Reset();
    m_textureUploadBuffer.Reset();
    m_enhancedTexture.Reset();
    m_srvDescriptorHeap.Reset();

    // Clean up ray tracing resources
    if (m_rayTracingHelper)
    {
        m_rayTracingHelper->Shutdown();
        m_rayTracingHelper.reset();
    }

    // Call base class cleanup
    VideoRenderer::ReleaseDeviceDependentResources();
}

bool VideoRendererD3D12::Render()
{
    if (m_deviceResourcesD3D12->SupportsD3D12Ultimate())
    {
        return RenderWithD3D12();
    }
    else
    {
        // Fall back to D3D11 rendering
        return VideoRenderer::Render();
    }
}

bool VideoRendererD3D12::RenderWithD3D12()
{
    // For now, fall back to D3D11 rendering until D3D12 shaders are implemented
    // In a complete implementation, this would use the D3D12 command list and pipeline
    return VideoRenderer::Render();
}

void VideoRendererD3D12::RenderWithVRS()
{
    if (!m_vrsEnabled || !m_commandList)
        return;

    // Apply Variable Rate Shading
    if (m_deviceResourcesD3D12->CheckVariableRateShadingSupport())
    {
        // Set the shading rate for the current frame
        m_commandList->RSSetShadingRate(m_currentShadingRate, nullptr);
        
        // Optionally use VRS texture for per-region control
        if (m_vrsTexture)
        {
            D3D12_SHADING_RATE_COMBINER combiners[2] = {
                D3D12_SHADING_RATE_COMBINER_PASSTHROUGH,
                D3D12_SHADING_RATE_COMBINER_OVERRIDE
            };
            m_commandList->RSSetShadingRateImage(m_vrsTexture.Get());
        }
    }
}

void VideoRendererD3D12::ApplyXboxSeriesXOptimizations()
{
    if (!m_xboxSeriesXOptimized)
        return;

    // Apply Xbox Series X specific optimizations
    RenderWithVRS();
    
    // Additional optimizations could include:
    // - Memory bandwidth optimizations
    // - Texture compression improvements
    // - Enhanced color space handling for HDR
}