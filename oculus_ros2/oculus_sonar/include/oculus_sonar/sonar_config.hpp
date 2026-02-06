/**
 * @file sonar_config.hpp
 * @brief Centralized sonar model configurations and constants
 * @date 260206
 *
 * Provides configuration registry for Oculus sonar models (M750d, M1200d, M3000d)
 * and OpenCV colormap mappings.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>

namespace oculus_sonar {

//==============================================================================
// Named Constants
//==============================================================================
namespace SonarConstants {
    // Coordinate transform constant (matches Python implementation)
    constexpr float REVERSE_Z = 1.0f;

    // Default beam configuration
    constexpr int DEFAULT_MAX_BEAMS = 512;
    constexpr int HALF_BEAMS = 256;

    // Resolution constants (mm) - derived from sonar specifications
    constexpr float M750D_LOW_FREQ_RES_MM = 4.0f;    // 750kHz
    constexpr float M750D_HIGH_FREQ_RES_MM = 2.5f;   // 1.2MHz
    constexpr float M1200D_LOW_FREQ_RES_MM = 2.5f;   // 1.2MHz
    constexpr float M1200D_HIGH_FREQ_RES_MM = 1.5f;  // 2.1MHz
    constexpr float M3000D_LOW_FREQ_RES_MM = 2.5f;   // 1.2MHz
    constexpr float M3000D_HIGH_FREQ_RES_MM = 2.0f;  // 3.0MHz

    // Frequency mode identifiers
    constexpr int FREQ_MODE_LOW = 1;
    constexpr int FREQ_MODE_HIGH = 2;
}

//==============================================================================
// Data Structures
//==============================================================================

/**
 * @brief Specifications for a specific frequency mode
 */
struct FrequencyModeSpecs {
    float horizontal_fov_deg;    // Horizontal field of view (degrees)
    float vertical_fov_deg;      // Vertical field of view (degrees)
    float frequency_mhz;         // Operating frequency (MHz)
    float max_range_m;           // Maximum range (meters)
    float resolution_mm;         // Range resolution (mm)
};

/**
 * @brief Complete sonar model configuration
 */
struct SonarModelConfig {
    std::string model_name;
    FrequencyModeSpecs low_freq;   // freq_mode = 1
    FrequencyModeSpecs high_freq;  // freq_mode = 2
    int max_beams;
};

/**
 * @brief OpenCV colormap configuration
 */
struct ColormapConfig {
    std::string name;
    int opencv_code;
};

//==============================================================================
// Configuration Registry
//==============================================================================

/**
 * @brief Central registry for sonar model and colormap configurations
 *
 * Usage:
 *   auto config = SonarConfigRegistry::getModelConfig("m3000d");
 *   auto cmap = SonarConfigRegistry::getColormapConfig("viridis");
 */
class SonarConfigRegistry {
public:
    /**
     * @brief Get configuration for a sonar model
     * @param model Model name ("m750d", "m1200d", "m3000d")
     * @return Model configuration
     * @throws std::runtime_error if model not found
     */
    static const SonarModelConfig& getModelConfig(const std::string& model);

    /**
     * @brief Get colormap configuration by name
     * @param name Colormap name ("jet", "hot", "viridis", etc.)
     * @return Colormap configuration
     * @throws std::runtime_error if colormap not found
     */
    static const ColormapConfig& getColormapConfig(const std::string& name);

    /**
     * @brief Get colormap configuration by OpenCV code
     * @param code OpenCV colormap code (0-21)
     * @return Colormap configuration
     * @throws std::runtime_error if code not found
     */
    static const ColormapConfig& getColormapByCode(int code);

    /**
     * @brief Get list of supported sonar models
     */
    static std::vector<std::string> getSupportedModels();

    /**
     * @brief Get list of supported colormap names
     */
    static std::vector<std::string> getSupportedColormaps();

    /**
     * @brief Check if a model is supported
     */
    static bool isModelSupported(const std::string& model);

    /**
     * @brief Check if a colormap is supported
     */
    static bool isColormapSupported(const std::string& name);

private:
    static const std::unordered_map<std::string, SonarModelConfig> models_;
    static const std::unordered_map<std::string, ColormapConfig> colormaps_;
    static const std::unordered_map<int, std::string> colormap_code_to_name_;
};

//==============================================================================
// Utility Functions
//==============================================================================

/**
 * @brief Get frequency mode specs from model config
 * @param config Model configuration
 * @param freq_mode Frequency mode (1=low, 2=high)
 * @return Frequency mode specifications
 */
inline const FrequencyModeSpecs& getFreqModeSpecs(
    const SonarModelConfig& config, int freq_mode) {
    return (freq_mode == SonarConstants::FREQ_MODE_HIGH)
           ? config.high_freq : config.low_freq;
}

/**
 * @brief Calculate pixels per meter from resolution
 * @param resolution_mm Resolution in millimeters
 * @return Pixels per meter
 */
inline float resolutionToPixelsPerMeter(float resolution_mm) {
    return 1000.0f / resolution_mm;
}

}  // namespace oculus_sonar
