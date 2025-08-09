#pragma once

#include "../Common/XboxHardwareDetection.h"
#include "../Common/DeviceResourcesD3D12.h"
#include <vector>
#include <string>

namespace moonlight_xbox_dx
{
    // Testing infrastructure for Xbox hardware compatibility
    class XboxCompatibilityTester
    {
    public:
        struct TestResult
        {
            std::string testName;
            bool passed;
            std::string details;
            std::string recommendation;
        };

        XboxCompatibilityTester();
        ~XboxCompatibilityTester() = default;

        // Run all compatibility tests
        std::vector<TestResult> RunAllTests();

        // Individual test categories
        TestResult TestHardwareDetection();
        TestResult TestDirectX12Support();
        TestResult TestDirectX12UltimateFeatures();
        TestResult TestMemoryConfiguration();
        TestResult TestPerformanceCapabilities();
        TestResult TestBackwardCompatibility();

        // Feature-specific tests
        TestResult TestVariableRateShading();
        TestResult TestRayTracingSupport();
        TestResult TestMeshShaderSupport();

        // Streaming-specific tests
        TestResult TestVideoMemoryCapacity();
        TestResult TestRecommendedSettings();

        // Get summary report
        std::string GenerateCompatibilityReport(const std::vector<TestResult>& results);

    private:
        const XboxHardwareDetection& m_xboxHardware;
        std::shared_ptr<DX::DeviceResourcesD3D12> m_testDeviceResources;

        // Helper methods
        bool TryCreateD3D12Device();
        bool TestFeatureSupport(D3D12_FEATURE feature, void* featureData, UINT dataSize);
        std::string GetRecommendationForHardware();
    };
}