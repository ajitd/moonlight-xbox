#pragma once

#include <windows.h>
#include <gamingdeviceinformation.h>

namespace moonlight_xbox_dx
{
    // Xbox hardware detection and capability management
    class XboxHardwareDetection
    {
    public:
        static XboxHardwareDetection& GetInstance()
        {
            static XboxHardwareDetection instance;
            return instance;
        }

        // Hardware detection
        bool IsXboxSeriesX() const { return m_isXboxSeriesX; }
        bool IsXboxSeriesS() const { return m_isXboxSeriesS; }
        bool IsXboxOne() const { return m_isXboxOne; }
        bool IsNextGenXbox() const { return m_isXboxSeriesX || m_isXboxSeriesS; }

        // Feature capabilities
        bool SupportsDirectX12Ultimate() const { return m_supportsD3D12Ultimate; }
        bool SupportsRayTracing() const { return m_supportsRayTracing; }
        bool SupportsVariableRateShading() const { return m_supportsVRS; }
        bool SupportsMeshShaders() const { return m_supportsMeshShaders; }
        bool SupportsHardwareDecompression() const { return m_supportsHardwareDecompression; }

        // Memory information
        UINT64 GetTotalVideoMemory() const { return m_totalVideoMemory; }
        UINT64 GetAvailableVideoMemory() const { return m_availableVideoMemory; }

        // Performance characteristics
        UINT GetRecommendedStreamingBitrate() const;
        UINT GetMaxSupportedResolutionWidth() const;
        UINT GetMaxSupportedResolutionHeight() const;
        UINT GetMaxSupportedFrameRate() const;

        // Feature recommendations
        bool ShouldUseVariableRateShading() const;
        bool ShouldEnableRayTracing() const;
        bool ShouldPrioritizePerformance() const;

    private:
        XboxHardwareDetection();
        ~XboxHardwareDetection() = default;

        // Prevent copying
        XboxHardwareDetection(const XboxHardwareDetection&) = delete;
        XboxHardwareDetection& operator=(const XboxHardwareDetection&) = delete;

        void DetectHardware();
        void DetectCapabilities();

        // Hardware flags
        bool m_isXboxSeriesX;
        bool m_isXboxSeriesS;
        bool m_isXboxOne;
        bool m_isXboxOneX;

        // Feature capabilities
        bool m_supportsD3D12Ultimate;
        bool m_supportsRayTracing;
        bool m_supportsVRS;
        bool m_supportsMeshShaders;
        bool m_supportsHardwareDecompression;

        // Memory information
        UINT64 m_totalVideoMemory;
        UINT64 m_availableVideoMemory;

        // Device information
        GAMING_DEVICE_MODEL_INFORMATION m_deviceInfo;
    };
}