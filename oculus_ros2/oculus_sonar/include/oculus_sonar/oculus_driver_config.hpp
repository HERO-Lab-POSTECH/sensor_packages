/**
 * @file oculus_driver_config.hpp
 * @brief Independent helper that owns Oculus sonar parameter declaration,
 *        dynamic-reconfigure callback, and SonarConfiguration synthesis.
 *
 * This class is intentionally NOT an rclcpp::Node subclass. It takes a
 * non-owning Node* whose lifetime is guaranteed by the orchestrator
 * (OculusDriver). Splitting this concern out makes parameter logic unit
 * testable with a mock node and keeps OculusDriver focused on wiring.
 */
#pragma once

#include <functional>
#include <string>
#include <vector>

#include <rcl_interfaces/msg/set_parameters_result.hpp>
#include <rclcpp/rclcpp.hpp>

#include "liboculus/SonarConfiguration.h"

namespace oculus_sonar {

class OculusDriverConfig {
 public:
  using OnChangeCallback = std::function<void(const liboculus::SonarConfiguration&)>;

  explicit OculusDriverConfig(rclcpp::Node* node);
  ~OculusDriverConfig() = default;

  OculusDriverConfig(const OculusDriverConfig&) = delete;
  OculusDriverConfig& operator=(const OculusDriverConfig&) = delete;

  // Declares all parameters, reads initial values, registers parameter callback.
  // Idempotent against accidental double calls in same process is NOT guaranteed —
  // declare_parameter throws if already declared.
  void initialize();

  // Snapshot of current SonarConfiguration; valid while *this is alive.
  const liboculus::SonarConfiguration& current() const { return sonar_config_; }

  // Register a listener invoked AFTER each successful parameter update.
  // Multiple listeners allowed; called in registration order.
  void on_change(OnChangeCallback cb) { listeners_.push_back(std::move(cb)); }

  // String accessors used by the orchestrator and other helpers.
  const std::string& ip_address() const { return ip_address_; }
  const std::string& frame_id() const { return frame_id_; }
  const std::string& sonar_model() const { return sonar_model_; }

 private:
  rcl_interfaces::msg::SetParametersResult parameterCallback(
      const std::vector<rclcpp::Parameter>& parameters);

  void updateSonarConfig();
  void setPingRate(int ping_rate);
  void setFreqMode(int freq_mode);
  void setDataSize(int data_size);
  int convertDataSizeString(const std::string& data_size_str);

  rclcpp::Node* node_;  // non-owning
  liboculus::SonarConfiguration sonar_config_;
  std::string ip_address_;
  std::string frame_id_;
  std::string sonar_model_;
  std::vector<OnChangeCallback> listeners_;
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_callback_handle_;
};

}  // namespace oculus_sonar
