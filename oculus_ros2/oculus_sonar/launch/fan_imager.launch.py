#!/usr/bin/env python3
"""
fan_imager.launch.py
Date: 260206
Description: Standalone ROS2 launch file for Oculus Fan Image Converter

Use this to add fan image conversion to an already running sonar driver,
or when processing bag files.

================================================================================
USAGE
================================================================================

# Basic usage:
ros2 launch oculus_sonar fan_imager.launch.py sonar_model:=m750d
ros2 launch oculus_sonar fan_imager.launch.py sonar_model:=m3000d colormap:=viridis
ros2 launch oculus_sonar fan_imager.launch.py qos_reliability:=best_effort

================================================================================
ARGUMENTS
================================================================================

Model Selection:
  sonar_model     Sonar model [m750d, m1200d, m3000d] (default: m750d)
                  (freq_mode is read from config file automatically)

Colormap Settings:
  apply_colormap  Apply colormap [true/false] (default: true)
  colormap        Colormap name (default: jet)
                  Options: jet, hot, viridis, turbo, plasma, magma, inferno,
                          ocean, rainbow, bone, autumn, winter, summer, etc.

Intensity Threshold:
  intensity_min   Min intensity threshold [0-254] (default: 0)
  intensity_max   Max intensity threshold [1-255] (default: 255)
                  Normalizes [min, max] → [0, 255] then applies colormap
                  Example: intensity_min:=100 intensity_max:=200

QoS Settings:
  qos_reliability QoS reliability [reliable/best_effort] (default: reliable)
                  Use best_effort for bag file playback

Visualization:
  foxglove        Launch Foxglove bridge [true/false] (default: false)
                  Connect via ws://localhost:8765

Topic Overrides (optional):
  input_topic     Input SonarImage topic (empty = auto from model)
  output_topic    Output fan image topic (empty = auto from model)

================================================================================
EXAMPLES
================================================================================

# M750d with turbo colormap:
ros2 launch oculus_sonar fan_imager.launch.py sonar_model:=m750d

# M3000d with viridis colormap:
ros2 launch oculus_sonar fan_imager.launch.py sonar_model:=m3000d colormap:=viridis

# Custom topics (for bag file playback):
ros2 launch oculus_sonar fan_imager.launch.py \\
    input_topic:=/my/sonar/topic output_topic:=/my/fan/image

# Grayscale output:
ros2 launch oculus_sonar fan_imager.launch.py apply_colormap:=false

# With Foxglove bridge:
ros2 launch oculus_sonar fan_imager.launch.py foxglove:=true

================================================================================
DYNAMIC PARAMETER CHANGES
================================================================================

ros2 param set /oculus/fan_imager colormap viridis
ros2 param set /oculus/fan_imager apply_colormap false
ros2 param set /oculus/fan_imager intensity_min 100
ros2 param set /oculus/fan_imager intensity_max 200

================================================================================
"""

