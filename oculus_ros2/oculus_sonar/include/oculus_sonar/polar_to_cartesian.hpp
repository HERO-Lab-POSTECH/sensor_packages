/**
 * @file polar_to_cartesian.hpp
 * @date 251125
 * @brief Utilities for converting Oculus sonar data from polar to Cartesian coordinates
 */

#pragma once

#include <opencv2/opencv.hpp>
#include <cmath>
#include <vector>
#include <rclcpp/rclcpp.hpp>
#include "liboculus/SimplePingResult.h"

namespace oculus_sonar {

/**
 * @brief Structure to hold sonar model specifications
 */
struct SonarSpecs {
    float horizontal_aperture_deg;  // Horizontal field of view in degrees
    float vertical_aperture_deg;    // Vertical field of view in degrees
    int max_beams;                  // Maximum number of beams
    float frequency_mhz;            // Operating frequency in MHz
};

/**
 * @brief Get specifications for M750d sonar
 */
inline SonarSpecs getM750dSpecs(bool high_freq = true) {
    SonarSpecs specs;
    specs.horizontal_aperture_deg = 130.0f;  // Always 130 degrees regardless of frequency
    specs.vertical_aperture_deg = 20.0f;     // Always 20 degrees
    specs.max_beams = 512;
    specs.frequency_mhz = high_freq ? 1.2f : 0.75f;
    return specs;
}

/**
 * @brief Get specifications for M1200d sonar
 */
inline SonarSpecs getM1200dSpecs(bool high_freq = true, bool narrow_mode = true) {
    SonarSpecs specs;
    specs.horizontal_aperture_deg = narrow_mode ? 130.0f : 60.0f;
    specs.vertical_aperture_deg = narrow_mode ? 20.0f : 12.0f;
    specs.max_beams = 512;
    specs.frequency_mhz = high_freq ? 2.1f : 1.2f;
    return specs;
}

/**
 * @brief Calculate pixels per meter based on sonar model and frequency
 * @param sonar_model Model name ("m750d" or "m1200d")
 * @param freq_mode Frequency mode (1 = low freq, 2 = high freq)
 * @return Pixels per meter for optimal resolution
 */
inline float getOptimalPixelsPerMeter(const std::string& sonar_model, int freq_mode) {
    if (sonar_model == "m750d") {
        // M750d: 750kHz = ~4mm resolution, 1.2MHz = ~2.5mm resolution
        return (freq_mode == 2) ? 400.0f : 250.0f;  // 2.5mm or 4mm per pixel
    } else if (sonar_model == "m1200d") {
        // M1200d: 1.2MHz = ~2.5mm resolution, 2.1MHz = ~1.5mm resolution
        return (freq_mode == 2) ? 667.0f : 400.0f;  // 1.5mm or 2.5mm per pixel
    } else {
        return 250.0f;  // Default 4mm resolution
    }
}

/**
 * @brief Class to handle polar to Cartesian conversion for Oculus sonar images
 */
class PolarToCartesianConverter {
public:
    /**
     * @param sonar_model Model name ("m750d" or "m1200d")
     * @param freq_mode Frequency mode (1 = low freq, 2 = high freq)
     */
    PolarToCartesianConverter(const std::string& sonar_model = "m750d", int freq_mode = 2)
        : sonar_model_(sonar_model),
          freq_mode_(freq_mode),
          initialized_(false),
          rows_(0),
          cols_(0) {

        // Set specifications based on model and frequency
        bool high_freq = (freq_mode == 2);
        if (sonar_model_ == "m750d") {
            specs_ = getM750dSpecs(high_freq);
        } else if (sonar_model_ == "m1200d") {
            specs_ = getM1200dSpecs(high_freq, true);  // Always use narrow mode (130°)
        } else {
            RCLCPP_WARN(rclcpp::get_logger("polar_converter"),
                       "Unknown sonar model: %s, using M750d specs", sonar_model_.c_str());
            specs_ = getM750dSpecs(high_freq);
        }

        // Set pixels_per_meter based on frequency
        pixels_per_meter_ = getOptimalPixelsPerMeter(sonar_model_, freq_mode);
    }

