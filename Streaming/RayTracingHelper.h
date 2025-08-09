#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace moonlight_xbox_dx
{
    // Optional Ray Tracing support for Xbox Series X
    // Provides enhanced reflections and lighting for video streaming
    class RayTracingHelper
    {
    public:
        RayTracingHelper(ID3D12Device5* device);
        ~RayTracingHelper() = default;

        // Initialize ray tracing resources
        bool Initialize(UINT width, UINT height);
        void Shutdown();

        // Ray tracing operations
        void BuildAccelerationStructures();
        void CreateRayTracingPipeline();
        void DispatchRayTracing(ID3D12GraphicsCommandList4* commandList);

        // Resource accessors
        ID3D12Resource* GetRayTracingOutput() const { return m_rayTracingOutput.Get(); }
        bool IsInitialized() const { return m_initialized; }

        // Configuration
        void SetEnabled(bool enabled) { m_enabled = enabled; }
        bool IsEnabled() const { return m_enabled; }

    private:
        void CreateRayTracingRootSignature();
        void CreateRayTracingShaderTable();
        void CreateAccelerationStructureBuffers();

        Microsoft::WRL::ComPtr<ID3D12Device5> m_device;
        Microsoft::WRL::ComPtr<ID3D12StateObject> m_rayTracingPipelineState;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rayTracingGlobalRootSignature;
        
        // Ray tracing output
        Microsoft::WRL::ComPtr<ID3D12Resource> m_rayTracingOutput;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_rayTracingOutputUAV;

        // Acceleration structures
        Microsoft::WRL::ComPtr<ID3D12Resource> m_topLevelAccelerationStructure;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_bottomLevelAccelerationStructure;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_scratchResource;

        // Shader table
        Microsoft::WRL::ComPtr<ID3D12Resource> m_rayGenShaderTable;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_missShaderTable;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_hitGroupShaderTable;

        // Descriptor heaps
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rayTracingDescriptorHeap;

        UINT m_width;
        UINT m_height;
        bool m_initialized;
        bool m_enabled;

        static const UINT c_numRayTracingDescriptors = 4;
    };
}