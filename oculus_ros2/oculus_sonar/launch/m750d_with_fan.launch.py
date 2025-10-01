#!/usr/bin/env python3
# m750d_with_fan.launch.py
# Date: 251209
# Description: ROS2 launch file for Oculus M750d sonar with fan image converter

import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


def launch_setup(context, *args, **kwargs):
    """
    Setup function to handle conditional parameter loading.
    """
    # Get the path to the config file
    config_file = os.path.join(
        get_package_share_directory('oculus_sonar'),
        'config',
        'm750d_params.yaml'
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
    colormap_type = LaunchConfiguration('colormap_type').perform(context)

    # Component container with both nodes
    container = ComposableNodeContainer(
        name='oculus_m750d_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        composable_node_descriptions=[
            # Main driver component
            ComposableNode(
                package='oculus_sonar',
                plugin='oculus_sonar::OculusDriver',
                name='m750d',
                namespace='oculus',
                parameters=[
                    config_file,  # Load config file first
                    param_overrides  # Then apply overrides (if any)
                ] if param_overrides else [config_file],  # Only config if no overrides
            ),
            # Fan image converter component
            ComposableNode(
                package='oculus_sonar',
                plugin='oculus_sonar::OculusFanImager',
                name='fan_imager',
                namespace='oculus',
                parameters=[{
                    'input_topic': '/sensor/sonar/oculus/m750d/image',
                    'output_topic': '/sensor/sonar/oculus/m750d/fan_image',
                    'sonar_model': 'm750d',
                    'freq_mode': int(freq_mode) if freq_mode else 2,
                    'apply_colormap': apply_colormap.lower() == 'true' if apply_colormap else True,
                    'colormap_type': int(colormap_type) if colormap_type else 2,  # COLORMAP_JET
                }],
            ),
        ],
        output='screen',
    )

    return [container]


def generate_launch_description():
    """
    Generate the launch description for the Oculus M750d sonar with fan image converter.
    Supports command-line argument overrides for all parameters.
    """
    # Declare launch arguments for driver (with empty default = use config value)
    declare_ip_address = DeclareLaunchArgument(
        'ip_address', default_value='',
        description='Sonar IP address (empty to use config value)'
    )
    declare_range = DeclareLaunchArgument(
        'range', default_value='',
        description='Sonar range in meters (empty to use config value)'
    )
    declare_gain = DeclareLaunchArgument(
        'gain', default_value='',
        description='Gain percentage (empty to use config value)'
    )
    declare_gamma = DeclareLaunchArgument(
        'gamma', default_value='',
        description='Gamma correction (empty to use config value)'
    )
    declare_ping_rate = DeclareLaunchArgument(
        'ping_rate', default_value='',
        description='Ping rate (empty to use config value)'
    )
    declare_freq_mode = DeclareLaunchArgument(
        'freq_mode', default_value='',
        description='Frequency mode (empty to use config value)'
    )
    declare_data_size = DeclareLaunchArgument(
        'data_size', default_value='',
        description='Data size: 8bit, 16bit, 32bit (empty to use config value)'
    )
    declare_num_beams = DeclareLaunchArgument(
        'num_beams', default_value='',
        description='Number of beams: 0=256, 1=512 (empty to use config value)'
    )
    
    # Declare launch arguments for fan imager
    declare_apply_colormap = DeclareLaunchArgument(
        'apply_colormap', default_value='true',
        description='Apply colormap to fan image: true/false'
    )
    declare_colormap_type = DeclareLaunchArgument(
        'colormap_type', default_value='2',
        description='OpenCV colormap type (2=JET, 11=HOT, 12=COOL, etc.)'
    )

    return LaunchDescription([
        # Declare arguments
        declare_ip_address,
        declare_range,
        declare_gain,
        declare_gamma,
        declare_ping_rate,
        declare_freq_mode,
        declare_data_size,
        declare_num_beams,
        declare_apply_colormap,
        declare_colormap_type,
        # Use OpaqueFunction to handle conditional parameters
        OpaqueFunction(function=launch_setup)
    ])

# Usage examples:
# 1. Default settings from config file with fan image:
#    ros2 launch oculus_sonar m750d_with_fan.launch.py
#
# 2. Override specific parameters:
#    ros2 launch oculus_sonar m750d_with_fan.launch.py range:=10.0 gain:=80
#    ros2 launch oculus_sonar m750d_with_fan.launch.py ip_address:=192.168.0.201
#
# 3. Customize fan image settings:
#    ros2 launch oculus_sonar m750d_with_fan.launch.py apply_colormap:=false
#    ros2 launch oculus_sonar m750d_with_fan.launch.py colormap_type:=11
#
# Topics published:
# Driver:
# - /sensor/sonar/oculus/m750d/image     (SonarImage)
# - /sensor/sonar/oculus/m750d/image     (Image - polar)
# - /sensor/sonar/oculus/m750d/metadata  (OculusMetadata)
# - /sensor/sonar/oculus/m750d/raw_data  (RawData)
# - /sensor/sonar/oculus/m750d/param/*   (Parameters)
#
# Fan Imager:
# - /sensor/sonar/oculus/m750d/fan_image (Image - Cartesian)