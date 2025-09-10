cmake_minimum_required(VERSION 3.5)
project(apl_msgs)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/Adc.msg"
  "msg/CharucoDetection.msg"
  "msg/Ctd.msg"
  "msg/DissolvedOxygen.msg"
  "msg/Int16Stamped.msg"
  "msg/JointPositionCommand.msg"
  "msg/LeakDetection.msg"
  "msg/LeakVoltage.msg"
  "msg/LightBrightness.msg"
  "msg/SetLightLevel.msg"
  "msg/IncrementLightLevel.msg"
  "msg/PowerCommand.msg"
  "msg/PowerStatus.msg"
  "msg/RawData.msg"
  "msg/SeapointTurbidity.msg"
  "msg/StringStamped.msg"
  "msg/UInt16Stamped.msg"
  DEPENDENCIES std_msgs
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
