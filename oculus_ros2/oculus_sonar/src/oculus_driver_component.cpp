/**
 * @file oculus_driver_component.cpp
 * @date 250721
 * @author seungmin kim
 * @brief ROS2 Oculus Sonar Driver main component implementation.
 *
 * This file implements the OculusDriver class, which provides the main ROS2 component for interfacing with the Oculus sonar device.
 */

#include <algorithm>
#include <boost/asio.hpp>
#include "liboculus/Constants.h"
#include "liboculus/SonarConfiguration.h"
#include "oculus_sonar/oculus_driver_component.hpp"
#include "oculus_sonar/publishing_data_rx.h"
#include "oculus_sonar/sonar_config.hpp"

namespace oculus_sonar {

using liboculus::SimplePingResultV1;
using liboculus::SimplePingResultV2;
OculusDriver::OculusDriver(const rclcpp::NodeOptions & options)
  : Node("oculus_driver", options),
    io_srv_(),
    status_rx_(io_srv_.context())
{
  init();
}

OculusDriver::~OculusDriver() {
  io_srv_.stop();
  io_srv_.join();
}

void OculusDriver::init() {
  // Declare parameters with default values
  this->declare_parameter<std::string>("ip_address", "auto");
  this->declare_parameter<std::string>("frame_id", "oculus");
  this->declare_parameter<std::string>("sonar_model", "m750d");  // m750d or m1200d
  this->declare_parameter<double>("range", SonarConstants::DEFAULT_RANGE_M);
  this->declare_parameter<int>("gain",     SonarConstants::DEFAULT_GAIN_PERCENT);
  this->declare_parameter<int>("gamma",    SonarConstants::DEFAULT_GAMMA);
  this->declare_parameter<int>("ping_rate", 3);
  this->declare_parameter<int>("freq_mode", 2);
  this->declare_parameter<bool>("send_gain", true);
  this->declare_parameter<std::string>("data_size", "8bit"); // string: "8bit", "16bit", "32bit"
  this->declare_parameter<bool>("send_range_as_meters", true);
  this->declare_parameter<bool>("send_simple_return", true);
  this->declare_parameter<bool>("gain_assistance", false);
  // num_beams parameter removed - always request 512 beams (hardware may downsample automatically)

  // Get initial parameter values
  ip_address_ = this->get_parameter("ip_address").as_string();
  frame_id_ = this->get_parameter("frame_id").as_string();
  sonar_model_ = this->get_parameter("sonar_model").as_string();

  data_rx_ = std::make_unique<PublishingDataRx>(io_srv_.context(), frame_id_);
  int freq_mode = this->get_parameter("freq_mode").as_int();

  // Initialize polar to Cartesian converter with frequency mode
  // Resolution is auto-calculated based on frequency mode

  RCLCPP_DEBUG(this->get_logger(), "Advertising topics in namespace %s",
               this->get_namespace());

  // Create topic prefix based on sonar model
  std::string topic_prefix = "/sensor/sonar/oculus/" + sonar_model_;

  // Create publishers
  // Use BEST_EFFORT QoS for all sensor data publishers
  auto sensor_qos = rclcpp::SensorDataQoS();

  imaging_sonar_pub_ = this->create_publisher<marine_acoustic_msgs::msg::SonarImage>(
    topic_prefix + "/sonar", sensor_qos);  // SonarImage 메시지
  oculus_meta_pub_ = this->create_publisher<oculus_sonar_msgs::msg::OculusMetadata>(
    topic_prefix + "/metadata", sensor_qos);
  raw_data_pub_ = this->create_publisher<apl_msgs::msg::RawData>(topic_prefix + "/raw_data", sensor_qos);
  image_pub_ = image_transport::create_publisher(this, topic_prefix + "/image", rmw_qos_profile_sensor_data);  // polar 이미지 (raw + compressed 자동 생성)


  // Create parameter publishers for recording
  ping_rate_pub_ = this->create_publisher<std_msgs::msg::Int32>(topic_prefix + "/param/ping_rate", sensor_qos);
  freq_mode_pub_ = this->create_publisher<std_msgs::msg::Int32>(topic_prefix + "/param/freq_mode", sensor_qos);
  data_size_pub_ = this->create_publisher<std_msgs::msg::String>(topic_prefix + "/param/data_size", sensor_qos);
  range_pub_ = this->create_publisher<std_msgs::msg::Float32>(topic_prefix + "/param/range", sensor_qos);
  gain_pub_ = this->create_publisher<std_msgs::msg::Int32>(topic_prefix + "/param/gain", sensor_qos);
  gamma_pub_ = this->create_publisher<std_msgs::msg::Int32>(topic_prefix + "/param/gamma", sensor_qos);
  ip_address_pub_ = this->create_publisher<std_msgs::msg::String>(topic_prefix + "/param/ip_address", sensor_qos);
  frame_id_pub_ = this->create_publisher<std_msgs::msg::String>(topic_prefix + "/param/frame_id", sensor_qos);
  sonar_model_pub_ = this->create_publisher<std_msgs::msg::String>(topic_prefix + "/param/sonar_model", sensor_qos);

  RCLCPP_INFO(this->get_logger(), "Publishing data with frame = %s", frame_id_.c_str());

  // Setup parameter callback for dynamic reconfiguration
  param_callback_handle_ = this->add_on_set_parameters_callback(
    std::bind(&OculusDriver::parameterCallback, this, std::placeholders::_1));

  // Initialize sonar configuration with current parameters
  updateSonarConfig();

  data_rx_->setRawPublisher(raw_data_pub_);

  data_rx_->setCallback<SimplePingResultV1>(std::bind(&OculusDriver::pingCallback<SimplePingResultV1>,
                                            this, std::placeholders::_1));

  data_rx_->setCallback<SimplePingResultV2>(std::bind(&OculusDriver::pingCallback<SimplePingResultV2>,
                                            this, std::placeholders::_1));

  // When the node connects, start the sonar pinging by sending
  // a OculusSimpleFireMessage current configuration.
  data_rx_->setOnConnectCallback([&]() {
    data_rx_->sendSimpleFireMessage(sonar_config_);
  });

  // Always setup status callback to check actual sonar model
  status_rx_.setCallback([&](const liboculus::SonarStatus &status, bool is_valid) {
    if (!is_valid) return;

    // Log the actual part number detected by hardware once
    static bool logged = false;
    if (!logged) {
      uint16_t part_num = status.msg()->partNumber;
      std::string detected_model = "Unknown";
      auto matches = [](uint16_t pn, const auto& list) {
        return std::find(list.begin(), list.end(), pn) != list.end();
      };
      if (matches(part_num, SonarConstants::M750D_PART_NUMBERS)) {
        detected_model = "M750d variant";
      } else if (matches(part_num, SonarConstants::M1200D_PART_NUMBERS)) {
        detected_model = "M1200d variant";
      } else if (matches(part_num, SonarConstants::M370S_PART_NUMBERS)) {
        detected_model = "M370s variant (70° FOV!)";
      } else if (matches(part_num, SonarConstants::M3000D_PART_NUMBERS)) {
        detected_model = "M3000d variant";
      }

      RCLCPP_WARN(this->get_logger(),
                  "Hardware detected - Part Number: %u (%s), Expected: %s",
                  part_num, detected_model.c_str(), sonar_model_.c_str());

      if (detected_model.find("M370") != std::string::npos) {
        RCLCPP_ERROR(this->get_logger(),
                    "WARNING: Hardware is M370s with 70° FOV, but config expects %s with 130° FOV!",
                    sonar_model_.c_str());
      }
      logged = true;
    }

    if (ip_address_ == "auto" && !data_rx_->isConnected()) {
      data_rx_->connect(status.ipAddr());
    }
  });

  if (ip_address_ == "auto") {
    RCLCPP_INFO(this->get_logger(), "Attempting to auto-detect sonar");
  } else {
    RCLCPP_INFO(this->get_logger(), "Opening sonar at %s", ip_address_.c_str());
    data_rx_->connect(ip_address_);
  }

  io_srv_.start();
}

// Parameter callback for dynamic reconfigure equivalent
rcl_interfaces::msg::SetParametersResult OculusDriver::parameterCallback(
  const std::vector<rclcpp::Parameter> & parameters) {
  
  rcl_interfaces::msg::SetParametersResult result;
  result.successful = true;

  for (const auto & param : parameters) {
    if (param.get_name() == "range") {
      double range = param.as_double();
      RCLCPP_INFO(this->get_logger(), "Setting sonar range to %.1f m", range);
      sonar_config_.setRange(range);
    }
    else if (param.get_name() == "gain") {
      int gain = param.as_int();
      RCLCPP_INFO(this->get_logger(), "Setting gain to %d pct", gain);
      sonar_config_.setGainPercent(gain);
    }
    else if (param.get_name() == "gamma") {
      int gamma = param.as_int();
      RCLCPP_INFO(this->get_logger(), "Setting gamma to %d", gamma);
      sonar_config_.setGamma(gamma);
    }
    else if (param.get_name() == "ping_rate") {
      int ping_rate = param.as_int();
      RCLCPP_INFO(this->get_logger(), "Setting ping rate to (%d): %.1f Hz", 
                  ping_rate, liboculus::PingRateToHz(ping_rate));
      setPingRate(ping_rate);
    }
    else if (param.get_name() == "freq_mode") {
      int freq_mode = param.as_int();
      RCLCPP_INFO(this->get_logger(), "Setting freq mode to %s",
                  liboculus::FreqModeToString(freq_mode).c_str());
      setFreqMode(freq_mode);
    }
    else if (param.get_name() == "data_size") {
      std::string data_size_str = param.as_string();
      int data_size = convertDataSizeString(data_size_str);
      RCLCPP_INFO(this->get_logger(), "Setting data size: %s (%d)", data_size_str.c_str(), data_size);
      setDataSize(data_size);
    }
    else if (param.get_name() == "send_range_as_meters") {
      sonar_config_.sendRangeAsMeters(param.as_bool());
    }
    else if (param.get_name() == "send_gain") {
      sonar_config_.setSendGain(param.as_bool());
    }
    else if (param.get_name() == "send_simple_return") {
      sonar_config_.setSimpleReturn(param.as_bool());
    }
    else if (param.get_name() == "gain_assistance") {
      sonar_config_.setGainAssistance(param.as_bool());
    }
  }

  // Update the sonar with new params
  if (data_rx_->isConnected()) {
    data_rx_->sendSimpleFireMessage(sonar_config_);
  }

  return result;
}

void OculusDriver::updateSonarConfig() {
  // Initialize configuration with current parameter values
  double range = this->get_parameter("range").as_double();
  int gain = this->get_parameter("gain").as_int();
  int gamma = this->get_parameter("gamma").as_int();
  int ping_rate = this->get_parameter("ping_rate").as_int();
  int freq_mode = this->get_parameter("freq_mode").as_int();
  std::string data_size_str = this->get_parameter("data_size").as_string();
  int data_size = convertDataSizeString(data_size_str);
  bool send_range_as_meters = this->get_parameter("send_range_as_meters").as_bool();
  bool send_gain = this->get_parameter("send_gain").as_bool();
  bool send_simple_return = this->get_parameter("send_simple_return").as_bool();
  bool gain_assistance = this->get_parameter("gain_assistance").as_bool();

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

  // Always request 512 beams (no downsampling)
  // M750d should provide 512 beams
  sonar_config_.use512Beams();
}

void OculusDriver::setPingRate(int ping_rate) {
  switch (ping_rate) {
    case 0: // OculusSonar_Normal
      sonar_config_.setPingRate(pingRateNormal);
      break;
    case 1: // OculusSonar_High
      sonar_config_.setPingRate(pingRateHigh);
      break;
    case 2: // OculusSonar_Highest
      sonar_config_.setPingRate(pingRateHighest);
      break;
    case 3: // OculusSonar_Low
      sonar_config_.setPingRate(pingRateLow);
      break;
    case 4: // OculusSonar_Lowest
      sonar_config_.setPingRate(pingRateLowest);
      break;
    case 5: // OculusSonar_Standby
      sonar_config_.setPingRate(pingRateStandby);
      break;
    default:
      RCLCPP_WARN(this->get_logger(), "Unknown ping rate %d", ping_rate);
  }
}

void OculusDriver::setFreqMode(int freq_mode) {
  switch (freq_mode) {
    case 1: // OculusSonar_LowFrequency
      sonar_config_.setFreqMode(liboculus::OCULUS_LOW_FREQ);
      break;
    case 2: // OculusSonar_HighFrequency
      sonar_config_.setFreqMode(liboculus::OCULUS_HIGH_FREQ);
      break;
    default:
      RCLCPP_WARN(this->get_logger(), "Unknown frequency mode %d", freq_mode);
  }
}

void OculusDriver::setDataSize(int data_size) {
  switch (data_size) {
    case 1: // OculusSonar_8bit
      sonar_config_.setDataSize(dataSize8Bit);
      break;
    case 2: // OculusSonar_16bit
      sonar_config_.setDataSize(dataSize16Bit);
      break;
    case 4: // OculusSonar_32bit
      sonar_config_.setDataSize(dataSize32Bit);
      break;
    default:
      RCLCPP_WARN(this->get_logger(), "Unknown data size %d", data_size);
  }
}

int OculusDriver::convertDataSizeString(const std::string& data_size_str) {
  if (data_size_str == "8bit") {
    return 1;
  } else if (data_size_str == "16bit") {
    return 2;
  } else if (data_size_str == "32bit") {
    return 4;
  } else {
    RCLCPP_WARN(this->get_logger(), "Unknown data size string '%s', defaulting to 8bit", 
                data_size_str.c_str());
    return 1;
  }
}

sensor_msgs::msg::Image OculusDriver::sonarToImage(
    const marine_acoustic_msgs::msg::SonarImage& sonar_msg) {
  sensor_msgs::msg::Image image_msg;
  
  image_msg.header = sonar_msg.header;
  
  // ranges와 azimuth_angles의 크기로 이미지 크기 계산
  image_msg.height = sonar_msg.ranges.size();
  image_msg.width = sonar_msg.azimuth_angles.size();
  
  // data_size에 따른 인코딩 설정
  if (sonar_msg.data_size == 1) {
    image_msg.encoding = "mono8";
  } else if (sonar_msg.data_size == 2) {
    image_msg.encoding = "mono16";  // ROS2에서는 "mono16" 사용
  } else if (sonar_msg.data_size == 4) {
    image_msg.encoding = "32FC1";
  }
  
  image_msg.is_bigendian = sonar_msg.is_bigendian;
  image_msg.step = image_msg.width * sonar_msg.data_size;
  
  // intensities 배열을 image data로 복사
  image_msg.data = sonar_msg.intensities;
  
  return image_msg;
}
void OculusDriver::publishParameters() {
  // Publish current parameters
  std_msgs::msg::Int32 ping_rate_msg;
  ping_rate_msg.data = this->get_parameter("ping_rate").as_int();
  ping_rate_pub_->publish(ping_rate_msg);
  
  std_msgs::msg::Int32 freq_mode_msg;
  freq_mode_msg.data = this->get_parameter("freq_mode").as_int();
  freq_mode_pub_->publish(freq_mode_msg);
  
  std_msgs::msg::String data_size_msg;
  data_size_msg.data = this->get_parameter("data_size").as_string();
  data_size_pub_->publish(data_size_msg);
  
  std_msgs::msg::Float32 range_msg;
  range_msg.data = static_cast<float>(this->get_parameter("range").as_double());
  range_pub_->publish(range_msg);
  
  std_msgs::msg::Int32 gain_msg;
  gain_msg.data = this->get_parameter("gain").as_int();
  gain_pub_->publish(gain_msg);
  
  std_msgs::msg::Int32 gamma_msg;
  gamma_msg.data = this->get_parameter("gamma").as_int();
  gamma_pub_->publish(gamma_msg);
  
  std_msgs::msg::String ip_msg;
  ip_msg.data = this->get_parameter("ip_address").as_string();
  ip_address_pub_->publish(ip_msg);
  
  std_msgs::msg::String frame_id_msg;
  frame_id_msg.data = this->get_parameter("frame_id").as_string();
  frame_id_pub_->publish(frame_id_msg);

  std_msgs::msg::String sonar_model_msg;
  sonar_model_msg.data = this->get_parameter("sonar_model").as_string();
  sonar_model_pub_->publish(sonar_model_msg);
}

}  // namespace oculus_sonar

// Register the component
RCLCPP_COMPONENTS_REGISTER_NODE(oculus_sonar::OculusDriver)