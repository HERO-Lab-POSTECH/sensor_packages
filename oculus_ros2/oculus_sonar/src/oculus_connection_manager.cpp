/**
 * @file oculus_connection_manager.cpp
 * @brief Implementation of OculusConnectionManager.
 */

#include "oculus_sonar/oculus_connection_manager.hpp"

namespace oculus_sonar {

using liboculus::SimplePingResultV1;
using liboculus::SimplePingResultV2;

OculusConnectionManager::OculusConnectionManager(const std::string& ip_address,
                                                 const std::string& frame_id,
                                                 rclcpp::Logger logger,
                                                 rclcpp::Clock::SharedPtr clock)
    : ip_address_(ip_address),
      frame_id_(frame_id),
      logger_(logger),
      io_srv_(),
      status_rx_(io_srv_.context()) {
  data_rx_ = std::make_unique<PublishingDataRx>(io_srv_.context(), frame_id_, std::move(clock));
}

OculusConnectionManager::~OculusConnectionManager() {
  stop();
}

void OculusConnectionManager::setPingV1Callback(PingV1Callback cb) {
  data_rx_->setCallback<SimplePingResultV1>(std::move(cb));
}

void OculusConnectionManager::setPingV2Callback(PingV2Callback cb) {
  data_rx_->setCallback<SimplePingResultV2>(std::move(cb));
}

void OculusConnectionManager::setStatusCallback(StatusCallback cb) {
  status_rx_.setCallback(std::move(cb));
}

void OculusConnectionManager::setOnConnectCallback(std::function<void()> cb) {
  data_rx_->setOnConnectCallback(std::move(cb));
}

void OculusConnectionManager::setRawPublisher(
    rclcpp::Publisher<apl_msgs::msg::RawData>::SharedPtr pub) {
  data_rx_->setRawPublisher(pub);
}

void OculusConnectionManager::start() {
  if (ip_address_ == "auto") {
    RCLCPP_INFO(logger_, "Attempting to auto-detect sonar");
  } else {
    RCLCPP_INFO(logger_, "Opening sonar at %s", ip_address_.c_str());
    data_rx_->connect(ip_address_);
  }
  io_srv_.start();
}

void OculusConnectionManager::stop() {
  io_srv_.stop();
  io_srv_.join();
}

void OculusConnectionManager::sendSimpleFireMessage(
    const liboculus::SonarConfiguration& config) {
  data_rx_->sendSimpleFireMessage(config);
}

bool OculusConnectionManager::isConnected() const {
  return data_rx_->isConnected();
}

void OculusConnectionManager::connect(const boost::asio::ip::address& ip) {
  data_rx_->connect(ip);
}

}  // namespace oculus_sonar