import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def launch_setup(context, *args, **kwargs):
    """Setup function for fan imager node."""
    # Get parameters
    sonar_model = LaunchConfiguration('sonar_model').perform(context)
    apply_colormap = LaunchConfiguration('apply_colormap').perform(context)
    colormap = LaunchConfiguration('colormap').perform(context)
    input_topic = LaunchConfiguration('input_topic').perform(context)
    output_topic = LaunchConfiguration('output_topic').perform(context)
    qos_reliability = LaunchConfiguration('qos_reliability').perform(context)
    intensity_min = LaunchConfiguration('intensity_min').perform(context)
    intensity_max = LaunchConfiguration('intensity_max').perform(context)
    foxglove = LaunchConfiguration('foxglove')

    # Get config file for this sonar model (contains freq_mode, sonar_model)
    config_file = os.path.join(
        get_package_share_directory('oculus_sonar'),
        'config',
        f'{sonar_model}_params.yaml'
    )

    # Check if config file exists
    if not os.path.exists(config_file):
        raise FileNotFoundError(
            f"Config file not found: {config_file}. "
            f"Valid models: m750d, m1200d, m3000d"
        )

    # Build topic names if not explicitly provided
    if not input_topic:
        input_topic = f'/sensor/sonar/oculus/{sonar_model}/sonar'
    if not output_topic:
        output_topic = f'/sensor/sonar/oculus/{sonar_model}/fan_image'

    # Override parameters (config file provides freq_mode, sonar_model)
    overrides = {
        'input_topic': input_topic,
        'output_topic': output_topic,
        'apply_colormap': apply_colormap.lower() == 'true' if apply_colormap else True,
        'colormap': colormap if colormap else 'jet',
        'qos_reliability': qos_reliability if qos_reliability else 'reliable',
        'intensity_min': int(intensity_min),
        'intensity_max': int(intensity_max),
    }

    # Create fan imager node
    fan_imager_node = Node(
        package='oculus_sonar',
        executable='oculus_fan_imager_node',
        name='fan_imager',
        namespace='oculus',
        parameters=[
            config_file,  # Read freq_mode, sonar_model from config
            overrides
        ],
        output='screen',
    )

    # Foxglove bridge (conditional)
    foxglove_bridge = Node(
        package='foxglove_bridge',
        executable='foxglove_bridge',
        condition=IfCondition(foxglove)
    )

    return [fan_imager_node, foxglove_bridge]


def generate_launch_description():
    """Generate launch description for standalone fan imager."""
    return LaunchDescription([
        # Sonar model selection (freq_mode is read from config file)
        DeclareLaunchArgument(
            'sonar_model', default_value='m750d',
            description='Sonar model: m750d, m1200d, m3000d (freq_mode from config)'
        ),

        # Colormap settings
        DeclareLaunchArgument(
            'apply_colormap', default_value='true',
            description='Apply colormap to fan image: true/false'
        ),
        DeclareLaunchArgument(
            'colormap', default_value='turbo',
            description='Colormap: jet, hot, viridis, turbo, plasma, magma, inferno, ocean, rainbow, etc.'
        ),

        # Topic overrides (optional)
        DeclareLaunchArgument(
            'input_topic', default_value='',
            description='Input SonarImage topic (empty = auto from model)'
        ),
        DeclareLaunchArgument(
            'output_topic', default_value='',
            description='Output fan image topic (empty = auto from model)'
        ),
        DeclareLaunchArgument(
            'qos_reliability', default_value='reliable',
            description='QoS reliability for subscriber: reliable or best_effort'
        ),
        # Intensity threshold
        DeclareLaunchArgument(
            'intensity_min', default_value='0',
            description='Intensity threshold min (0-254). Values below are clipped to 0'
        ),
        DeclareLaunchArgument(
            'intensity_max', default_value='255',
            description='Intensity threshold max (1-255). Values above are clipped to 255'
        ),

        DeclareLaunchArgument(
            'foxglove', default_value='false',
            description='Launch Foxglove bridge (connect via ws://localhost:8765)'
        ),

        OpaqueFunction(function=launch_setup)
    ])

# Usage examples:
# 1. Basic usage with M750d:
#    ros2 launch oculus_sonar fan_imager.launch.py sonar_model:=m750d
#
# 2. M3000d with viridis colormap:
#    ros2 launch oculus_sonar fan_imager.launch.py sonar_model:=m3000d colormap:=viridis
#
# 3. Custom topics (for bag file playback):
#    ros2 launch oculus_sonar fan_imager.launch.py \
#        input_topic:=/my/sonar/topic output_topic:=/my/fan/image
#
# 4. No colormap (grayscale):
#    ros2 launch oculus_sonar fan_imager.launch.py apply_colormap:=false
#
# Dynamic parameter changes:
#    ros2 param set /oculus/fan_imager colormap viridis
#    ros2 param set /oculus/fan_imager apply_colormap false
