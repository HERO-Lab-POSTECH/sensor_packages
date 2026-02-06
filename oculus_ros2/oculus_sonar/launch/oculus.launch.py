#!/usr/bin/env python3
"""
oculus.launch.py
Date: 260206
Description: Unified ROS2 launch file for all Oculus sonar models

================================================================================
USAGE
================================================================================

# Basic (driver only):
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d

# With fan image:
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d with_fan:=true
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d with_fan:=true colormap:=viridis

================================================================================
ARGUMENTS
================================================================================

Model Selection:
  sonar_model     Sonar model [m750d, m1200d, m3000d] (default: m750d)
  with_fan        Enable fan image converter [true/false] (default: false)

Driver Parameters (empty = use config file):
  ip_address      Sonar IP address
  range           Range in meters
  gain            Gain percentage (1-100)
  gamma           Gamma correction (0-255)
  ping_rate       Ping rate (0=Normal, 1=High, 2=Highest, 3=Low, 4=Lowest)
  freq_mode       Frequency mode (1=low, 2=high)
  data_size       Data size [8bit, 16bit, 32bit]
  num_beams       Number of beams (0=256, 1=512)

Fan Imager Parameters:
  apply_colormap  Apply colormap [true/false] (default: true)
  colormap        Colormap name (default: turbo)
                  Options: jet, hot, viridis, turbo, plasma, magma, inferno,
                          ocean, rainbow, bone, autumn, winter, summer, etc.

QoS Settings:
  qos_reliability QoS reliability for fan_imager subscriber (default: reliable)
                  Options: reliable, best_effort
                  Note: Use best_effort when playing back old bag files recorded
                        with BEST_EFFORT QoS

================================================================================
EXAMPLES
================================================================================

# M750d with default settings:
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d

# M3000d with fan image and viridis colormap:
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d with_fan:=true colormap:=viridis

# Override range and gain:
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d range:=15.0 gain:=80

# M3000d low frequency mode (1.2MHz, 130° FOV):
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d freq_mode:=1 range:=20.0

# Grayscale output (no colormap):
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d with_fan:=true apply_colormap:=false

# BEST_EFFORT QoS (for old bag files):
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d with_fan:=true qos_reliability:=best_effort

================================================================================
TOPICS PUBLISHED
================================================================================

Driver:
  /sensor/sonar/oculus/{model}/sonar      (SonarImage)
  /sensor/sonar/oculus/{model}/image      (Image - polar)
  /sensor/sonar/oculus/{model}/metadata   (OculusMetadata)

Fan Imager (when with_fan:=true):
  /sensor/sonar/oculus/{model}/fan_image  (Image - Cartesian)

================================================================================
DYNAMIC PARAMETER CHANGES
================================================================================

ros2 param set /oculus/fan_imager colormap viridis
ros2 param set /oculus/fan_imager apply_colormap false

================================================================================
"""

import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


def launch_setup(context, *args, **kwargs):
    """Setup function to handle conditional parameter loading."""
    # Get model selection
    sonar_model = LaunchConfiguration('sonar_model').perform(context)
    with_fan = LaunchConfiguration('with_fan').perform(context).lower() == 'true'

    # Get the path to the config file
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

    # Build parameter overrides dictionary for driver
    param_overrides = {}

    # Check each parameter and add only if provided (not empty)
    ip_address = LaunchConfiguration('ip_address').perform(context)
    if ip_address:
        param_overrides['ip_address'] = ip_address

    range_val = LaunchConfiguration('range').perform(context)
    if range_val:
        param_overrides['range'] = float(range_val)

    gain = LaunchConfiguration('gain').perform(context)
    if gain:
        param_overrides['gain'] = int(gain)

    gamma = LaunchConfiguration('gamma').perform(context)
    if gamma:
        param_overrides['gamma'] = int(gamma)

    ping_rate = LaunchConfiguration('ping_rate').perform(context)
    if ping_rate:
        param_overrides['ping_rate'] = int(ping_rate)

    freq_mode = LaunchConfiguration('freq_mode').perform(context)
    if freq_mode:
        param_overrides['freq_mode'] = int(freq_mode)

    data_size = LaunchConfiguration('data_size').perform(context)
    if data_size:
        param_overrides['data_size'] = data_size

    num_beams = LaunchConfiguration('num_beams').perform(context)
    if num_beams:
        param_overrides['num_beams'] = int(num_beams)

    # Fan imager parameters
    apply_colormap = LaunchConfiguration('apply_colormap').perform(context)
    colormap = LaunchConfiguration('colormap').perform(context)
    qos_reliability = LaunchConfiguration('qos_reliability').perform(context)

    # Build composable nodes list
    composable_nodes = [
        # Main driver component
        ComposableNode(
            package='oculus_sonar',
            plugin='oculus_sonar::OculusDriver',
            name=sonar_model,
            namespace='oculus',
            parameters=[
                config_file,
                param_overrides
            ] if param_overrides else [config_file],
        ),
    ]

    # Add fan imager if requested
    if with_fan:
        # Fan imager uses same config file and overrides as driver for consistency
        fan_imager_overrides = {
            'input_topic': f'/sensor/sonar/oculus/{sonar_model}/sonar',
            'output_topic': f'/sensor/sonar/oculus/{sonar_model}/fan_image',
            'apply_colormap': apply_colormap.lower() == 'true' if apply_colormap else True,
            'colormap': colormap if colormap else 'turbo',
            'qos_reliability': qos_reliability if qos_reliability else 'reliable',
        }
        # Apply same freq_mode override if provided (keep driver and fan_imager in sync)
        if freq_mode:
            fan_imager_overrides['freq_mode'] = int(freq_mode)

        composable_nodes.append(
            ComposableNode(
                package='oculus_sonar',
                plugin='oculus_sonar::OculusFanImager',
                name='fan_imager',
                namespace='oculus',
                parameters=[
                    config_file,  # Read freq_mode, sonar_model from config
                    fan_imager_overrides
                ],
            )
        )

    # Component container with nodes
    container = ComposableNodeContainer(
        name=f'oculus_{sonar_model}_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        composable_node_descriptions=composable_nodes,
        output='screen',
    )

    return [container]


