/**
 * @file polar_to_cartesian.cpp
 * @brief Implementation of polar to Cartesian coordinate conversion
 * @date 260206
 *
 * Converts Oculus sonar polar images (range x bearing) to Cartesian fan images.
 * Supports M750d, M1200d, and M3000d sonar models.
 */

#include "oculus_sonar/polar_to_cartesian.hpp"
#include <cmath>
#include <algorithm>

namespace oculus_sonar {

//==============================================================================
// Constructor
//==============================================================================

PolarToCartesianConverter::PolarToCartesianConverter(
    const std::string& sonar_model, int freq_mode)
    : sonar_model_(sonar_model),
      freq_mode_(freq_mode),
      colormap_code_(cv::COLORMAP_TURBO),  // Default colormap
      initialized_(false),
      rows_(0), cols_(0),
      num_beams_(0), num_ranges_(0),
      range_resolution_(0.0f), max_range_(0.0f) {

    // Load model configuration
    if (SonarConfigRegistry::isModelSupported(sonar_model_)) {
        config_ = SonarConfigRegistry::getModelConfig(sonar_model_);
        current_specs_ = getFreqModeSpecs(config_, freq_mode_);
    } else {
        // Fallback to M750d if unknown model
        RCLCPP_WARN(rclcpp::get_logger("polar_converter"),
                   "Unknown model '%s', using m750d defaults", sonar_model_.c_str());
        config_ = SonarConfigRegistry::getModelConfig("m750d");
        current_specs_ = getFreqModeSpecs(config_, freq_mode_);
    }
}

//==============================================================================
// Public Methods
//==============================================================================

void PolarToCartesianConverter::updateMapping(
    int num_ranges, int num_beams, float max_range,
    const std::vector<float>& azimuth_angles) {

    // Skip if parameters unchanged (optimization)
    if (initialized_ &&
        num_beams == num_beams_ &&
        num_ranges == num_ranges_ &&
        std::abs(max_range - max_range_) < 1e-6) {
        return;
    }

    num_beams_ = num_beams;
    num_ranges_ = num_ranges;
    max_range_ = max_range;
    range_resolution_ = max_range / num_ranges;

    // Use actual bearings from sonar message (priority)
    // Fall back to config-based FOV only if azimuth_angles is empty
    bearings_.clear();
    bearings_.reserve(num_beams);

    if (!azimuth_angles.empty() &&
        static_cast<int>(azimuth_angles.size()) == num_beams) {
        // Use actual bearings from liboculus/SonarImage
        bearings_ = azimuth_angles;

        float actual_fov_deg = (bearings_.back() - bearings_.front()) * 180.0f / M_PI;
        RCLCPP_INFO(rclcpp::get_logger("polar_converter"),
                   "%s: Using actual bearings from sonar (%.1f° FOV, %d beams)",
                   sonar_model_.c_str(), actual_fov_deg, num_beams);
    } else {
        // Fallback: generate bearings from model specs
        float fov_rad = current_specs_.horizontal_fov_deg * M_PI / 180.0f;
        float beam_spacing_rad = fov_rad / (num_beams - 1);
        float start_bearing = -fov_rad / 2.0f;

        for (int i = 0; i < num_beams; ++i) {
            bearings_.push_back(start_bearing + i * beam_spacing_rad);
        }

        RCLCPP_INFO(rclcpp::get_logger("polar_converter"),
                   "%s: Using default FOV (%.1f°, %d beams) - no azimuth_angles provided",
                   sonar_model_.c_str(), current_specs_.horizontal_fov_deg, num_beams);
    }

    // Generate the Cartesian mapping
    generateCartesianMapping();

    RCLCPP_INFO(rclcpp::get_logger("polar_converter"),
               "%s: %dx%d polar -> %dx%d cartesian (%.1fm range)",
               sonar_model_.c_str(), num_beams_, num_ranges_,
               cols_, rows_, max_range_);
}

cv::Mat PolarToCartesianConverter::convertToCartesian(
    const cv::Mat& polar_image, bool apply_colormap) {

    if (!initialized_) {
        RCLCPP_ERROR(rclcpp::get_logger("polar_converter"),
                    "Mapping not initialized! Call updateMapping first.");
        return cv::Mat();
    }

    // Perform remapping using precomputed lookup tables
    cv::Mat cartesian_image;
    cv::remap(polar_image, cartesian_image, map_x_, map_y_,
              cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0));

    // Apply colormap if requested
    if (apply_colormap && cartesian_image.channels() == 1) {
        cv::Mat colored;
        cv::applyColorMap(cartesian_image, colored, colormap_code_);
        return colored;
    }

    return cartesian_image;
}

