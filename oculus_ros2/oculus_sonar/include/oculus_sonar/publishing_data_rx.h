/**
 * @file publishing_data_rx.h
 * @date 250721
 * @author seungmin kim
 * @brief DataRx subclass for publishing raw data as ROS2 messages.
 */

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <apl_msgs/msg/raw_data.hpp>
#include <vector>
#include "liboculus/DataRx.h"

namespace oculus_sonar {

/**
 * @class PublishingDataRx
 * @brief Subclass of liboculus::DataRx for publishing raw data as ROS2 messages.
 */
class PublishingDataRx : public liboculus::DataRx {
 public:
  /**
   * @brief Constructor.
   * @param iosrv Shared pointer to IO service context.
   */
  PublishingDataRx(const liboculus::IoServiceThread::IoContextPtr &iosrv);

  /**
   * @brief Destructor.
   */
  ~PublishingDataRx();

  /**
   * @brief Set the publisher for raw data messages.
   * @param pub Shared pointer to the publisher.
   */
  void setRawPublisher(rclcpp::Publisher<apl_msgs::msg::RawData>::SharedPtr pub);

  /**
   * @brief Called when data has been written.
   * @param bytes Data bytes written.
   */
  void haveWritten(const std::vector<uint8_t> &bytes) override;

  /**
   * @brief Called when data has been read.
   * @param bytes Data bytes read.
   */
  void haveRead(const std::vector<uint8_t> &bytes) override;

  /**
   * @brief Publish the raw data message.
   * @param bytes Data bytes.
   * @param direction Direction flag (DATA_IN/OUT).
   */
  void doPublish(const std::vector<uint8_t> &bytes, uint8_t direction);

private:
  rclcpp::Publisher<apl_msgs::msg::RawData>::SharedPtr raw_data_pub_;
  rclcpp::Clock::SharedPtr clock_;
};

} // namespace oculus_sonar