def generate_launch_description():
    """
    Generate the launch description for Oculus sonar.
    Supports all models (M750d, M1200d, M3000d) with optional fan image converter.
    """
    return LaunchDescription([
        # Model selection
        DeclareLaunchArgument(
            'sonar_model', default_value='m750d',
            description='Sonar model: m750d, m1200d, m3000d'
        ),

        # Fan imager toggle
        DeclareLaunchArgument(
            'with_fan', default_value='false',
            description='Enable fan image converter: true/false'
        ),

        # Driver parameters (empty = use config file value)
        DeclareLaunchArgument(
            'ip_address', default_value='',
            description='Sonar IP address (empty = use config)'
        ),
        DeclareLaunchArgument(
            'range', default_value='',
            description='Sonar range in meters (empty = use config)'
        ),
        DeclareLaunchArgument(
            'gain', default_value='',
            description='Gain percentage (empty = use config)'
        ),
        DeclareLaunchArgument(
            'gamma', default_value='',
            description='Gamma correction (empty = use config)'
        ),
        DeclareLaunchArgument(
            'ping_rate', default_value='',
            description='Ping rate (empty = use config)'
        ),
        DeclareLaunchArgument(
            'freq_mode', default_value='',
            description='Frequency mode: 1=low, 2=high (empty = use config)'
        ),
        DeclareLaunchArgument(
            'data_size', default_value='',
            description='Data size: 8bit, 16bit, 32bit (empty = use config)'
        ),
        DeclareLaunchArgument(
            'num_beams', default_value='',
            description='Number of beams: 0=256, 1=512 (empty = use config)'
        ),

        # Fan imager parameters
        DeclareLaunchArgument(
            'apply_colormap', default_value='true',
            description='Apply colormap to fan image: true/false'
        ),
        DeclareLaunchArgument(
            'colormap', default_value='turbo',
            description='Colormap: jet, hot, viridis, turbo, plasma, magma, inferno, ocean, rainbow, etc.'
        ),
        DeclareLaunchArgument(
            'qos_reliability', default_value='reliable',
            description='QoS reliability for subscriber: reliable or best_effort'
        ),

        OpaqueFunction(function=launch_setup)
    ])

# ==============================================================================
# Usage Examples
# ==============================================================================
#
# Basic usage (driver only):
#   ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d
#   ros2 launch oculus_sonar oculus.launch.py sonar_model:=m1200d
#   ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d
#
# With fan image converter:
#   ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d with_fan:=true
#   ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d with_fan:=true colormap:=viridis
#
# Override driver parameters:
#   ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d range:=15.0 gain:=80
#   ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d freq_mode:=1 range:=20.0
#
# Different colormaps:
#   ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d with_fan:=true colormap:=hot
#   ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d with_fan:=true colormap:=ocean
#   ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d with_fan:=true colormap:=plasma
#
# Grayscale output (no colormap):
#   ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d with_fan:=true apply_colormap:=false
#
# ==============================================================================
# Topics Published
# ==============================================================================
#
# Driver (always):
#   /sensor/sonar/oculus/{model}/sonar      (SonarImage)
#   /sensor/sonar/oculus/{model}/image      (Image - polar)
#   /sensor/sonar/oculus/{model}/metadata   (OculusMetadata)
#   /sensor/sonar/oculus/{model}/raw_data   (RawData)
#
# Fan Imager (when with_fan:=true):
#   /sensor/sonar/oculus/{model}/fan_image  (Image - Cartesian)
#
# ==============================================================================
# Dynamic Parameter Changes
# ==============================================================================
#
#   ros2 param set /oculus/fan_imager colormap viridis
#   ros2 param set /oculus/fan_imager apply_colormap false
#   ros2 param set /oculus/fan_imager freq_mode 1
#   ros2 param set /oculus/fan_imager sonar_model m1200d
