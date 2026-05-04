/**
 * @file oculus_connection_manager.hpp
 * @brief Owns IoServiceThread, PublishingDataRx, StatusRx and dispatches
 *        ping/status callbacks. No direct ROS2 publishing — that is the
 *        Publishers' responsibility.
 */
#pragma once

#include <functional>
#include <memory>
#include <string>

#include <apl_msgs/msg/raw_data.hpp>
#include <rclcpp/rclcpp.hpp>

#include "liboculus/IoServiceThread.h"
#include "liboculus/SimplePingResult.h"
#include "liboculus/SonarConfiguration.h"
#include "liboculus/StatusRx.h"
#include "oculus_sonar/publishing_data_rx.h"

namespace oculus_sonar {

class OculusConnectionManager {
 public:
  using PingV1Callback =
      std::function<void(const liboculus::SimplePingResult<OculusSimplePingResult>&)>;
  using PingV2Callback =
      std::function<void(const liboculus::SimplePingResult<OculusSimplePingResult2>&)>;
  using StatusCallback = std::function<void(const liboculus::SonarStatus&, bool valid)>;

  OculusConnectionManager(const std::string& ip_address,
                          const std::string& frame_id,
                          rclcpp::Logger logger);
  ~OculusConnectionManager();

  OculusConnectionManager(const OculusConnectionManager&) = delete;
  OculusConnectionManager& operator=(const OculusConnectionManager&) = delete;

  // Wire callbacks BEFORE start().
  void setPingV1Callback(PingV1Callback cb);
  void setPingV2Callback(PingV2Callback cb);
  void setStatusCallback(StatusCallback cb);
  void setOnConnectCallback(std::function<void()> cb);

  void setRawPublisher(rclcpp::Publisher<apl_msgs::msg::RawData>::SharedPtr pub);

  // Lifecycle: start the io_service thread and (if non-auto) connect.
  void start();
  void stop();

  // Forward to PublishingDataRx.
  void sendSimpleFireMessage(const liboculus::SonarConfiguration& config);
  bool isConnected() const;

  // For status callback that wants to trigger auto-connect upon discovery.
  void connect(const boost::asio::ip::address& ip);

 private:
  std::string ip_address_;
  std::string frame_id_;
  rclcpp::Logger logger_;

  liboculus::IoServiceThread io_srv_;
  std::unique_ptr<PublishingDataRx> data_rx_;
  liboculus::StatusRx status_rx_;
};

}  // namespace oculus_sonar
