#include "pch.h"
#include "DeviceResourcesD3D12.h"
#include "DirectXHelper.h"
#include <gamingdeviceinformation.h>
#include <windows.ui.xaml.media.dxinterop.h>

using namespace DX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::Gaming::Input;

// Constructor for DeviceResourcesD3D12
DeviceResourcesD3D12::DeviceResourcesD3D12() :
    m_frameIndex(0),
    m_rtvDescriptorSize(0),
    m_fenceEvent(nullptr),
    m_useD3D12(false),
    m_vrsEnabled(false),
    m_rayTracingEnabled(false),
    m_backBufferFormat(DXGI_FORMAT_R10G10B10A2_UNORM),
    m_swapChainPanel(nullptr)
{
    ZeroMemory(m_fenceValues, sizeof(m_fenceValues));
    
    // Set default output size
    m_outputSize.Width = 1920;
    m_outputSize.Height = 1080;
    m_d3dRenderTargetSize = m_outputSize;
    
    // Detect Xbox capabilities first
    m_xboxCaps = DetectXboxCapabilities();
    
    // Try to initialize D3D12 if supported
    if (m_xboxCaps.SupportsD3D12Ultimate)
    {
        try
        {
            if (InitializeD3D12())
            {
                m_useD3D12 = true;
            }
        }
        catch (...)
        {
            // Fall back to D3D11 on failure
            m_useD3D12 = false;
        }
    }
}

DeviceResourcesD3D12::~DeviceResourcesD3D12()
{
    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
    }
}

XboxCapabilities DeviceResourcesD3D12::DetectXboxCapabilities()
{
    XboxCapabilities caps = {};
    
    // Check if running on Xbox
    GAMING_DEVICE_MODEL_INFORMATION deviceInfo;
    if (SUCCEEDED(GetGamingDeviceModelInformation(&deviceInfo)))
    {
        switch (deviceInfo.deviceId)
        {
        case GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_X:
            caps.IsXboxSeriesX = true;
            caps.SupportsRayTracing = true;
            caps.SupportsVariableRateShading = true;
            caps.SupportsMeshShaders = true;
            caps.SupportsD3D12Ultimate = true;
            caps.TotalVideoMemory = 16 * 1024 * 1024 * 1024; // 16GB GDDR6
            break;
        case GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_S:
            caps.IsXboxSeriesS = true;
            caps.SupportsRayTracing = true;
            caps.SupportsVariableRateShading = true;
            caps.SupportsMeshShaders = true;
            caps.SupportsD3D12Ultimate = true;
            caps.TotalVideoMemory = 10 * 1024 * 1024 * 1024; // 10GB GDDR6
            break;
        case GAMING_DEVICE_DEVICE_ID_XBOX_ONE:
        case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_S:
        case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X:
            caps.IsXboxOne = true;
            caps.SupportsD3D12Ultimate = false; // Basic D3D12 support only
            caps.TotalVideoMemory = (deviceInfo.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X) ? 
                12 * 1024 * 1024 * 1024 : 8 * 1024 * 1024 * 1024;
            break;
        }
    }

    return caps;
}

bool DeviceResourcesD3D12::InitializeD3D12()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    // Create DXGI factory
    DX::ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_dxgiFactory)));

    // Try to create D3D12 device
    Microsoft::WRL::ComPtr<IDXGIAdapter1> hardwareAdapter;
    
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != m_dxgiFactory->EnumAdapters1(adapterIndex, &hardwareAdapter); ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        hardwareAdapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }

        if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_d3d12Device))))
        {
            break;
        }
    }

    if (!m_d3d12Device)
    {
        return false;
    }

    // Check for D3D12 Ultimate support
    if (!CheckD3D12UltimateSupport())
    {
        return false;
    }

    CreateD3D12CommandQueue();
    CreateD3D12Fence();

    return true;
}

void DeviceResourcesD3D12::CreateD3D12CommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    DX::ThrowIfFailed(m_d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
}

void DeviceResourcesD3D12::CreateD3D12Fence()
{
    DX::ThrowIfFailed(m_d3d12Device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValues[m_frameIndex]++;

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
    {
        DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
}

bool DeviceResourcesD3D12::CheckD3D12UltimateSupport()
{
    if (!m_d3d12Device)
        return false;

    // Check D3D12 Options5 for ray tracing
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    if (FAILED(m_d3d12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5))))
    {
        return false;
    }
    m_d3d12Options5 = options5;

    // Check D3D12 Options7 for mesh shaders and variable rate shading
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
    if (FAILED(m_d3d12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7))))
    {
        return false;
    }
    m_d3d12Options7 = options7;

    return (options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0 &&
            options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1 &&
            options7.VariableRateShadingTier >= D3D12_VARIABLE_RATE_SHADING_TIER_1);
}

bool DeviceResourcesD3D12::CheckRayTracingSupport()
{
    return m_d3d12Options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
}

bool DeviceResourcesD3D12::CheckVariableRateShadingSupport()
{
    return m_d3d12Options7.VariableRateShadingTier >= D3D12_VARIABLE_RATE_SHADING_TIER_1;
}

bool DeviceResourcesD3D12::CheckMeshShaderSupport()
{
    return m_d3d12Options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1;
}

void DeviceResourcesD3D12::EnableVariableRateShading(bool enable)
{
    if (CheckVariableRateShadingSupport())
    {
        m_vrsEnabled = enable;
    }
}

