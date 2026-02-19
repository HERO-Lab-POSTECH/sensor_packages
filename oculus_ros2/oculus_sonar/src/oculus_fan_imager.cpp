/**
 * @file oculus_fan_imager.cpp
 * @date 260206
 * @brief ROS2 Oculus Fan Image Converter Node Implementation
 *
 * Converts polar sonar images to Cartesian fan images with configurable colormaps.
 * Supports M750d, M1200d, and M3000d sonar models.
 */

#include "oculus_sonar/oculus_fan_imager.hpp"
#include "oculus_sonar/sonar_config.hpp"
#include <rclcpp_components/register_node_macro.hpp>

namespace oculus_sonar {

OculusFanImager::OculusFanImager(const rclcpp::NodeOptions& options)
  : Node("oculus_fan_imager", options),
    frame_count_(0) {
  init();
}

void OculusFanImager::init() {
  // Declare parameters with default values
  this->declare_parameter<std::string>("input_topic", "/sensor/sonar/oculus/m750d/sonar");
  this->declare_parameter<std::string>("output_topic", "/sensor/sonar/oculus/m750d/fan_image");
  this->declare_parameter<std::string>("sonar_model", "m750d");
  this->declare_parameter<int>("freq_mode", 2);  // 1=low freq, 2=high freq
  this->declare_parameter<bool>("apply_colormap", true);
  this->declare_parameter<std::string>("colormap", "turbo");  // Default: turbo (perceptually uniform)
  this->declare_parameter<std::string>("qos_reliability", "reliable");  // reliable or best_effort

  // Get initial parameter values
  input_topic_ = this->get_parameter("input_topic").as_string();
  output_topic_ = this->get_parameter("output_topic").as_string();
  sonar_model_ = this->get_parameter("sonar_model").as_string();
  freq_mode_ = this->get_parameter("freq_mode").as_int();
  apply_colormap_ = this->get_parameter("apply_colormap").as_bool();
  colormap_name_ = this->get_parameter("colormap").as_string();

  // Log startup info
  RCLCPP_INFO(this->get_logger(), "Oculus Fan Imager Node started");
  RCLCPP_INFO(this->get_logger(), "  Input topic: %s", input_topic_.c_str());
  RCLCPP_INFO(this->get_logger(), "  Output topic: %s", output_topic_.c_str());
  RCLCPP_INFO(this->get_logger(), "  Sonar model: %s", sonar_model_.c_str());
  RCLCPP_INFO(this->get_logger(), "  Frequency mode: %d", freq_mode_);
  RCLCPP_INFO(this->get_logger(), "  Apply colormap: %s", apply_colormap_ ? "true" : "false");
  RCLCPP_INFO(this->get_logger(), "  Colormap: %s", colormap_name_.c_str());

  // Log supported models and colormaps
  auto models = SonarConfigRegistry::getSupportedModels();
  std::string models_str;
  for (const auto& m : models) models_str += m + " ";
  RCLCPP_INFO(this->get_logger(), "  Supported models: %s", models_str.c_str());

  // Initialize polar to Cartesian converter
  polar_converter_ = std::make_unique<PolarToCartesianConverter>(sonar_model_, freq_mode_);

  // Set colormap
  if (SonarConfigRegistry::isColormapSupported(colormap_name_)) {
    polar_converter_->setColormap(colormap_name_);
  } else {
    RCLCPP_WARN(this->get_logger(),
               "Unknown colormap '%s', using 'turbo'. Valid options: "
               "jet, hot, viridis, turbo, plasma, magma, inferno, ocean, rainbow, etc.",
               colormap_name_.c_str());
    colormap_name_ = "turbo";
    polar_converter_->setColormap(colormap_name_);
  }

  // Create subscriber for SonarImage messages
  // Default: RELIABLE QoS, can be changed to BEST_EFFORT via qos_reliability parameter
  std::string qos_reliability = this->get_parameter("qos_reliability").as_string();
  rclcpp::QoS qos(10);
  if (qos_reliability == "best_effort") {
    qos = rclcpp::SensorDataQoS();
    RCLCPP_INFO(this->get_logger(), "  QoS reliability: BEST_EFFORT");
  } else {
    // Default: RELIABLE
    RCLCPP_INFO(this->get_logger(), "  QoS reliability: RELIABLE");
  }
  sonar_sub_ = this->create_subscription<marine_acoustic_msgs::msg::SonarImage>(
    input_topic_, qos,
    std::bind(&OculusFanImager::sonarImageCallback, this, std::placeholders::_1));

  // Create publisher (image_transport: raw + compressed 자동 생성)
  fan_image_pub_ = image_transport::create_publisher(this, output_topic_, rmw_qos_profile_default);

  // Setup parameter callback for dynamic reconfiguration
  param_callback_handle_ = this->add_on_set_parameters_callback(
    std::bind(&OculusFanImager::parameterCallback, this, std::placeholders::_1));

  // Setup statistics timer (print every 5 seconds)
  stats_timer_ = this->create_wall_timer(
    std::chrono::seconds(5),
    std::bind(&OculusFanImager::printStatistics, this));

  last_image_time_ = this->get_clock()->now();
}

void OculusFanImager::sonarImageCallback(
    const marine_acoustic_msgs::msg::SonarImage::SharedPtr msg) {
  try {
    // Extract dimensions from SonarImage
    int num_ranges = msg->ranges.size();
    int num_beams = msg->azimuth_angles.size();

    if (num_ranges == 0 || num_beams == 0) {
      RCLCPP_ERROR(this->get_logger(), "Invalid dimensions: ranges=%d, beams=%d",
                   num_ranges, num_beams);
      return;
    }

    // Calculate max range from ranges array
    float max_range = msg->ranges.back();

    // Create OpenCV Mat from intensities data
    cv::Mat polar_image;
    if (msg->data_size == 1) {
      // 8-bit data
      polar_image = cv::Mat(num_ranges, num_beams, CV_8UC1,
                            const_cast<uint8_t*>(msg->intensities.data()));
    } else if (msg->data_size == 2) {
      // 16-bit data - convert to 8-bit
      cv::Mat temp(num_ranges, num_beams, CV_16UC1,
                   const_cast<uint8_t*>(msg->intensities.data()));
      temp.convertTo(polar_image, CV_8UC1, 255.0 / 65535.0);
    } else if (msg->data_size == 4) {
      // 32-bit float data - convert to 8-bit
      cv::Mat temp(num_ranges, num_beams, CV_32FC1,
                   const_cast<uint8_t*>(msg->intensities.data()));
      temp.convertTo(polar_image, CV_8UC1, 255.0);
    } else {
      RCLCPP_ERROR(this->get_logger(), "Unsupported data_size: %d", msg->data_size);
      return;
    }

    // Update mapping with actual bearings from sonar
    // Priority: use azimuth_angles from message (from liboculus)
    polar_converter_->updateMapping(num_ranges, num_beams, max_range,
                                    msg->azimuth_angles);

    // Convert polar to Cartesian
    cv::Mat fan_image = polar_converter_->convertToCartesian(polar_image, apply_colormap_);

    // Create output message
    cv_bridge::CvImage out_msg;
    out_msg.header = msg->header;
    out_msg.encoding = apply_colormap_ ? sensor_msgs::image_encodings::BGR8 :
                                        sensor_msgs::image_encodings::MONO8;
    out_msg.image = fan_image;

    // Publish fan image
    fan_image_pub_.publish(*out_msg.toImageMsg());

    // Update statistics
    frame_count_++;
    last_image_time_ = this->get_clock()->now();

  } catch (const cv_bridge::Exception& e) {
    RCLCPP_ERROR(this->get_logger(), "CV Bridge exception: %s", e.what());
  } catch (const std::exception& e) {
    RCLCPP_ERROR(this->get_logger(), "Exception in sonarImageCallback: %s", e.what());
  }
}

rcl_interfaces::msg::SetParametersResult OculusFanImager::parameterCallback(
  const std::vector<rclcpp::Parameter>& parameters) {

  rcl_interfaces::msg::SetParametersResult result;
  result.successful = true;

  for (const auto& param : parameters) {
    if (param.get_name() == "freq_mode") {
      int new_freq_mode = param.as_int();
      if (new_freq_mode != freq_mode_) {
        freq_mode_ = new_freq_mode;
        polar_converter_->setFrequencyMode(freq_mode_);
        RCLCPP_INFO(this->get_logger(), "Frequency mode changed to %d", freq_mode_);
      }
    }
    else if (param.get_name() == "apply_colormap") {
      apply_colormap_ = param.as_bool();
      RCLCPP_INFO(this->get_logger(), "Apply colormap: %s",
                  apply_colormap_ ? "true" : "false");
    }
    else if (param.get_name() == "colormap") {
      std::string new_colormap = param.as_string();
      if (SonarConfigRegistry::isColormapSupported(new_colormap)) {
        colormap_name_ = new_colormap;
        polar_converter_->setColormap(colormap_name_);
        RCLCPP_INFO(this->get_logger(), "Colormap changed to '%s'", colormap_name_.c_str());
      } else {
        RCLCPP_WARN(this->get_logger(),
                   "Unknown colormap '%s'. Valid: jet, hot, viridis, turbo, plasma, etc.",
                   new_colormap.c_str());
        result.successful = false;
        result.reason = "Unknown colormap: " + new_colormap;
      }
    }
    else if (param.get_name() == "sonar_model") {
      std::string new_model = param.as_string();
      if (SonarConfigRegistry::isModelSupported(new_model)) {
        if (new_model != sonar_model_) {
          sonar_model_ = new_model;
          polar_converter_->setSonarModel(sonar_model_);
          RCLCPP_INFO(this->get_logger(), "Sonar model changed to %s", sonar_model_.c_str());
        }
      } else {
        RCLCPP_WARN(this->get_logger(),
                   "Unknown sonar model '%s'. Valid: m750d, m1200d, m3000d",
                   new_model.c_str());
        result.successful = false;
        result.reason = "Unknown model: " + new_model;
      }
    }
  }

  return result;
}

void OculusFanImager::printStatistics() {
  if (frame_count_ > 0) {
    auto current_time = this->get_clock()->now();
    double elapsed_sec = (current_time - last_image_time_).seconds();

    if (elapsed_sec < 1.0) {
      RCLCPP_INFO(this->get_logger(),
                  "Fan Imager Stats: %d frames processed, last update %.2f sec ago",
                  frame_count_, elapsed_sec);
    } else {
      RCLCPP_WARN(this->get_logger(),
                  "Fan Imager Stats: No new frames for %.2f seconds (total: %d frames)",
                  elapsed_sec, frame_count_);
    }
  } else {
    RCLCPP_INFO(this->get_logger(), "Fan Imager Stats: Waiting for images on %s",
                input_topic_.c_str());
  }
}

}  // namespace oculus_sonar

// Register the component
RCLCPP_COMPONENTS_REGISTER_NODE(oculus_sonar::OculusFanImager)
