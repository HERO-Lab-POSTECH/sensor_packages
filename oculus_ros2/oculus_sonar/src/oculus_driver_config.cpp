/**
 * @file oculus_driver_config.cpp
 * @brief Implementation of OculusDriverConfig — see header for design notes.
 */

#include "oculus_sonar/oculus_driver_config.hpp"

#include "liboculus/Constants.h"
#include "liboculus/DataTypes.h"
#include "oculus_sonar/sonar_config.hpp"

namespace oculus_sonar {

OculusDriverConfig::OculusDriverConfig(rclcpp::Node* node) : node_(node) {}

void OculusDriverConfig::initialize() {
  node_->declare_parameter<std::string>("ip_address", "auto");
  node_->declare_parameter<std::string>("frame_id", "oculus");
  node_->declare_parameter<std::string>("sonar_model", "m750d");
  node_->declare_parameter<double>("range", SonarConstants::DEFAULT_RANGE_M);
  node_->declare_parameter<int>("gain", SonarConstants::DEFAULT_GAIN_PERCENT);
  node_->declare_parameter<int>("gamma", SonarConstants::DEFAULT_GAMMA);
  node_->declare_parameter<int>("ping_rate", 3);
  node_->declare_parameter<int>("freq_mode", 2);
  node_->declare_parameter<bool>("send_gain", true);
  node_->declare_parameter<std::string>("data_size", "8bit");
  node_->declare_parameter<bool>("send_range_as_meters", true);
  node_->declare_parameter<bool>("send_simple_return", true);
  node_->declare_parameter<bool>("gain_assistance", false);

  ip_address_ = node_->get_parameter("ip_address").as_string();
  frame_id_ = node_->get_parameter("frame_id").as_string();
  sonar_model_ = node_->get_parameter("sonar_model").as_string();

  updateSonarConfig();

  param_callback_handle_ = node_->add_on_set_parameters_callback(
      std::bind(&OculusDriverConfig::parameterCallback, this, std::placeholders::_1));
}

rcl_interfaces::msg::SetParametersResult OculusDriverConfig::parameterCallback(
    const std::vector<rclcpp::Parameter>& parameters) {
  rcl_interfaces::msg::SetParametersResult result;
  result.successful = true;

  for (const auto& param : parameters) {
    if (param.get_name() == "range") {
      double range = param.as_double();
      RCLCPP_INFO(node_->get_logger(), "Setting sonar range to %.1f m", range);
      sonar_config_.setRange(range);
    } else if (param.get_name() == "gain") {
      int gain = param.as_int();
      RCLCPP_INFO(node_->get_logger(), "Setting gain to %d pct", gain);
      sonar_config_.setGainPercent(gain);
    } else if (param.get_name() == "gamma") {
      int gamma = param.as_int();
      RCLCPP_INFO(node_->get_logger(), "Setting gamma to %d", gamma);
      sonar_config_.setGamma(gamma);
    } else if (param.get_name() == "ping_rate") {
      int ping_rate = param.as_int();
      RCLCPP_INFO(node_->get_logger(), "Setting ping rate to (%d): %.1f Hz",
                  ping_rate, liboculus::PingRateToHz(ping_rate));
      setPingRate(ping_rate);
    } else if (param.get_name() == "freq_mode") {
      int freq_mode = param.as_int();
      RCLCPP_INFO(node_->get_logger(), "Setting freq mode to %s",
                  liboculus::FreqModeToString(freq_mode).c_str());
      setFreqMode(freq_mode);
    } else if (param.get_name() == "data_size") {
      std::string data_size_str = param.as_string();
      int data_size = convertDataSizeString(data_size_str);
      RCLCPP_INFO(node_->get_logger(), "Setting data size: %s (%d)",
                  data_size_str.c_str(), data_size);
      setDataSize(data_size);
    } else if (param.get_name() == "send_range_as_meters") {
      sonar_config_.sendRangeAsMeters(param.as_bool());
    } else if (param.get_name() == "send_gain") {
      sonar_config_.setSendGain(param.as_bool());
    } else if (param.get_name() == "send_simple_return") {
      sonar_config_.setSimpleReturn(param.as_bool());
    } else if (param.get_name() == "gain_assistance") {
      sonar_config_.setGainAssistance(param.as_bool());
    }
  }

  for (auto& cb : listeners_) {
    cb(sonar_config_);
  }
  return result;
}

void OculusDriverConfig::updateSonarConfig() {
  double range = node_->get_parameter("range").as_double();
  int gain = node_->get_parameter("gain").as_int();
  int gamma = node_->get_parameter("gamma").as_int();
  int ping_rate = node_->get_parameter("ping_rate").as_int();
  int freq_mode = node_->get_parameter("freq_mode").as_int();
  std::string data_size_str = node_->get_parameter("data_size").as_string();
  int data_size = convertDataSizeString(data_size_str);
  bool send_range_as_meters = node_->get_parameter("send_range_as_meters").as_bool();
  bool send_gain = node_->get_parameter("send_gain").as_bool();
  bool send_simple_return = node_->get_parameter("send_simple_return").as_bool();
  bool gain_assistance = node_->get_parameter("gain_assistance").as_bool();

  sonar_config_.setRange(range);
  sonar_config_.setGainPercent(gain);
  sonar_config_.setGamma(gamma);
  setPingRate(ping_rate);
  setFreqMode(freq_mode);
  setDataSize(data_size);

  sonar_config_.sendRangeAsMeters(send_range_as_meters)
      .setSendGain(send_gain)
      .setSimpleReturn(send_simple_return)
      .setGainAssistance(gain_assistance);

  sonar_config_.use512Beams();
}

void OculusDriverConfig::setPingRate(int ping_rate) {
  switch (ping_rate) {
    case 0: sonar_config_.setPingRate(pingRateNormal); break;
    case 1: sonar_config_.setPingRate(pingRateHigh); break;
    case 2: sonar_config_.setPingRate(pingRateHighest); break;
    case 3: sonar_config_.setPingRate(pingRateLow); break;
    case 4: sonar_config_.setPingRate(pingRateLowest); break;
    case 5: sonar_config_.setPingRate(pingRateStandby); break;
    default:
      RCLCPP_WARN(node_->get_logger(), "Unknown ping rate %d", ping_rate);
  }
}

void OculusDriverConfig::setFreqMode(int freq_mode) {
  switch (freq_mode) {
    case 1: sonar_config_.setFreqMode(liboculus::OCULUS_LOW_FREQ); break;
    case 2: sonar_config_.setFreqMode(liboculus::OCULUS_HIGH_FREQ); break;
    default:
      RCLCPP_WARN(node_->get_logger(), "Unknown frequency mode %d", freq_mode);
  }
}

void OculusDriverConfig::setDataSize(int data_size) {
  switch (data_size) {
    case 1: sonar_config_.setDataSize(dataSize8Bit); break;
    case 2: sonar_config_.setDataSize(dataSize16Bit); break;
    case 4: sonar_config_.setDataSize(dataSize32Bit); break;
    default:
      RCLCPP_WARN(node_->get_logger(), "Unknown data size %d", data_size);
  }
}

int OculusDriverConfig::convertDataSizeString(const std::string& data_size_str) {
  if (data_size_str == "8bit") {
    return 1;
  } else if (data_size_str == "16bit") {
    return 2;
  } else if (data_size_str == "32bit") {
    return 4;
  } else {
    RCLCPP_WARN(node_->get_logger(),
                "Unknown data size string '%s', defaulting to 8bit",
                data_size_str.c_str());
    return 1;
  }
}

}  // namespace oculus_sonar
