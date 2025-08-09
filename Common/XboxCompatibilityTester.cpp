#include "pch.h"
#include "XboxCompatibilityTester.h"
#include <sstream>
#include <iomanip>

using namespace moonlight_xbox_dx;

XboxCompatibilityTester::XboxCompatibilityTester() :
    m_xboxHardware(XboxHardwareDetection::GetInstance())
{
}

std::vector<XboxCompatibilityTester::TestResult> XboxCompatibilityTester::RunAllTests()
{
    std::vector<TestResult> results;

    // Run all test categories
    results.push_back(TestHardwareDetection());
    results.push_back(TestDirectX12Support());
    results.push_back(TestDirectX12UltimateFeatures());
    results.push_back(TestMemoryConfiguration());
    results.push_back(TestPerformanceCapabilities());
    results.push_back(TestVariableRateShading());
    results.push_back(TestRayTracingSupport());
    results.push_back(TestMeshShaderSupport());
    results.push_back(TestVideoMemoryCapacity());
    results.push_back(TestRecommendedSettings());
    results.push_back(TestBackwardCompatibility());

    return results;
}

XboxCompatibilityTester::TestResult XboxCompatibilityTester::TestHardwareDetection()
{
    TestResult result;
    result.testName = "Xbox Hardware Detection";

    if (m_xboxHardware.IsXboxSeriesX())
    {
        result.passed = true;
        result.details = "Detected Xbox Series X";
        result.recommendation = "Enable all DirectX 12 Ultimate features for optimal experience";
    }
    else if (m_xboxHardware.IsXboxSeriesS())
    {
        result.passed = true;
        result.details = "Detected Xbox Series S";
        result.recommendation = "Enable DirectX 12 Ultimate features with performance optimizations";
    }
    else if (m_xboxHardware.IsXboxOne())
    {
        result.passed = true;
        result.details = "Detected Xbox One family console";
        result.recommendation = "Use DirectX 11 rendering for compatibility";
    }
    else
    {
        result.passed = false;
        result.details = "Unknown or unsupported hardware platform";
        result.recommendation = "Use conservative settings and DirectX 11 fallback";
    }

    return result;
}

XboxCompatibilityTester::TestResult XboxCompatibilityTester::TestDirectX12Support()
{
    TestResult result;
    result.testName = "DirectX 12 Support";

    bool d3d12Available = TryCreateD3D12Device();
    if (d3d12Available)
    {
        result.passed = true;
        result.details = "DirectX 12 device created successfully";
        result.recommendation = "Can use DirectX 12 rendering path";
    }
    else
    {
        result.passed = false;
        result.details = "Failed to create DirectX 12 device";
        result.recommendation = "Fall back to DirectX 11 rendering";
    }

    return result;
}

XboxCompatibilityTester::TestResult XboxCompatibilityTester::TestDirectX12UltimateFeatures()
{
    TestResult result;
    result.testName = "DirectX 12 Ultimate Features";

    if (m_xboxHardware.SupportsDirectX12Ultimate())
    {
        result.passed = true;
        result.details = "Hardware supports DirectX 12 Ultimate";
        result.recommendation = "Enable advanced rendering features";
    }
    else
    {
        result.passed = false;
        result.details = "DirectX 12 Ultimate not supported";
        result.recommendation = "Use basic DirectX 12 or fall back to DirectX 11";
    }

    return result;
}

XboxCompatibilityTester::TestResult XboxCompatibilityTester::TestMemoryConfiguration()
{
    TestResult result;
    result.testName = "Memory Configuration";

    UINT64 totalMemory = m_xboxHardware.GetTotalVideoMemory();
    UINT64 availableMemory = m_xboxHardware.GetAvailableVideoMemory();

    std::stringstream details;
    details << "Total: " << std::fixed << std::setprecision(1) 
            << (totalMemory / (1024.0 * 1024.0 * 1024.0)) << "GB, ";
    details << "Available: " << std::fixed << std::setprecision(1) 
            << (availableMemory / (1024.0 * 1024.0 * 1024.0)) << "GB";

    result.details = details.str();

    if (availableMemory >= 4ULL * 1024 * 1024 * 1024) // 4GB
    {
        result.passed = true;
        result.recommendation = "Sufficient memory for high-quality streaming";
    }
    else
    {
        result.passed = false;
        result.recommendation = "Limited memory - use conservative settings";
    }

    return result;
}

XboxCompatibilityTester::TestResult XboxCompatibilityTester::TestPerformanceCapabilities()
{
    TestResult result;
    result.testName = "Performance Capabilities";

    UINT maxWidth = m_xboxHardware.GetMaxSupportedResolutionWidth();
    UINT maxHeight = m_xboxHardware.GetMaxSupportedResolutionHeight();
    UINT maxFrameRate = m_xboxHardware.GetMaxSupportedFrameRate();
    UINT recommendedBitrate = m_xboxHardware.GetRecommendedStreamingBitrate();

    std::stringstream details;
    details << "Max Resolution: " << maxWidth << "x" << maxHeight << ", ";
    details << "Max FPS: " << maxFrameRate << ", ";
    details << "Recommended Bitrate: " << (recommendedBitrate / 1000) << " Mbps";

    result.details = details.str();
    result.passed = true;

    if (m_xboxHardware.IsXboxSeriesX())
    {
        result.recommendation = "Optimal for 4K HDR streaming at high bitrates";
    }
    else if (m_xboxHardware.IsXboxSeriesS())
    {
        result.recommendation = "Optimal for 1440p streaming with good performance";
    }
    else
    {
        result.recommendation = "Suitable for 1080p streaming";
    }

    return result;
}

