/**
 * @file publishing_data_rx.cpp
 * @date 250721
 * @author seungmin kim
 * @brief DataRx subclass for publishing raw data as ROS2 messages.
 */

#include "oculus_sonar/publishing_data_rx.h"
#include <rclcpp/rclcpp.hpp>

namespace oculus_sonar {

PublishingDataRx::PublishingDataRx(const liboculus::IoServiceThread::IoContextPtr &iosrv,
                                   const std::string &frame_id)
    : liboculus::DataRx(iosrv),
      raw_data_pub_(nullptr),
      clock_(std::make_shared<rclcpp::Clock>()),
      frame_id_(frame_id) {
}

PublishingDataRx::~PublishingDataRx() {
}

void PublishingDataRx::setRawPublisher(rclcpp::Publisher<apl_msgs::msg::RawData>::SharedPtr pub) {
  raw_data_pub_ = pub;
}

void PublishingDataRx::haveWritten(const std::vector<uint8_t> &bytes) {
  doPublish(bytes, 0); // 0 for DATA_OUT
}

void PublishingDataRx::haveRead(const std::vector<uint8_t> &bytes) {
  doPublish(bytes, 1); // 1 for DATA_IN
}

void PublishingDataRx::doPublish(const std::vector<uint8_t> &bytes, uint8_t direction) {
  if (!raw_data_pub_) {
    return;
  }

  auto msg = apl_msgs::msg::RawData();
  msg.header.stamp = clock_->now();
  msg.header.frame_id = frame_id_;
  msg.data = bytes;
  msg.direction = direction;

  raw_data_pub_->publish(msg);
}

} // namespace oculus_sonar
