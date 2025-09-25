/**
 * @file oculus_fan_imager.hpp
 * @date 251209
 * @brief ROS2 Oculus Fan Image Converter Node
 *
 * This node subscribes to Oculus sonar polar images and converts them to 
 * Cartesian (fan) images for better visualization.
 */

#pragma once

#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <marine_acoustic_msgs/msg/sonar_image.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>

#include "oculus_sonar/polar_to_cartesian.hpp"

namespace oculus_sonar {

/**
 * @class OculusFanImager
 * @brief Node for converting Oculus sonar polar images to Cartesian fan images
 */
class OculusFanImager : public rclcpp::Node {
public:
  /**
   * @brief Constructor
   * @param options Node options
   */
  explicit OculusFanImager(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());

  /**
   * @brief Destructor
   */
  virtual ~OculusFanImager() = default;

private:
  /**
   * @brief Callback for incoming sonar images
   * @param msg Incoming sonar image message with full metadata
   */
  void sonarImageCallback(const marine_acoustic_msgs::msg::SonarImage::SharedPtr msg);

  /**
   * @brief Initialize the node parameters and interfaces
   */
  void init();

  /**
   * @brief Parameter callback for dynamic reconfigure
   * @param parameters Vector of parameters to set
   * @return SetParametersResult indicating success or failure
   */
  rcl_interfaces::msg::SetParametersResult parameterCallback(
    const std::vector<rclcpp::Parameter>& parameters);

  // Subscribers and Publishers
  rclcpp::Subscription<marine_acoustic_msgs::msg::SonarImage>::SharedPtr sonar_sub_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr fan_image_pub_;

  // Parameters
  std::string input_topic_;
  std::string output_topic_;
  std::string sonar_model_;
  int freq_mode_;
  bool apply_colormap_;
  int colormap_type_;

  // Polar to Cartesian converter
  std::unique_ptr<PolarToCartesianConverter> polar_converter_;

  // OpenCV bridge
  cv_bridge::CvImagePtr cv_ptr_;

  // Parameter callback handle
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_callback_handle_;

  // Performance tracking
  rclcpp::Time last_image_time_;
  int frame_count_;
  rclcpp::TimerBase::SharedPtr stats_timer_;
  void printStatistics();
};

} // namespace oculus_sonar