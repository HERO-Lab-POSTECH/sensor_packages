/**
 * @file oculus_driver_component.cpp
 * @date 250721
 * @author seungmin kim
 * @brief ROS2 Oculus Sonar Driver main component implementation.
 *
 * This file implements the OculusDriver class, which provides the main ROS2 component for interfacing with the Oculus sonar device.
 */

#include <algorithm>
#include "liboculus/Constants.h"
#include "oculus_sonar/oculus_driver_component.hpp"
#include "oculus_sonar/oculus_driver_config.hpp"
#include "oculus_sonar/oculus_driver_publishers.hpp"
#include "oculus_sonar/oculus_connection_manager.hpp"
#include "oculus_sonar/sonar_config.hpp"

namespace oculus_sonar {

using liboculus::SimplePingResultV1;
using liboculus::SimplePingResultV2;

OculusDriver::OculusDriver(const rclcpp::NodeOptions & options)
  : Node("oculus_driver", options) {
  init();
}

OculusDriver::~OculusDriver() = default;

void OculusDriver::init() {
  config_ = std::make_unique<OculusDriverConfig>(this);
  config_->initialize();

  RCLCPP_DEBUG(this->get_logger(), "Advertising topics in namespace %s",
               this->get_namespace());

  std::string topic_prefix = "/sensor/sonar/oculus/" + config_->sonar_model();
  publishers_ = std::make_unique<OculusDriverPublishers>(this, topic_prefix, config_->frame_id());
  publishers_->initialize();

  connection_ = std::make_unique<OculusConnectionManager>(
      config_->ip_address(), config_->frame_id(), this->get_logger());

  connection_->setRawPublisher(publishers_->raw_data_publisher());

  connection_->setPingV1Callback(
      [this](const liboculus::SimplePingResultV1& ping) { publishers_->publishPing(ping); });
  connection_->setPingV2Callback(
      [this](const liboculus::SimplePingResultV2& ping) { publishers_->publishPing(ping); });

  connection_->setOnConnectCallback([this]() {
    connection_->sendSimpleFireMessage(config_->current());
  });

  connection_->setStatusCallback(
      [this](const liboculus::SonarStatus& status, bool is_valid) {
        if (!is_valid) return;

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

        if (config_->ip_address() == "auto" && !connection_->isConnected()) {
          connection_->connect(status.ipAddr());
        }
      });

  config_->on_change([this](const liboculus::SonarConfiguration& new_config) {
    if (connection_->isConnected()) {
      connection_->sendSimpleFireMessage(new_config);
    }
  });

  connection_->start();
}

}  // namespace oculus_sonar

// Register the component
RCLCPP_COMPONENTS_REGISTER_NODE(oculus_sonar::OculusDriver)
