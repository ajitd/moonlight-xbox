#pragma once

#include "../State/Stats.h"
#include "DeviceResources.h"
#include <d3d12.h>
#include <dxgi1_6.h>

namespace DX
{
    // Xbox Series X hardware detection and capabilities
    struct XboxCapabilities
    {
        bool IsXboxSeriesX = false;
        bool IsXboxSeriesS = false;
        bool IsXboxOne = false;
        bool SupportsRayTracing = false;
        bool SupportsVariableRateShading = false;
        bool SupportsMeshShaders = false;
        bool SupportsD3D12Ultimate = false;
        UINT TotalVideoMemory = 0;
        UINT AvailableVideoMemory = 0;
    };

    // DirectX 12 Device Resources with backward compatibility
    class DeviceResourcesD3D12 : public DeviceResources
    {
    public:
        DeviceResourcesD3D12();
        virtual ~DeviceResourcesD3D12();

        // Override base class methods
        void CreateDeviceResources() override;
        void CreateWindowSizeDependentResources() override;
        void ValidateDevice() override;
        void HandleDeviceLost() override;
        void Present() override;

        // D3D12-specific methods
        bool InitializeD3D12();
        void CreateD3D12CommandQueue();
        void CreateD3D12SwapChain();
        void CreateD3D12RenderTargets();
        void CreateD3D12DepthStencil();
        void CreateD3D12Fence();

        // Xbox hardware detection
        XboxCapabilities DetectXboxCapabilities();
        bool IsXboxSeriesX() const { return m_xboxCaps.IsXboxSeriesX; }
        bool SupportsD3D12Ultimate() const { return m_xboxCaps.SupportsD3D12Ultimate; }

        // Variable Rate Shading support
        void EnableVariableRateShading(bool enable);
        bool IsVariableRateShadingEnabled() const { return m_vrsEnabled; }

        // Ray tracing support
        void EnableRayTracing(bool enable);
        bool IsRayTracingEnabled() const { return m_rayTracingEnabled; }

        // D3D12 Accessors
        ID3D12Device5* GetD3D12Device() const { return m_d3d12Device.Get(); }
        ID3D12CommandQueue* GetD3D12CommandQueue() const { return m_commandQueue.Get(); }
        ID3D12GraphicsCommandList4* GetD3D12CommandList() const { return m_commandList.Get(); }
        ID3D12Resource* GetCurrentRenderTarget() const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView() const;

        // Feature support queries
        bool CheckD3D12UltimateSupport();
        bool CheckRayTracingSupport();
        bool CheckVariableRateShadingSupport();
        bool CheckMeshShaderSupport();

    private:
        void WaitForGpu();
        void MoveToNextFrame();
        UINT GetCurrentFrameIndex() const { return m_frameIndex; }

        // D3D12 objects
        Microsoft::WRL::ComPtr<ID3D12Device5>           m_d3d12Device;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue>      m_commandQueue;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> m_commandList;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator>  m_commandAllocators[3];
        Microsoft::WRL::ComPtr<IDXGISwapChain4>         m_d3d12SwapChain;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    m_rtvHeap;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    m_dsvHeap;
        Microsoft::WRL::ComPtr<ID3D12Resource>          m_renderTargets[3];
        Microsoft::WRL::ComPtr<ID3D12Resource>          m_depthStencil;
        Microsoft::WRL::ComPtr<ID3D12Fence>             m_fence;

        // Synchronization objects
        HANDLE                                          m_fenceEvent;
        UINT64                                          m_fenceValues[3];
        UINT                                            m_frameIndex;
        UINT                                            m_rtvDescriptorSize;

        // Xbox capabilities and features
        XboxCapabilities                                m_xboxCaps;
        bool                                            m_useD3D12;
        bool                                            m_vrsEnabled;
        bool                                            m_rayTracingEnabled;

        // D3D12 Ultimate features
        Microsoft::WRL::ComPtr<ID3D12VariableRateShadingTier2> m_vrsInterface;
        D3D12_FEATURE_DATA_D3D12_OPTIONS5               m_d3d12Options5;
        D3D12_FEATURE_DATA_D3D12_OPTIONS7               m_d3d12Options7;

        static const UINT c_frameCount = 3;
    };
}