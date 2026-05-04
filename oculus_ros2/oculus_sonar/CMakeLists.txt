# CMakeLists.txt
# Date: 250721
# Author: seungmin kim
# Description: Build configuration for oculus_sonar ROS2 package. Handles dependencies, component registration, and installation.
#
cmake_minimum_required(VERSION 3.8)
project(oculus_sonar)

# Default to C++17
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_STANDARD 17)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(marine_acoustic_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(image_transport REQUIRED)
find_package(liboculus REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

find_package(g3log REQUIRED)

# For message generation
find_package(apl_msgs REQUIRED)
find_package(oculus_sonar_msgs REQUIRED)

# Include directories
include_directories(include)
include_directories(${OpenCV_INCLUDE_DIRS})

# Component library (replaces nodelet)
add_library(oculus_driver_component SHARED
  src/oculus_driver_component.cpp
  src/oculus_driver_config.cpp
  src/oculus_driver_publishers.cpp
  src/oculus_connection_manager.cpp
  src/publishing_data_rx.cpp
)

target_include_directories(oculus_driver_component PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../liboculus/include>
  $<INSTALL_INTERFACE:include>
  ${liboculus_INCLUDE_DIRS}
)

ament_target_dependencies(oculus_driver_component
  rclcpp
  rclcpp_components
  std_msgs
  sensor_msgs
  image_transport
  marine_acoustic_msgs
  apl_msgs
  oculus_sonar_msgs
  liboculus
  Boost
)

# Fan Imager Component (separate from main driver)
# Includes sonar_config and polar_to_cartesian for coordinate conversion
add_library(oculus_fan_imager_component SHARED
  src/oculus_fan_imager.cpp
  src/polar_to_cartesian.cpp
  src/sonar_config.cpp
)

target_include_directories(oculus_fan_imager_component PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../liboculus/include>
  $<INSTALL_INTERFACE:include>
  ${liboculus_INCLUDE_DIRS}
)

ament_target_dependencies(oculus_fan_imager_component
  rclcpp
  rclcpp_components
  sensor_msgs
  marine_acoustic_msgs
  cv_bridge
  image_transport
  OpenCV
  liboculus
)

# Link with generated messages
target_link_libraries(oculus_driver_component
  ${liboculus_LIBRARIES}
  g3log
)

target_link_libraries(oculus_fan_imager_component
  ${liboculus_LIBRARIES}
  ${OpenCV_LIBRARIES}
)

# Register components — register_node creates both the plugin entry
# AND a standalone executable. Earlier versions also called
# register_nodes which led to duplicate registration; removed in Phase B-1.
rclcpp_components_register_node(oculus_driver_component
  PLUGIN "oculus_sonar::OculusDriver"
  EXECUTABLE oculus_driver_node
)

rclcpp_components_register_node(oculus_fan_imager_component
  PLUGIN "oculus_sonar::OculusFanImager"
  EXECUTABLE oculus_fan_imager_node
)

# Install
install(TARGETS oculus_driver_component oculus_fan_imager_component
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/
  DESTINATION include
)

install(DIRECTORY 
  launch
  config
  DESTINATION share/${PROJECT_NAME}
)

# Install plugin description
install(FILES plugins.xml
  DESTINATION share/${PROJECT_NAME}
)

# Header files installation
install(FILES
  include/oculus_sonar/ping_to_sonar_image.hpp
  include/oculus_sonar/polar_to_cartesian.hpp
  include/oculus_sonar/sonar_config.hpp
  include/oculus_sonar/oculus_fan_imager.hpp
  DESTINATION include/${PROJECT_NAME}
)

# Export dependencies for downstream packages
# ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(
  rclcpp
  rclcpp_components
  std_msgs
  sensor_msgs
  marine_acoustic_msgs
  cv_bridge
  image_transport
  liboculus
  OpenCV
  Boost
)

ament_export_include_directories(include)
ament_export_libraries(oculus_driver_component oculus_fan_imager_component)

# Testing — Phase C-1 onwards.
# First gtest in oculus_sonar; tests byte-exact equivalence of
# pingToSonarImage refactor against frozen-old implementation.
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_ping_to_sonar_image
    test/test_ping_to_sonar_image.cpp
  )
  if(TARGET test_ping_to_sonar_image)
    target_include_directories(test_ping_to_sonar_image PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/../liboculus/include
    )
    ament_target_dependencies(test_ping_to_sonar_image
      rclcpp
      marine_acoustic_msgs
      liboculus
    )
    target_link_libraries(test_ping_to_sonar_image
      ${liboculus_LIBRARIES}
      g3log
    )
  endif()
endif()

ament_package()