void DeviceResourcesD3D12::EnableRayTracing(bool enable)
{
    if (CheckRayTracingSupport())
    {
        m_rayTracingEnabled = enable;
    }
}

void DeviceResourcesD3D12::CreateD3D12WindowSizeDependentResources()
{
    if (m_useD3D12)
    {
        CreateD3D12SwapChain();
        CreateD3D12RenderTargets();
        CreateD3D12DepthStencil();
    }
}

void DeviceResourcesD3D12::CreateD3D12SwapChain()
{
    if (!m_swapChainPanel)
        return;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = static_cast<UINT>(m_d3dRenderTargetSize.Width);
    swapChainDesc.Height = static_cast<UINT>(m_d3dRenderTargetSize.Height);
    swapChainDesc.Format = m_backBufferFormat;
    swapChainDesc.Stereo = false;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = c_frameCount;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = 0;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    ComPtr<IDXGISwapChain1> swapChain;
    DX::ThrowIfFailed(GetDXGIFactory()->CreateSwapChainForComposition(
        m_commandQueue.Get(),
        &swapChainDesc,
        nullptr,
        &swapChain
    ));

    DX::ThrowIfFailed(swapChain.As(&m_d3d12SwapChain));

    // Associate swap chain with the SwapChainPanel
    ComPtr<ISwapChainPanelNative> panelNative;
    DX::ThrowIfFailed(reinterpret_cast<IUnknown*>(m_swapChainPanel)->QueryInterface(IID_PPV_ARGS(&panelNative)));
    DX::ThrowIfFailed(panelNative->SetSwapChain(m_d3d12SwapChain.Get()));

    m_frameIndex = m_d3d12SwapChain->GetCurrentBackBufferIndex();
}

void DeviceResourcesD3D12::CreateD3D12RenderTargets()
{
    // Create descriptor heap for render target views
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = c_frameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    DX::ThrowIfFailed(m_d3d12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

    m_rtvDescriptorSize = m_d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create render target views
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT n = 0; n < c_frameCount; n++)
    {
        DX::ThrowIfFailed(m_d3d12SwapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
        m_d3d12Device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_rtvDescriptorSize;
    }
}

void DeviceResourcesD3D12::CreateD3D12DepthStencil()
{
    // Create descriptor heap for depth stencil view
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    DX::ThrowIfFailed(m_d3d12Device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

    // Create depth stencil buffer
    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
    depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = static_cast<UINT>(m_d3dRenderTargetSize.Width);
    resourceDesc.Height = static_cast<UINT>(m_d3dRenderTargetSize.Height);
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 0;
    resourceDesc.Format = DXGI_FORMAT_D32_FLOAT;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    DX::ThrowIfFailed(m_d3d12Device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthOptimizedClearValue,
        IID_PPV_ARGS(&m_depthStencil)));

    m_d3d12Device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void DeviceResourcesD3D12::ValidateD3D12Device()
{
    if (m_useD3D12)
    {
        // D3D12 device validation
        if (!m_d3d12Device)
        {
            HandleD3D12DeviceLost();
        }
    }
}

void DeviceResourcesD3D12::HandleD3D12DeviceLost()
{
    if (m_useD3D12)
    {
        // Wait for GPU to finish
        WaitForGpu();
        
        // Reset D3D12 resources
        m_d3d12Device.Reset();
        m_commandQueue.Reset();
        m_commandList.Reset();
        for (auto& allocator : m_commandAllocators)
        {
            allocator.Reset();
        }
        m_d3d12SwapChain.Reset();
        m_rtvHeap.Reset();
        m_dsvHeap.Reset();
        for (auto& rt : m_renderTargets)
        {
            rt.Reset();
        }
        m_depthStencil.Reset();
        m_fence.Reset();

        // Try to reinitialize
        if (!InitializeD3D12())
        {
            // Fall back to D3D11
            m_useD3D12 = false;
        }
    }
}

void DeviceResourcesD3D12::PresentD3D12()
{
    if (m_useD3D12)
    {
        // Present the frame
        HRESULT hr = m_d3d12SwapChain->Present(1, 0); // Always use VSync for now
        
        if (FAILED(hr))
        {
            if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
            {
                HandleD3D12DeviceLost();
            }
            else
            {
                DX::ThrowIfFailed(hr);
            }
        }
        else
        {
            MoveToNextFrame();
        }
    }
}

void DeviceResourcesD3D12::WaitForGpu()
{
    if (m_commandQueue && m_fence && m_fenceEvent)
    {
        const UINT64 fence = m_fenceValues[m_frameIndex];
        DX::ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
        m_fenceValues[m_frameIndex]++;

        if (m_fence->GetCompletedValue() < fence)
        {
            DX::ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
            WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
        }
    }
}

void DeviceResourcesD3D12::MoveToNextFrame()
{
    const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
    DX::ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

    m_frameIndex = m_d3d12SwapChain->GetCurrentBackBufferIndex();

    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
    {
        DX::ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }

    m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

ID3D12Resource* DeviceResourcesD3D12::GetCurrentRenderTarget() const
{
    if (m_useD3D12)
    {
        return m_renderTargets[m_frameIndex].Get();
    }
    return nullptr;
}

D3D12_CPU_DESCRIPTOR_HANDLE DeviceResourcesD3D12::GetCurrentRenderTargetView() const
{
    if (m_useD3D12)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += static_cast<SIZE_T>(m_frameIndex) * static_cast<SIZE_T>(m_rtvDescriptorSize);
        return rtvHandle;
    }
    return {};
}