    /**
     * @brief Update frequency mode and recalculate resolution
     */
    void setFrequencyMode(int freq_mode) {
        if (freq_mode_ != freq_mode) {
            freq_mode_ = freq_mode;
            bool high_freq = (freq_mode == 2);

            // Update specs
            if (sonar_model_ == "m750d") {
                specs_ = getM750dSpecs(high_freq);
            } else if (sonar_model_ == "m1200d") {
                specs_ = getM1200dSpecs(high_freq, true);  // Always use narrow mode (130°)
            }

            // Update pixels_per_meter
            pixels_per_meter_ = getOptimalPixelsPerMeter(sonar_model_, freq_mode);
            initialized_ = false;  // Force regeneration of mapping
        }
    }

    /**
     * @brief Generate mapping for polar to Cartesian conversion
     * @tparam PingT Type of ping data
     * @param ping Input ping data containing bearing and range information
     */
    template<typename PingT>
    void generateMapping(const liboculus::SimplePingResult<PingT>& ping) {
        // Get ping parameters
        int num_beams = ping.ping()->nBeams;
        int num_ranges = ping.ping()->nRanges;
        float range_resolution = ping.ping()->rangeResolution;
        float max_range = num_ranges * range_resolution;

        // Check if we need to regenerate the mapping
        if (initialized_ &&
            num_beams == num_beams_ &&
            num_ranges == num_ranges_ &&
            std::abs(range_resolution - range_resolution_) < 1e-6) {
            return;
        }

        num_beams_ = num_beams;
        num_ranges_ = num_ranges;
        range_resolution_ = range_resolution;
        max_range_ = max_range;

        // Get bearing angles in radians
        bearings_.clear();

        // For M750d, we know it should be 130 degrees FOV with 512 beams
        // But hardware only provides ~70 degree span, so we'll scale it
        if (sonar_model_ == "m750d" && num_beams == 512) {
            // Expected FOV in radians (130 degrees)
            float expected_fov_rad = 130.0f * M_PI / 180.0f;
            float beam_spacing_rad = expected_fov_rad / (num_beams - 1);
            float start_bearing = -expected_fov_rad / 2.0f;

            for (int i = 0; i < num_beams; ++i) {
                bearings_.push_back(start_bearing + i * beam_spacing_rad);
            }

            RCLCPP_INFO(rclcpp::get_logger("polar_converter"),
                        "M750d: Manually setting 130° FOV for 512 beams");
        } else {
            // Use actual bearings from hardware
            for (int i = 0; i < num_beams; ++i) {
                bearings_.push_back(ping.bearings().at_rad(i));
            }
        }

        // Debug: Check actual FOV
        if (!bearings_.empty()) {
            float fov_rad = bearings_.back() - bearings_.front();
            float fov_deg = fov_rad * 180.0f / M_PI;

            // Check if bearings are spaced at 0.25 degrees
            float expected_fov = (num_beams - 1) * 0.25f;
            float bearing_spacing_deg = fov_deg / (num_beams - 1);

            RCLCPP_INFO(rclcpp::get_logger("polar_converter"),
                        "FOV: %.2f° (expected: %.1f°), beams: %d, spacing: %.4f° (expected 0.25°)",
                        fov_deg, expected_fov, num_beams, bearing_spacing_deg);
            RCLCPP_INFO(rclcpp::get_logger("polar_converter"),
                        "First bearing: %.4f rad (%.2f°), Last: %.4f rad (%.2f°)",
                        bearings_.front(), bearings_.front() * 180.0f / M_PI,
                        bearings_.back(), bearings_.back() * 180.0f / M_PI);
        }

        // Calculate output image dimensions for proper resolution
        // For M750d at 1.2MHz, resolution is about 2.5mm
        float bearing_span = bearings_.back() - bearings_.front();  // in radians
        float half_angle = bearing_span / 2.0f;

        // Width at maximum range (arc length)
        float arc_width = 2.0f * max_range * std::sin(half_angle);

        // Height is simply the max range
        float height = max_range;

        // Convert to pixels based on range resolution
        // Range resolution is approximately 2.5mm for 1.2MHz
        rows_ = static_cast<int>(std::ceil(height / range_resolution));  // height in pixels
        cols_ = static_cast<int>(std::ceil(arc_width / range_resolution));  // width in pixels

        RCLCPP_INFO(rclcpp::get_logger("polar_converter"),
                   "Fan image: %.1fm range, %.1f° FOV -> %d x %d pixels (%.1fmm resolution)",
                   max_range, bearing_span * 180.0f / M_PI, rows_, cols_, range_resolution * 1000.0f);

        // Create bearing interpolation function similar to Python's interp1d
        // This maps bearing angles to beam indices

        // Create meshgrid for mapping (Python style)
        map_x_ = cv::Mat(rows_, cols_, CV_32F);
        map_y_ = cv::Mat(rows_, cols_, CV_32F);

        for (int row = 0; row < rows_; ++row) {
            for (int col = 0; col < cols_; ++col) {
                // Python: x = self.res * (self.rows - YY)
                float x = range_resolution_ * (rows_ - row);

                // Python: y = self.res * (-self.cols / 2.0 + XX + 0.5)
                float y = range_resolution_ * (-cols_ / 2.0f + col + 0.5f);

                // Python: b = np.arctan2(y, x) * self.REVERSE_Z
                // Note: REVERSE_Z is typically -1 to flip the bearing
                float bearing = std::atan2(y, x) * -1.0f;

                // Python: r = np.sqrt(np.square(x) + np.square(y))
                float range = std::sqrt(x * x + y * y);

                // Convert to indices
                float range_idx = range / range_resolution_;

                // Find beam index by interpolating bearing
                float beam_idx = -1.0f;
                if (!bearings_.empty()) {
                    // Linear interpolation for bearing to beam index
                    if (bearing < bearings_.front() || bearing > bearings_.back()) {
                        beam_idx = -1.0f;  // Out of bounds
                    } else {
                        // Find the two bearings that bracket our bearing
                        for (size_t i = 1; i < bearings_.size(); ++i) {
                            if (bearing <= bearings_[i]) {
                                float t = (bearing - bearings_[i-1]) / (bearings_[i] - bearings_[i-1]);
                                beam_idx = static_cast<float>(i - 1) + t;
                                break;
                            }
                        }
                    }
                }

                map_x_.at<float>(row, col) = beam_idx;
                map_y_.at<float>(row, col) = range_idx;
            }
        }

        initialized_ = true;
    }

