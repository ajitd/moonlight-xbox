#include "pch.h"
#include "XboxHardwareDetection.h"

using namespace moonlight_xbox_dx;

XboxHardwareDetection::XboxHardwareDetection() :
    m_isXboxSeriesX(false),
    m_isXboxSeriesS(false),
    m_isXboxOne(false),
    m_isXboxOneX(false),
    m_supportsD3D12Ultimate(false),
    m_supportsRayTracing(false),
    m_supportsVRS(false),
    m_supportsMeshShaders(false),
    m_supportsHardwareDecompression(false),
    m_totalVideoMemory(0),
    m_availableVideoMemory(0)
{
    ZeroMemory(&m_deviceInfo, sizeof(m_deviceInfo));
    DetectHardware();
    DetectCapabilities();
}

void XboxHardwareDetection::DetectHardware()
{
    // Get gaming device information
    HRESULT hr = GetGamingDeviceModelInformation(&m_deviceInfo);
    if (SUCCEEDED(hr))
    {
        switch (m_deviceInfo.deviceId)
        {
        case GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_X:
            m_isXboxSeriesX = true;
            m_totalVideoMemory = 16ULL * 1024 * 1024 * 1024; // 16GB GDDR6
            m_availableVideoMemory = 13ULL * 1024 * 1024 * 1024; // ~13GB available for games
            break;

        case GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_S:
            m_isXboxSeriesS = true;
            m_totalVideoMemory = 10ULL * 1024 * 1024 * 1024; // 10GB GDDR6
            m_availableVideoMemory = 7ULL * 1024 * 1024 * 1024; // ~7GB available for games
            break;

        case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X:
            m_isXboxOne = true;
            m_isXboxOneX = true;
            m_totalVideoMemory = 12ULL * 1024 * 1024 * 1024; // 12GB GDDR5
            m_availableVideoMemory = 9ULL * 1024 * 1024 * 1024; // ~9GB available for games
            break;

        case GAMING_DEVICE_DEVICE_ID_XBOX_ONE:
        case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_S:
            m_isXboxOne = true;
            m_totalVideoMemory = 8ULL * 1024 * 1024 * 1024; // 8GB DDR3
            m_availableVideoMemory = 5ULL * 1024 * 1024 * 1024; // ~5GB available for games
            break;

        default:
            // Unknown or non-Xbox platform
            m_totalVideoMemory = 4ULL * 1024 * 1024 * 1024; // Conservative default
            m_availableVideoMemory = 2ULL * 1024 * 1024 * 1024;
            break;
        }
    }
    else
    {
        // Fallback for when gaming device information is not available
        m_totalVideoMemory = 4ULL * 1024 * 1024 * 1024;
        m_availableVideoMemory = 2ULL * 1024 * 1024 * 1024;
    }
}

void XboxHardwareDetection::DetectCapabilities()
{
    // Xbox Series X|S support DirectX 12 Ultimate
    if (m_isXboxSeriesX || m_isXboxSeriesS)
    {
        m_supportsD3D12Ultimate = true;
        m_supportsRayTracing = true;
        m_supportsVRS = true;
        m_supportsMeshShaders = true;
        m_supportsHardwareDecompression = true;
    }
    // Xbox One X has limited DirectX 12 support
    else if (m_isXboxOneX)
    {
        m_supportsD3D12Ultimate = false; // Basic D3D12 only
        m_supportsRayTracing = false;
        m_supportsVRS = false;
        m_supportsMeshShaders = false;
        m_supportsHardwareDecompression = true;
    }
    // Original Xbox One and One S
    else if (m_isXboxOne)
    {
        m_supportsD3D12Ultimate = false;
        m_supportsRayTracing = false;
        m_supportsVRS = false;
        m_supportsMeshShaders = false;
        m_supportsHardwareDecompression = false;
    }
}

UINT XboxHardwareDetection::GetRecommendedStreamingBitrate() const
{
    if (m_isXboxSeriesX)
    {
        return 150000; // 150 Mbps for Series X (4K HDR capability)
    }
    else if (m_isXboxSeriesS)
    {
        return 75000; // 75 Mbps for Series S (1440p optimized)
    }
    else if (m_isXboxOneX)
    {
        return 100000; // 100 Mbps for One X (4K capability)
    }
    else if (m_isXboxOne)
    {
        return 50000; // 50 Mbps for original Xbox One
    }
    else
    {
        return 30000; // Conservative default
    }
}

UINT XboxHardwareDetection::GetMaxSupportedResolutionWidth() const
{
    if (m_isXboxSeriesX || m_isXboxOneX)
    {
        return 3840; // 4K
    }
    else if (m_isXboxSeriesS)
    {
        return 2560; // 1440p
    }
    else
    {
        return 1920; // 1080p
    }
}

UINT XboxHardwareDetection::GetMaxSupportedResolutionHeight() const
{
    if (m_isXboxSeriesX || m_isXboxOneX)
    {
        return 2160; // 4K
    }
    else if (m_isXboxSeriesS)
    {
        return 1440; // 1440p
    }
    else
    {
        return 1080; // 1080p
    }
}

UINT XboxHardwareDetection::GetMaxSupportedFrameRate() const
{
    if (m_isXboxSeriesX || m_isXboxSeriesS)
    {
        return 120; // 120fps capability
    }
    else
    {
        return 60; // 60fps for Xbox One family
    }
}

bool XboxHardwareDetection::ShouldUseVariableRateShading() const
{
    // Enable VRS on next-gen Xbox for better performance
    return m_supportsVRS && (m_isXboxSeriesX || m_isXboxSeriesS);
}

bool XboxHardwareDetection::ShouldEnableRayTracing() const
{
    // Only enable ray tracing on Xbox Series X for optional enhancements
    // Series S can support it but may prefer performance
    return m_supportsRayTracing && m_isXboxSeriesX;
}

bool XboxHardwareDetection::ShouldPrioritizePerformance() const
{
    // Xbox Series S prioritizes performance over visual quality
    // Xbox One family needs performance optimizations
    return m_isXboxSeriesS || m_isXboxOne;
}