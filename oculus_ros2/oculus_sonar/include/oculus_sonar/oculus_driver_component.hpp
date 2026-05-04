/**
 * @file oculus_driver_component.hpp
 * @date 250721
 * @author seungmin kim
 * @brief ROS2 Oculus Sonar Driver main component header.
 *
 * This file defines the OculusDriver class, which implements the main ROS2 component for interfacing with the Oculus sonar device.
 */

#pragma once

#include <cstdlib>
#include <sstream>
#include <string>
#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_components/register_node_macro.hpp>
#include <std_msgs/msg/string.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <std_msgs/msg/int32.hpp>
#include <std_msgs/msg/float32.hpp>
#include <std_msgs/msg/string.hpp>
#include <image_transport/image_transport.hpp>

#include "liboculus/SimplePingResult.h"
#include "liboculus/StatusRx.h"
#include "liboculus/IoServiceThread.h"
#include "liboculus/SonarConfiguration.h"

#include "oculus_sonar/publishing_data_rx.h"
#include "oculus_sonar/ping_to_sonar_image.hpp"
#include "oculus_sonar/oculus_driver_config.hpp"
#include "oculus_sonar/oculus_driver_publishers.hpp"
#include "marine_acoustic_msgs/msg/sonar_image.hpp"
#include "oculus_sonar_msgs/msg/oculus_metadata.hpp"
#include "apl_msgs/msg/raw_data.hpp"

namespace oculus_sonar {

/**
 * @class OculusDriver
 * @brief Main driver class for Oculus sonar.
 */
class OculusDriver : public rclcpp::Node {
 public:
  /**
   * @brief Constructor.
   * @param options Node options.
   */
  explicit OculusDriver(const rclcpp::NodeOptions & options);

  /**
   * @brief Destructor.
   */
  virtual ~OculusDriver();

  /**
   * @brief Callback for sonar ping data.
   * @tparam Ping_t Ping data type.
   * @param ping Ping data.
   */
  template <typename Ping_t>
  void pingCallback(const Ping_t &ping) {
    publishers_->publishPing(ping);
  }

 private:
  /**
   * @brief Initialize all ROS interfaces and start the sonar client.
   */
  void init();

  liboculus::IoServiceThread io_srv_;
  std::unique_ptr<PublishingDataRx> data_rx_;
  liboculus::StatusRx status_rx_;

  std::unique_ptr<OculusDriverConfig> config_;
  std::unique_ptr<OculusDriverPublishers> publishers_;
};

}  // namespace oculus_sonar