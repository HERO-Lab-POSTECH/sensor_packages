// Workspace-standard QoS profiles (spec §2.4)
#pragma once

#include <rclcpp/qos.hpp>

namespace pkrc_qos {

inline rclcpp::QoS sensor_qos() {
  return rclcpp::QoS(rclcpp::KeepLast(5)).best_effort();
}

inline rclcpp::QoS reliable_qos() {
  return rclcpp::QoS(rclcpp::KeepLast(10)).reliable();
}

inline rclcpp::QoS latched_qos() {
  return rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
}

}  // namespace pkrc_qos