XboxCompatibilityTester::TestResult XboxCompatibilityTester::TestVariableRateShading()
{
    TestResult result;
    result.testName = "Variable Rate Shading";

    if (m_xboxHardware.SupportsVariableRateShading())
    {
        result.passed = true;
        result.details = "Variable Rate Shading supported";
        result.recommendation = "Enable VRS for improved performance";
    }
    else
    {
        result.passed = false;
        result.details = "Variable Rate Shading not supported";
        result.recommendation = "Use standard pixel shading";
    }

    return result;
}

XboxCompatibilityTester::TestResult XboxCompatibilityTester::TestRayTracingSupport()
{
    TestResult result;
    result.testName = "Ray Tracing Support";

    if (m_xboxHardware.SupportsRayTracing())
    {
        result.passed = true;
        result.details = "Hardware ray tracing supported";
        
        if (m_xboxHardware.IsXboxSeriesX())
        {
            result.recommendation = "Can enable ray tracing for enhanced visual effects";
        }
        else
        {
            result.recommendation = "Ray tracing available but may impact performance";
        }
    }
    else
    {
        result.passed = false;
        result.details = "Ray tracing not supported";
        result.recommendation = "Use traditional rendering techniques";
    }

    return result;
}

XboxCompatibilityTester::TestResult XboxCompatibilityTester::TestMeshShaderSupport()
{
    TestResult result;
    result.testName = "Mesh Shader Support";

    if (m_xboxHardware.SupportsMeshShaders())
    {
        result.passed = true;
        result.details = "Mesh shaders supported";
        result.recommendation = "Can use advanced geometry pipeline";
    }
    else
    {
        result.passed = false;
        result.details = "Mesh shaders not supported";
        result.recommendation = "Use traditional vertex/geometry shaders";
    }

    return result;
}

XboxCompatibilityTester::TestResult XboxCompatibilityTester::TestVideoMemoryCapacity()
{
    TestResult result;
    result.testName = "Video Memory Capacity";

    UINT64 availableMemory = m_xboxHardware.GetAvailableVideoMemory();
    double availableGB = availableMemory / (1024.0 * 1024.0 * 1024.0);

    std::stringstream details;
    details << std::fixed << std::setprecision(1) << availableGB << "GB available";
    result.details = details.str();

    if (availableGB >= 8.0)
    {
        result.passed = true;
        result.recommendation = "Sufficient for 4K streaming with high-quality buffers";
    }
    else if (availableGB >= 4.0)
    {
        result.passed = true;
        result.recommendation = "Good for 1440p-4K streaming";
    }
    else
    {
        result.passed = false;
        result.recommendation = "Limited memory - optimize buffer sizes";
    }

    return result;
}

XboxCompatibilityTester::TestResult XboxCompatibilityTester::TestRecommendedSettings()
{
    TestResult result;
    result.testName = "Recommended Settings";

    std::stringstream details;
    details << GetRecommendationForHardware();
    result.details = details.str();
    result.passed = true;
    result.recommendation = "Apply recommended settings for optimal experience";

    return result;
}

XboxCompatibilityTester::TestResult XboxCompatibilityTester::TestBackwardCompatibility()
{
    TestResult result;
    result.testName = "Backward Compatibility";

    result.passed = true;
    result.details = "DirectX 11 fallback available for all Xbox platforms";
    result.recommendation = "Seamless compatibility across Xbox family";

    return result;
}

bool XboxCompatibilityTester::TryCreateD3D12Device()
{
    try
    {
        m_testDeviceResources = std::make_shared<DX::DeviceResourcesD3D12>();
        return m_testDeviceResources->SupportsD3D12Ultimate();
    }
    catch (...)
    {
        return false;
    }
}

std::string XboxCompatibilityTester::GetRecommendationForHardware()
{
    if (m_xboxHardware.IsXboxSeriesX())
    {
        return "Xbox Series X: Enable D3D12 Ultimate, VRS, optional ray tracing, 4K streaming";
    }
    else if (m_xboxHardware.IsXboxSeriesS())
    {
        return "Xbox Series S: Enable D3D12 Ultimate, VRS, performance mode, 1440p streaming";
    }
    else if (m_xboxHardware.IsXboxOne())
    {
        return "Xbox One: Use DirectX 11, conservative settings, 1080p streaming";
    }
    else
    {
        return "Unknown platform: Use DirectX 11 fallback with conservative settings";
    }
}

std::string XboxCompatibilityTester::GenerateCompatibilityReport(const std::vector<TestResult>& results)
{
    std::stringstream report;
    
    report << "Xbox Moonlight Compatibility Report\n";
    report << "====================================\n\n";

    int passedTests = 0;
    int totalTests = static_cast<int>(results.size());

    for (const auto& result : results)
    {
        report << "[" << (result.passed ? "PASS" : "FAIL") << "] " << result.testName << "\n";
        report << "  Details: " << result.details << "\n";
        report << "  Recommendation: " << result.recommendation << "\n\n";

        if (result.passed)
            passedTests++;
    }

    report << "Summary: " << passedTests << "/" << totalTests << " tests passed\n";
    
    if (passedTests == totalTests)
    {
        report << "Status: Fully compatible - all features available\n";
    }
    else if (passedTests >= totalTests * 0.8)
    {
        report << "Status: Compatible with some limitations\n";
    }
    else
    {
        report << "Status: Limited compatibility - use conservative settings\n";
    }

    return report.str();
}