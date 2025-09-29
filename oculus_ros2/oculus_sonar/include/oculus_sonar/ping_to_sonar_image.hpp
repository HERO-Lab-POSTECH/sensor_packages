/**
 * @file ping_to_sonar_image.hpp
 * @date 250721
 * @author seungmin kim
 * @brief Utility for converting Oculus sonar ping data to SonarImage message.
 */

#pragma once

#include <rclcpp/rclcpp.hpp>
#include "marine_acoustic_msgs/msg/sonar_image.hpp"
#include "liboculus/SimplePingResult.h"
#include "liboculus/Constants.h"

namespace oculus_sonar {

/**
 * @brief Convert Oculus sonar ping data to marine_acoustic_msgs::msg::SonarImage.
 * @tparam PingT Type of ping data.
 * @param ping Input ping data.
 * @return SonarImage message.
 */
template<typename PingT>
marine_acoustic_msgs::msg::SonarImage pingToSonarImage(
    const liboculus::SimplePingResult<PingT> &ping) {
  marine_acoustic_msgs::msg::SonarImage sonar_image;
  sonar_image.frequency = ping.ping()->frequency;

  if (sonar_image.frequency > 2000000) {
    sonar_image.azimuth_beamwidth = liboculus::Oculus_2100MHz::AzimuthBeamwidthRad;
    sonar_image.elevation_beamwidth = liboculus::Oculus_2100MHz::ElevationBeamwidthRad;
  } else if ((sonar_image.frequency > 1100000) && (sonar_image.frequency < 1300000)) {
    sonar_image.azimuth_beamwidth = liboculus::Oculus_1200MHz::AzimuthBeamwidthRad;
    sonar_image.elevation_beamwidth = liboculus::Oculus_1200MHz::ElevationBeamwidthRad;
  } else {
    RCLCPP_ERROR_STREAM(rclcpp::get_logger("oculus_sonar"),
                        "Unsupported frequency received from oculus: "
                        << sonar_image.frequency << ". Not publishing SonarImage");
  }

  const int num_bearings = ping.ping()->nBeams;
  const int num_ranges = ping.ping()->nRanges;
  const int bearing_count = ping.bearings().size();

  RCLCPP_INFO(rclcpp::get_logger("ping_to_sonar_image"),
              "num_bearings: %d, bearing_count: %d, num_ranges: %d, range_resolution: %.4fm, max_range: %.2fm",
              num_bearings, bearing_count, num_ranges, ping.ping()->rangeResolution,
              num_ranges * ping.ping()->rangeResolution);

  sonar_image.azimuth_angles.resize(num_bearings);

  // If we have 512 beams but only 256 bearings, interpolate
  if (num_bearings == 512 && bearing_count <= 256) {
    RCLCPP_INFO(rclcpp::get_logger("ping_to_sonar_image"),
                "Interpolating bearings: 512 beams from %d bearings", bearing_count);

    // Debug: Check actual bearing range from hardware
    if (bearing_count > 0) {
      float first_deg = ping.bearings().at_rad(0) * 180.0f / M_PI;
      float last_deg = ping.bearings().at_rad(bearing_count-1) * 180.0f / M_PI;
      float hardware_fov = (ping.bearings().at_rad(bearing_count-1) - ping.bearings().at_rad(0)) * 180.0f / M_PI;
      RCLCPP_WARN_ONCE(rclcpp::get_logger("ping_to_sonar_image"),
                       "Hardware bearings: first=%.2f°, last=%.2f°, FOV=%.2f° (but sensor spec is 130°!)",
                       first_deg, last_deg, hardware_fov);
    }
    // Oculus sends 256 bearings for 512 beam mode, need to interpolate
    for (unsigned int b = 0; b < num_bearings; b++) {
      if (b % 2 == 0) {
        // Even indices: use actual bearing data
        int bearing_idx = b / 2;
        if (bearing_idx < bearing_count) {
          sonar_image.azimuth_angles[b] = ping.bearings().at_rad(bearing_idx);
        } else {
          // Out of bounds, use last bearing
          sonar_image.azimuth_angles[b] = ping.bearings().at_rad(bearing_count - 1);
        }
      } else {
        // Odd indices: interpolate between adjacent bearings
        int idx = b / 2;
        if (idx < bearing_count - 1) {
          float bearing1 = ping.bearings().at_rad(idx);
          float bearing2 = ping.bearings().at_rad(idx + 1);
          sonar_image.azimuth_angles[b] = (bearing1 + bearing2) / 2.0f;
        } else {
          // Last beam, use last bearing
          sonar_image.azimuth_angles[b] = ping.bearings().at_rad(bearing_count - 1);
        }
      }
    }
  } else {
    // Normal case: one bearing per beam
    int max_bearings = std::min(num_bearings, bearing_count);
    for (int b = 0; b < max_bearings; b++) {
      sonar_image.azimuth_angles[b] = ping.bearings().at_rad(b);
    }

    // Debug: Print first and last bearing angles
    if (max_bearings > 0) {
      float first_bearing_deg = ping.bearings().at_rad(0) * 180.0f / M_PI;
      float last_bearing_deg = ping.bearings().at_rad(max_bearings-1) * 180.0f / M_PI;
      float fov_deg = (ping.bearings().at_rad(max_bearings-1) - ping.bearings().at_rad(0)) * 180.0f / M_PI;
      RCLCPP_INFO_ONCE(rclcpp::get_logger("ping_to_sonar_image"),
                       "Bearings from hardware: first=%.2f°, last=%.2f°, FOV=%.2f° (expected 130°)",
                       first_bearing_deg, last_bearing_deg, fov_deg);
    }
    // If num_bearings > bearing_count, fill remaining with last bearing
    if (num_bearings > bearing_count && bearing_count > 0) {
      float last_bearing = ping.bearings().at_rad(bearing_count - 1);
      for (int b = bearing_count; b < num_bearings; b++) {
        sonar_image.azimuth_angles[b] = last_bearing;
      }
      RCLCPP_WARN(rclcpp::get_logger("ping_to_sonar_image"),
                  "num_bearings (%d) > bearing_count (%d), filling with last bearing",
                  num_bearings, bearing_count);
    }
  }

  sonar_image.ranges.resize(num_ranges);
  for (unsigned int i = 0; i < num_ranges; i++) {
    sonar_image.ranges[i] = static_cast<float>(i+0.5) * ping.ping()->rangeResolution;
  }

  sonar_image.is_bigendian = false;
  sonar_image.data_size = ping.dataSize();

  for (unsigned int r = 0; r < num_ranges; r++) {
    for (unsigned int b = 0; b < num_bearings; b++) {
      if (ping.dataSize() == 1) {
        const uint8_t data = ping.image().at_uint8(b, r);
        sonar_image.intensities.push_back(data & 0xFF);
      } else if (ping.dataSize() == 2) {
        const uint16_t data = ping.image().at_uint16(b, r);
        sonar_image.intensities.push_back(data & 0xFF);
        sonar_image.intensities.push_back((data & 0xFF00) >> 8);
      } else if (ping.dataSize() == 4) {
        const uint32_t data = ping.image().at_uint32(b, r);
        sonar_image.intensities.push_back(data & 0x000000FF);
        sonar_image.intensities.push_back((data & 0x0000FF00) >> 8);
        sonar_image.intensities.push_back((data & 0x00FF0000) >> 16);
        sonar_image.intensities.push_back((data & 0xFF000000) >> 24);
      }
    }
  }

  return sonar_image;
}

}  // namespace oculus_sonar