    /**
     * @brief Convert polar sonar image to Cartesian fan image
     * @param polar_image Input image in polar coordinates (beams x ranges)
     * @param apply_colormap Whether to apply a colormap to the output
     * @return Cartesian fan image
     */
    cv::Mat convertToCartesian(const cv::Mat& polar_image, bool apply_colormap = true) {
        if (!initialized_) {
            RCLCPP_ERROR(rclcpp::get_logger("polar_converter"),
                        "Mapping not initialized. Call generateMapping first!");
            return cv::Mat();
        }

        // Perform remapping - output size is rows_ x cols_ (height x width)
        cv::Mat cartesian_image;
        cv::remap(polar_image, cartesian_image, map_x_, map_y_, cv::INTER_LINEAR,
                  cv::BORDER_CONSTANT, cv::Scalar(0));

        // Apply colormap if requested
        if (apply_colormap && cartesian_image.channels() == 1) {
            cv::Mat colored;
            cv::applyColorMap(cartesian_image, colored, cv::COLORMAP_HOT);

            // Add range and model info
            cv::putText(colored,
                       cv::format("%s - Range: %.1fm", sonar_model_.c_str(), max_range_),
                       cv::Point(10, 30),
                       cv::FONT_HERSHEY_SIMPLEX,
                       0.8,
                       cv::Scalar(255, 255, 255),
                       2);

            return colored;
        }

        return cartesian_image;
    }

    /**
     * @brief Set pixels per meter for output resolution
     */
    void setPixelsPerMeter(float ppm) {
        if (std::abs(ppm - pixels_per_meter_) > 1e-6) {
            pixels_per_meter_ = ppm;
            initialized_ = false;  // Force regeneration of mapping
        }
    }

private:
    std::string sonar_model_;
    SonarSpecs specs_;
    int freq_mode_;
    float pixels_per_meter_;
    bool initialized_;

    // Output image dimensions
    int rows_;  // height (num_ranges)
    int cols_;  // width (calculated from bearing span)

    // Ping parameters
    int num_beams_;
    int num_ranges_;
    float range_resolution_;
    float max_range_;
    std::vector<float> bearings_;

    // Mapping matrices for OpenCV remap
    cv::Mat map_x_;
    cv::Mat map_y_;
};

}  // namespace oculus_sonar