void PolarToCartesianConverter::setFrequencyMode(int freq_mode) {
    if (freq_mode_ != freq_mode) {
        freq_mode_ = freq_mode;
        current_specs_ = getFreqModeSpecs(config_, freq_mode_);
        initialized_ = false;  // Force regeneration

        RCLCPP_INFO(rclcpp::get_logger("polar_converter"),
                   "Frequency mode changed to %d (%.1fMHz, %.1f° FOV)",
                   freq_mode_, current_specs_.frequency_mhz,
                   current_specs_.horizontal_fov_deg);
    }
}

void PolarToCartesianConverter::setSonarModel(const std::string& model) {
    if (sonar_model_ != model) {
        sonar_model_ = model;

        if (SonarConfigRegistry::isModelSupported(model)) {
            config_ = SonarConfigRegistry::getModelConfig(model);
            current_specs_ = getFreqModeSpecs(config_, freq_mode_);
        } else {
            RCLCPP_WARN(rclcpp::get_logger("polar_converter"),
                       "Unknown model '%s', keeping current settings", model.c_str());
            return;
        }

        initialized_ = false;  // Force regeneration

        RCLCPP_INFO(rclcpp::get_logger("polar_converter"),
                   "Sonar model changed to %s", model.c_str());
    }
}

void PolarToCartesianConverter::setColormap(const std::string& colormap_name) {
    if (SonarConfigRegistry::isColormapSupported(colormap_name)) {
        colormap_code_ = SonarConfigRegistry::getColormapConfig(colormap_name).opencv_code;
        RCLCPP_INFO(rclcpp::get_logger("polar_converter"),
                   "Colormap changed to '%s' (code: %d)", colormap_name.c_str(), colormap_code_);
    } else {
        RCLCPP_WARN(rclcpp::get_logger("polar_converter"),
                   "Unknown colormap '%s', keeping current", colormap_name.c_str());
    }
}

void PolarToCartesianConverter::setColormapCode(int code) {
    colormap_code_ = code;
}

//==============================================================================
// Private Methods
//==============================================================================

void PolarToCartesianConverter::generateCartesianMapping() {
    // IMPORTANT: Cartesian image height = polar image height (num_ranges)
    // This maintains 1:1 range resolution mapping
    rows_ = num_ranges_;

    // Calculate width based on FOV
    float bearing_span = bearings_.back() - bearings_.front();  // in radians
    float half_angle = std::abs(bearing_span) / 2.0f;

    // Width calculation: arc width at max range
    // width = 2 * max_range * sin(half_angle)
    // Scale to match height (num_ranges corresponds to max_range)
    float arc_width_ratio = 2.0f * std::sin(half_angle);
    cols_ = static_cast<int>(std::ceil(rows_ * arc_width_ratio));

    // Ensure minimum width
    if (cols_ < 1) cols_ = 1;

    // Allocate mapping matrices
    map_x_ = cv::Mat(rows_, cols_, CV_32F);
    map_y_ = cv::Mat(rows_, cols_, CV_32F);

    // Pixel size in range units (max_range corresponds to rows_ pixels)
    float pixel_size = max_range_ / static_cast<float>(rows_);

    // Generate reverse mapping: for each output pixel, find source pixel
    for (int row = 0; row < rows_; ++row) {
        for (int col = 0; col < cols_; ++col) {
            // Convert pixel position to physical coordinates
            // Origin at bottom-center of image
            // X: forward (range direction), Y: lateral
            float x = pixel_size * (rows_ - row);
            float y = pixel_size * (-cols_ / 2.0f + col + 0.5f);

            // Convert to polar coordinates
            float bearing = std::atan2(y, x) * SonarConstants::REVERSE_Z;
            float range = std::sqrt(x * x + y * y);

            // Convert to source image indices
            float range_idx = range / range_resolution_;
            float beam_idx = interpolateBearing(bearing);

            map_x_.at<float>(row, col) = beam_idx;
            map_y_.at<float>(row, col) = range_idx;
        }
    }

    initialized_ = true;
}

float PolarToCartesianConverter::interpolateBearing(float bearing) const {
    // Out of bounds check
    if (bearings_.empty() ||
        bearing < bearings_.front() ||
        bearing > bearings_.back()) {
        return -1.0f;
    }

    // Binary search for efficiency (bearings are sorted)
    auto it = std::lower_bound(bearings_.begin(), bearings_.end(), bearing);

    if (it == bearings_.begin()) {
        return 0.0f;
    }
    if (it == bearings_.end()) {
        return static_cast<float>(bearings_.size() - 1);
    }

    // Linear interpolation between adjacent bearings
    size_t idx = std::distance(bearings_.begin(), it);
    float denominator = bearings_[idx] - bearings_[idx - 1];

    // Avoid division by zero
    if (std::abs(denominator) < 1e-9f) {
        return static_cast<float>(idx - 1);
    }

    float t = (bearing - bearings_[idx - 1]) / denominator;
    return static_cast<float>(idx - 1) + t;
}

}  // namespace oculus_sonar
