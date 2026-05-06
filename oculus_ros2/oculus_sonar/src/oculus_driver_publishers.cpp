/**
 * @file oculus_driver_publishers.cpp
 * @brief Implementation of OculusDriverPublishers.
 */

#include "oculus_sonar/oculus_driver_publishers.hpp"

namespace oculus_sonar {

OculusDriverPublishers::OculusDriverPublishers(rclcpp::Node* node,
                                               const std::string& topic_prefix,
                                               const std::string& frame_id)
    : node_(node), topic_prefix_(topic_prefix), frame_id_(frame_id) {}

void OculusDriverPublishers::initialize() {
  auto sensor_qos = rclcpp::SensorDataQoS();
  auto latched_qos = rclcpp::QoS(1).transient_local();

  imaging_sonar_pub_ = node_->create_publisher<marine_acoustic_msgs::msg::SonarImage>(
      topic_prefix_ + "/sonar", sensor_qos);
  oculus_meta_pub_ = node_->create_publisher<oculus_sonar_msgs::msg::OculusMetadata>(
      topic_prefix_ + "/metadata", sensor_qos);
  raw_data_pub_ = node_->create_publisher<apl_msgs::msg::RawData>(
      topic_prefix_ + "/raw_data", sensor_qos);
  image_pub_ = image_transport::create_publisher(
      node_, topic_prefix_ + "/image", rmw_qos_profile_sensor_data);

  ping_rate_pub_ = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/ping_rate", latched_qos);
  freq_mode_pub_ = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/freq_mode", latched_qos);
  data_size_pub_ = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/data_size", latched_qos);
  range_pub_ = node_->create_publisher<std_msgs::msg::Float32>(
      topic_prefix_ + "/param/range", latched_qos);
  gain_pub_ = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/gain", latched_qos);
  gamma_pub_ = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/gamma", latched_qos);
  ip_address_pub_ = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/ip_address", latched_qos);
  frame_id_pub_ = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/frame_id", latched_qos);
  sonar_model_pub_ = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/sonar_model", latched_qos);

  RCLCPP_INFO(node_->get_logger(), "Publishing data with frame = %s", frame_id_.c_str());

  // Seed the TRANSIENT_LOCAL durability cache so subscribers connecting
  // before the first timer tick still receive a value via late-join replay.
  publishParameters();

  param_publish_timer_ = rclcpp::create_timer(
      node_, node_->get_clock(),
      rclcpp::Duration::from_seconds(1.0),
      [this]() { publishParameters(); });
}

void OculusDriverPublishers::republishParameters() {
  publishParameters();
}

void OculusDriverPublishers::publishParameters() {
  std_msgs::msg::Int32 ping_rate_msg;
  ping_rate_msg.data = node_->get_parameter("ping_rate").as_int();
  ping_rate_pub_->publish(ping_rate_msg);

  std_msgs::msg::Int32 freq_mode_msg;
  freq_mode_msg.data = node_->get_parameter("freq_mode").as_int();
  freq_mode_pub_->publish(freq_mode_msg);

  std_msgs::msg::String data_size_msg;
  data_size_msg.data = node_->get_parameter("data_size").as_string();
  data_size_pub_->publish(data_size_msg);

  std_msgs::msg::Float32 range_msg;
  range_msg.data = static_cast<float>(node_->get_parameter("range").as_double());
  range_pub_->publish(range_msg);

  std_msgs::msg::Int32 gain_msg;
  gain_msg.data = node_->get_parameter("gain").as_int();
  gain_pub_->publish(gain_msg);

  std_msgs::msg::Int32 gamma_msg;
  gamma_msg.data = node_->get_parameter("gamma").as_int();
  gamma_pub_->publish(gamma_msg);

  std_msgs::msg::String ip_msg;
  ip_msg.data = node_->get_parameter("ip_address").as_string();
  ip_address_pub_->publish(ip_msg);

  std_msgs::msg::String frame_id_msg;
  frame_id_msg.data = node_->get_parameter("frame_id").as_string();
  frame_id_pub_->publish(frame_id_msg);

  std_msgs::msg::String sonar_model_msg;
  sonar_model_msg.data = node_->get_parameter("sonar_model").as_string();
  sonar_model_pub_->publish(sonar_model_msg);
}

sensor_msgs::msg::Image OculusDriverPublishers::sonarToImage(
    const marine_acoustic_msgs::msg::SonarImage& sonar_msg) const {
  sensor_msgs::msg::Image image_msg;

  image_msg.header = sonar_msg.header;
  image_msg.height = sonar_msg.ranges.size();
  image_msg.width = sonar_msg.azimuth_angles.size();

  if (sonar_msg.data_size == 1) {
    image_msg.encoding = "mono8";
  } else if (sonar_msg.data_size == 2) {
    image_msg.encoding = "mono16";
  } else if (sonar_msg.data_size == 4) {
    image_msg.encoding = "32FC1";
  }

  image_msg.is_bigendian = sonar_msg.is_bigendian;
  image_msg.step = image_msg.width * sonar_msg.data_size;
  image_msg.data = sonar_msg.intensities;

  return image_msg;
}

void OculusDriverPublishers::recordLatencyAndLog(double latency_ms) {
  if (latency_window_.size() < kLatencyWindow) {
    latency_window_.push_back(latency_ms);
  } else {
    latency_window_[latency_write_idx_] = latency_ms;
    latency_write_idx_ = (latency_write_idx_ + 1) % kLatencyWindow;
  }
  ++total_pings_;

  std::vector<double> sorted = latency_window_;
  std::sort(sorted.begin(), sorted.end());
  const size_t n = sorted.size();
  const double p50 = sorted[n / 2];
  const double p99 = sorted[(n * 99) / 100];

  RCLCPP_INFO_THROTTLE(
      node_->get_logger(), *node_->get_clock(), 1000,
      "[c1-perf] pings=%lu  window=%zu  p50=%.3fms  p99=%.3fms",
      static_cast<unsigned long>(total_pings_), n, p50, p99);
}

}  // namespace oculus_sonar
