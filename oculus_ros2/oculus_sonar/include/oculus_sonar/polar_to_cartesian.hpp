/**
 * @file polar_to_cartesian.hpp
 * @brief Polar to Cartesian coordinate conversion for Oculus sonar images
 * @date 260206
 *
 * Converts Oculus sonar polar images (range x bearing) to Cartesian fan images.
 * Supports M750d, M1200d, and M3000d sonar models with configurable colormaps.
 */

#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <rclcpp/rclcpp.hpp>

#include "oculus_sonar/sonar_config.hpp"

namespace oculus_sonar {

/**
 * @brief Converts polar sonar images to Cartesian (fan) images
 *
 * This class handles the coordinate transformation from polar (range, bearing)
 * to Cartesian (x, y) coordinates. It uses precomputed lookup tables for
 * efficient real-time conversion.
 *
 * Key features:
 * - Supports M750d, M1200d, M3000d sonar models
 * - Uses actual bearings from sonar when available
 * - Maintains height parity: output rows = input rows (num_ranges)
 * - Configurable colormap with 20+ OpenCV options
 *
 * Usage:
 * @code
 *   PolarToCartesianConverter converter("m750d", 2);  // M750d, high freq
 *   converter.setColormap("viridis");
 *   converter.updateMapping(num_ranges, num_beams, max_range, azimuth_angles);
 *   cv::Mat fan = converter.convertToCartesian(polar_image, true);
 * @endcode
 */
class PolarToCartesianConverter {
public:
    /**
     * @brief Construct converter for specified sonar model
     * @param sonar_model Model name ("m750d", "m1200d", "m3000d")
     * @param freq_mode Frequency mode (1=low, 2=high)
     */
    explicit PolarToCartesianConverter(
        const std::string& sonar_model = "m750d",
        int freq_mode = 2);

    /**
     * @brief Default destructor
     */
    ~PolarToCartesianConverter() = default;

    //==========================================================================
    // Main Interface
    //==========================================================================

    /**
     * @brief Update mapping for new sonar parameters
     *
     * Call this when sonar dimensions or range change. The mapping is cached
     * and only regenerated when parameters actually change.
     *
     * @param num_ranges Number of range samples (polar image height)
     * @param num_beams Number of beams (polar image width)
     * @param max_range Maximum range in meters
     * @param azimuth_angles Actual bearing angles from sonar (radians).
     *                       If empty, uses model's default FOV.
     */
    void updateMapping(int num_ranges, int num_beams, float max_range,
                      const std::vector<float>& azimuth_angles = {});

    /**
     * @brief Convert polar image to Cartesian fan image
     *
     * @param polar_image Input polar image (rows=ranges, cols=beams)
     * @param apply_colormap Apply configured colormap to output
     * @return Cartesian fan image (MONO8 or BGR8 if colormap applied)
     */
    cv::Mat convertToCartesian(const cv::Mat& polar_image,
                               bool apply_colormap = true);

    //==========================================================================
    // Configuration
    //==========================================================================

    /**
     * @brief Set frequency mode
     * @param freq_mode 1=low frequency, 2=high frequency
     */
    void setFrequencyMode(int freq_mode);

    /**
     * @brief Set sonar model
     * @param model Model name ("m750d", "m1200d", "m3000d")
     */
    void setSonarModel(const std::string& model);

    /**
     * @brief Set colormap by name
     * @param colormap_name Colormap name (e.g., "viridis", "turbo", "hot")
     *
     * Supported colormaps:
     * - Classic: jet, hot, rainbow, ocean, bone, autumn, winter, summer,
     *           spring, cool, hsv, pink
     * - Perceptually uniform: viridis, plasma, magma, inferno, cividis,
     *                        parula, turbo, twilight
     */
    void setColormap(const std::string& colormap_name);

    /**
     * @brief Set colormap by OpenCV code
     * @param code OpenCV colormap code (cv::COLORMAP_*)
     */
    void setColormapCode(int code);

    //==========================================================================
    // State Queries
    //==========================================================================

    /** @brief Check if mapping is initialized */
    bool isInitialized() const { return initialized_; }

    /** @brief Get output image height (rows) */
    int getOutputRows() const { return rows_; }

    /** @brief Get output image width (cols) */
    int getOutputCols() const { return cols_; }

    /** @brief Get current sonar model name */
    const std::string& getSonarModel() const { return sonar_model_; }

    /** @brief Get current frequency mode */
    int getFrequencyMode() const { return freq_mode_; }

    /** @brief Get current colormap code */
    int getColormapCode() const { return colormap_code_; }

private:
    /**
     * @brief Generate Cartesian mapping matrices
     *
     * Creates lookup tables (map_x_, map_y_) for cv::remap().
     * Output height equals input height (num_ranges) for 1:1 range mapping.
     */
    void generateCartesianMapping();

    /**
     * @brief Interpolate bearing angle to beam index
     * @param bearing Bearing angle in radians
     * @return Interpolated beam index, or -1 if out of bounds
     */
    float interpolateBearing(float bearing) const;

    // Model configuration
    std::string sonar_model_;
    SonarModelConfig config_;
    FrequencyModeSpecs current_specs_;
    int freq_mode_;
    int colormap_code_;

    // Mapping state
    bool initialized_;
    int rows_;           // Output height (= num_ranges for 1:1 mapping)
    int cols_;           // Output width (calculated from FOV)

    // Sonar parameters
    int num_beams_;
    int num_ranges_;
    float range_resolution_;
    float max_range_;
    std::vector<float> bearings_;

    // Precomputed mapping matrices for cv::remap()
    cv::Mat map_x_;  // Beam index lookup
    cv::Mat map_y_;  // Range index lookup
};

}  // namespace oculus_sonar
