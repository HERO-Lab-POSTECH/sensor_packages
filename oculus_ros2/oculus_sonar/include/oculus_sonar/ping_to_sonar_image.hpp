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
              "num_bearings: %d, bearing_count: %d", num_bearings, bearing_count);

  sonar_image.azimuth_angles.resize(num_bearings);

  // If we have 512 beams but only 256 bearings, interpolate
  if (num_bearings == 512 && bearing_count <= 258) {
    RCLCPP_INFO(rclcpp::get_logger("ping_to_sonar_image"),
                "Interpolating bearings: 512 beams from %d bearings", bearing_count);
    // Oculus sends 256 bearings for 512 beam mode, need to interpolate
    for (unsigned int b = 0; b < num_bearings; b++) {
      if (b % 2 == 0) {
        // Even indices: use actual bearing data
        sonar_image.azimuth_angles[b] = ping.bearings().at_rad(b / 2);
      } else {
        // Odd indices: interpolate between adjacent bearings
        int idx = b / 2;
        if (idx < bearing_count - 1) {
          float bearing1 = ping.bearings().at_rad(idx);
          float bearing2 = ping.bearings().at_rad(idx + 1);
          sonar_image.azimuth_angles[b] = (bearing1 + bearing2) / 2.0f;
        } else {
          // Last beam, extrapolate
          sonar_image.azimuth_angles[b] = ping.bearings().at_rad(bearing_count - 1);
        }
      }
    }
  } else {
    // Normal case: one bearing per beam
    for (unsigned int b = 0; b < num_bearings && b < bearing_count; b++) {
      sonar_image.azimuth_angles[b] = ping.bearings().at_rad(b);
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