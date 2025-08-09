#pragma once

#include "VideoRenderer.h"
#include "RayTracingHelper.h"
#include "../Common/DeviceResourcesD3D12.h"
#include <d3d12.h>
#include <dxgi1_6.h>

namespace moonlight_xbox_dx
{
    // Enhanced VideoRenderer with DirectX 12 Ultimate support
    class VideoRendererD3D12 : public VideoRenderer
    {
    public:
        VideoRendererD3D12(const std::shared_ptr<DX::DeviceResourcesD3D12>& deviceResources, 
                          MoonlightClient* client, 
                          StreamConfiguration^ sConfig);
        virtual ~VideoRendererD3D12();

        // Override base class methods
        void CreateDeviceDependentResources() override;
        void CreateWindowSizeDependentResources() override;
        void ReleaseDeviceDependentResources() override;
        bool Render() override;

        // D3D12 Ultimate features
        void EnableVariableRateShading(bool enable);
        void EnableRayTracing(bool enable);
        void SetShadingRate(D3D12_SHADING_RATE rate);

        // Xbox Series X optimizations
        void OptimizeForXboxSeriesX();
        void ConfigurePerformanceMode(bool prioritizePerformance);

    private:
        void CreateD3D12Resources();
        void CreateD3D12RootSignature();
        void CreateD3D12PipelineState();
        void CreateD3D12CommandList();
        void CreateD3D12VRSResources();
        void CreateD3D12RayTracingResources();

        void UpdateConstantBuffers();
        void RenderWithD3D12();
        void RenderWithVRS();
        void ApplyXboxSeriesXOptimizations();

        // D3D12 resources
        std::shared_ptr<DX::DeviceResourcesD3D12> m_deviceResourcesD3D12;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> m_commandList;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;

        // Variable Rate Shading resources
        Microsoft::WRL::ComPtr<ID3D12Resource> m_vrsTexture;
        D3D12_SHADING_RATE m_currentShadingRate;
        bool m_vrsEnabled;

        // Ray tracing resources (optional)
        std::unique_ptr<RayTracingHelper> m_rayTracingHelper;
        Microsoft::WRL::ComPtr<ID3D12StateObject> m_rayTracingPipelineState;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_rayTracingOutput;
        bool m_rayTracingEnabled;

        // Xbox Series X specific optimizations
        bool m_xboxSeriesXOptimized;
        bool m_performanceMode;

        // Enhanced texture resources
        Microsoft::WRL::ComPtr<ID3D12Resource> m_textureUploadBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_enhancedTexture;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvDescriptorHeap;

        UINT m_srvDescriptorSize;
    };
}