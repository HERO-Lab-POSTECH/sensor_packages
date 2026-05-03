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
    auto sonar_msg = pingToSonarImage(ping);
    sonar_msg.header.stamp = this->get_clock()->now();
    sonar_msg.header.frame_id = frame_id_;
    imaging_sonar_pub_->publish(sonar_msg);
    auto image_msg = sonarToImage(sonar_msg);
    image_pub_.publish(image_msg);

    oculus_sonar_msgs::msg::OculusMetadata meta;
    meta.header = sonar_msg.header;
    for (unsigned int i = 0; i < ping.gains().size(); i++) {
      meta.tvg.push_back(ping.gains().at(i));
    }
    oculus_meta_pub_->publish(meta);
    // Publish parameters for recording
    publishParameters();
  }

 private:
  /**
   * @brief Initialize all ROS interfaces and start the sonar client.
   */
  void init();

  /**
   * @brief Parameter callback for dynamic reconfigure equivalent.
   * @param parameters Vector of parameters to set.
   * @return SetParametersResult indicating success or failure.
   */
  rcl_interfaces::msg::SetParametersResult parameterCallback(
    const std::vector<rclcpp::Parameter> & parameters);

  /**
   * @brief Set the sonar ping rate.
   * @param ping_rate Ping rate value.
   */
  void setPingRate(int ping_rate);

  /**
   * @brief Set the sonar frequency mode.
   * @param freq_mode Frequency mode value.
   */
  void setFreqMode(int freq_mode);

  /**
   * @brief Set the sonar data size.
   * @param data_size Data size value.
   */
  void setDataSize(int data_size);

  /**
   * @brief Update the sonar configuration with current parameters.
   */
  void updateSonarConfig();

  /**
   * @brief Convert data size string to integer value.
   * @param data_size_str Data size as string ("8bit", "16bit", "32bit").
   * @return Corresponding integer value (1, 2, 4).
   */
  int convertDataSizeString(const std::string& data_size_str);

  /**
   * @brief Convert SonarImage to standard Image for rviz2 visualization.
   * @param sonar_msg SonarImage message.
   * @return sensor_msgs::msg::Image for visualization.
   */
  sensor_msgs::msg::Image sonarToImage(const marine_acoustic_msgs::msg::SonarImage& sonar_msg);

  liboculus::IoServiceThread io_srv_;
  std::unique_ptr<PublishingDataRx> data_rx_;
  liboculus::StatusRx status_rx_;

  rclcpp::Publisher<marine_acoustic_msgs::msg::SonarImage>::SharedPtr imaging_sonar_pub_;
  rclcpp::Publisher<oculus_sonar_msgs::msg::OculusMetadata>::SharedPtr oculus_meta_pub_;
  rclcpp::Publisher<apl_msgs::msg::RawData>::SharedPtr raw_data_pub_;
  image_transport::Publisher image_pub_;
  
  // Parameter publishers for recording
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr ping_rate_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr freq_mode_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr data_size_pub_;
  rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr range_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr gain_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr gamma_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr ip_address_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr frame_id_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr sonar_model_pub_;
  void publishParameters();

  std::string ip_address_;
  std::string frame_id_;
  std::string sonar_model_;

  liboculus::SonarConfiguration sonar_config_;

  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_callback_handle_;
};

}  // namespace oculus_sonar