/**
 * @file oculus_driver_publishers.hpp
 * @brief Owns the 4 sensor publishers + 9 parameter-echo publishers,
 *        and the templated publishPing call site.
 */
#pragma once

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <apl_msgs/msg/raw_data.hpp>
#include <image_transport/image_transport.hpp>
#include <marine_acoustic_msgs/msg/sonar_image.hpp>
#include <oculus_sonar_msgs/msg/oculus_metadata.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <std_msgs/msg/float32.hpp>
#include <std_msgs/msg/int32.hpp>
#include <std_msgs/msg/string.hpp>

#include "oculus_sonar/ping_to_sonar_image.hpp"

namespace oculus_sonar {

class OculusDriverPublishers {
 public:
  OculusDriverPublishers(rclcpp::Node* node,
                         const std::string& topic_prefix,
                         const std::string& frame_id);
  ~OculusDriverPublishers() = default;

  OculusDriverPublishers(const OculusDriverPublishers&) = delete;
  OculusDriverPublishers& operator=(const OculusDriverPublishers&) = delete;

  // Creates all publishers; must be called before publishPing.
  void initialize();

  // Returns the raw_data publisher so the connection layer can hand it
  // to PublishingDataRx::setRawPublisher.
  rclcpp::Publisher<apl_msgs::msg::RawData>::SharedPtr raw_data_publisher() const {
    return raw_data_pub_;
  }

  // Templated entry: publishes sonar/image/metadata + parameter echoes.
  // Phase C-1: chrono-based latency instrumentation around the conversion +
  // publish path. Sliding window of last 100 samples; logs p50/p99 every 1s.
  template <typename PingT>
  void publishPing(const PingT& ping) {
    const auto t_start = std::chrono::steady_clock::now();

    auto sonar_msg = pingToSonarImage(ping);
    sonar_msg.header.stamp = node_->get_clock()->now();
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

    publishParameters();

    const auto t_end = std::chrono::steady_clock::now();
    const double latency_ms =
        std::chrono::duration<double, std::milli>(t_end - t_start).count();
    recordLatencyAndLog(latency_ms);
  }

  // Publishes all 9 parameter echoes once. Called by:
  //   1) wall_timer (steady-state ~1 Hz),
  //   2) on_change listener after a parameter update (immediate latch update),
  //   3) initialize() (latched seed for late-join subscribers).
  void republishParameters();

 private:
  void publishParameters();
  sensor_msgs::msg::Image sonarToImage(
      const marine_acoustic_msgs::msg::SonarImage& sonar_msg) const;

  // Phase C-1: ping callback latency stats (sliding window p50/p99).
  void recordLatencyAndLog(double latency_ms);

  static constexpr size_t kLatencyWindow = 100;
  std::vector<double> latency_window_;     // sliding window, last N samples
  size_t latency_write_idx_ = 0;           // ring index
  uint64_t total_pings_ = 0;               // monotonic ping count

  rclcpp::Node* node_;  // non-owning
  std::string topic_prefix_;
  std::string frame_id_;

  rclcpp::Publisher<marine_acoustic_msgs::msg::SonarImage>::SharedPtr imaging_sonar_pub_;
  rclcpp::Publisher<oculus_sonar_msgs::msg::OculusMetadata>::SharedPtr oculus_meta_pub_;
  rclcpp::Publisher<apl_msgs::msg::RawData>::SharedPtr raw_data_pub_;
  image_transport::Publisher image_pub_;

  rclcpp::TimerBase::SharedPtr param_publish_timer_;

  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr ping_rate_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr freq_mode_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr data_size_pub_;
  rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr range_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr gain_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr gamma_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr ip_address_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr frame_id_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr sonar_model_pub_;
};

}  // namespace oculus_sonar
