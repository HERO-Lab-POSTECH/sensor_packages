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
#include "oculus_sonar/oculus_driver_config.hpp"
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
  config_ = std::make_unique<OculusDriverConfig>(this);
  config_->initialize();

  data_rx_ = std::make_unique<PublishingDataRx>(io_srv_.context(), config_->frame_id());

  RCLCPP_DEBUG(this->get_logger(), "Advertising topics in namespace %s",
               this->get_namespace());

  std::string topic_prefix = "/sensor/sonar/oculus/" + config_->sonar_model();

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

  RCLCPP_INFO(this->get_logger(), "Publishing data with frame = %s", config_->frame_id().c_str());

  data_rx_->setRawPublisher(raw_data_pub_);

  data_rx_->setCallback<SimplePingResultV1>(std::bind(&OculusDriver::pingCallback<SimplePingResultV1>,
                                            this, std::placeholders::_1));

  data_rx_->setCallback<SimplePingResultV2>(std::bind(&OculusDriver::pingCallback<SimplePingResultV2>,
                                            this, std::placeholders::_1));

  // When the node connects, start the sonar pinging by sending
  // a OculusSimpleFireMessage current configuration.
  data_rx_->setOnConnectCallback([this]() {
    data_rx_->sendSimpleFireMessage(config_->current());
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
                  part_num, detected_model.c_str(), config_->sonar_model().c_str());

      if (detected_model.find("M370") != std::string::npos) {
        RCLCPP_ERROR(this->get_logger(),
                    "WARNING: Hardware is M370s with 70° FOV, but config expects %s with 130° FOV!",
                    config_->sonar_model().c_str());
      }
      logged = true;
    }

    if (config_->ip_address() == "auto" && !data_rx_->isConnected()) {
      data_rx_->connect(status.ipAddr());
    }
  });

  config_->on_change([this](const liboculus::SonarConfiguration& new_config) {
    if (data_rx_->isConnected()) {
      data_rx_->sendSimpleFireMessage(new_config);
    }
  });

  if (config_->ip_address() == "auto") {
    RCLCPP_INFO(this->get_logger(), "Attempting to auto-detect sonar");
  } else {
    RCLCPP_INFO(this->get_logger(), "Opening sonar at %s", config_->ip_address().c_str());
    data_rx_->connect(config_->ip_address());
  }

  io_srv_.start();
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