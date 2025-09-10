#!/usr/bin/env python3
# oculus_launch.py
# Date: 250721
# Author: seungmin kim
# Description: ROS2 launch file for Oculus sonar driver. Launches the Oculus driver component and (optionally) visualization/postprocessing components.

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, TextSubstitution
from launch_ros.actions import ComposableNodeContainer, Node
from launch_ros.descriptions import ComposableNode


def generate_launch_description():
    """
    Generate the launch description for the Oculus sonar driver and related components.
    @return LaunchDescription object for ROS2 launch system.
    """
    # Declare launch arguments (equivalent to ROS1 <arg>)
    declare_range = DeclareLaunchArgument(
        'range', default_value='2.0',
        description='Sonar range in meters'
    )
    declare_gain_percent = DeclareLaunchArgument(
        'gain_percent', default_value='100',
        description='Gain percentage'
    )
    declare_gamma = DeclareLaunchArgument(
        'gamma', default_value='200',
        description='Gamma value'
    )
    declare_ping_rate = DeclareLaunchArgument(
        'ping_rate', default_value='3',
        description='Ping rate'
    )
    declare_freq_mode = DeclareLaunchArgument(
        'freq_mode', default_value='2',
        description='Frequency mode'
    )
    declare_ip_address = DeclareLaunchArgument(
        'ip_address', default_value='auto',
        description='IP address of sonar'
    )
    declare_send_gain = DeclareLaunchArgument(
        'send_gain', default_value='true',
        description='Send gain flag'
    )
    declare_data_size = DeclareLaunchArgument(
        'data_size', default_value='8bit',
        description='Data size (8bit, 16bit, 32bit)'
    )
    declare_frame_id = DeclareLaunchArgument(
        'frame_id', default_value='oculus',
        description='Frame ID for sonar data'
    )
    declare_draw_sonar = DeclareLaunchArgument(
        'draw_sonar', default_value='false',
        description='Enable sonar visualization (not yet implemented in ROS2)'
    )
    declare_postprocess_sonar = DeclareLaunchArgument(
        'postprocess_sonar', default_value='false',
        description='Enable sonar postprocessing (not yet implemented in ROS2)'
    )

    # Component container (equivalent to nodelet manager)
    container = ComposableNodeContainer(
        name='oculus_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        composable_node_descriptions=[
            # Main oculus driver component
            ComposableNode(
                package='oculus_sonar',
                plugin='oculus_sonar::OculusDriver',
                name='oculus_sonar',
                namespace='oculus',
                parameters=[
                    # Option 1: Load from YAML file
                    # PathJoinSubstitution([FindPackageShare('oculus_sonar'), 'config', 'oculus_params.yaml']),
                    
                    # Option 2: Override with launch arguments
                    {
                        'ip_address': LaunchConfiguration('ip_address'),
                        'range': LaunchConfiguration('range'),
                        'ping_rate': LaunchConfiguration('ping_rate'),
                        'gamma': LaunchConfiguration('gamma'),
                        'gain': LaunchConfiguration('gain_percent'),
                        'freq_mode': LaunchConfiguration('freq_mode'),
                        'data_size': LaunchConfiguration('data_size'),
                        'send_gain': LaunchConfiguration('send_gain'),
                        'frame_id': LaunchConfiguration('frame_id'),
                    }
                ],
            ),
            
            # NOTE: The following components are commented out because 
            # sonar_image_proc package has not been ported to ROS2 yet.
            # For visualization, use rviz2 to display the sonar_image topic.
            # Draw sonar component (conditional) - TODO: Port sonar_image_proc to ROS2
            # ComposableNode(
            #     package='sonar_image_proc',  # Not available in ROS2
            #     plugin='sonar_image_proc::DrawSonar',
            #     name='draw_sonar',
            #     namespace='oculus',
            #     parameters=[],
            #     remappings=[
            #         ('sonar_image', '/oculus/sonar_image'),
            #     ],
            #     condition=IfCondition(LaunchConfiguration('draw_sonar'))
            # ),
            # Postprocessor component (conditional) - TODO: Port sonar_image_proc to ROS2
            # ComposableNode(
            #     package='sonar_image_proc',  # Not available in ROS2
            #     plugin='sonar_image_proc::SonarPostprocessor',
            #     name='sonar_postprocessor',
            #     namespace='postprocess',
            #     parameters=[],
            #     remappings=[
            #         ('sonar_image', '/oculus/sonar_image'),
            #         ('sonar_image_postproc', '/postprocess/sonar_image'),
            #     ],
            #     condition=IfCondition(LaunchConfiguration('postprocess_sonar'))
            # ),
            # Postprocess draw sonar (conditional) - TODO: Port sonar_image_proc to ROS2
            # ComposableNode(
            #     package='sonar_image_proc',  # Not available in ROS2
            #     plugin='sonar_image_proc::DrawSonar',
            #     name='draw_sonar_postprocess',
            #     namespace='postprocess',
            #     parameters=[],
            #     remappings=[
            #         ('sonar_image', '/postprocess/sonar_image'),
            #     ],
            #     condition=IfCondition(LaunchConfiguration('postprocess_sonar'))
            # ),
        ],
        output='screen',
    )

    return LaunchDescription([
        # Launch arguments
        declare_range,
        declare_gain_percent,
        declare_gamma,
        declare_ping_rate,
        declare_freq_mode,
        declare_ip_address,
        declare_send_gain,
        declare_data_size,
        declare_frame_id,
        declare_draw_sonar,
        declare_postprocess_sonar,
        
        # Container with components
        container,
    ])
    
# NOTE: To visualize sonar data in ROS2, use rviz2:
# 1. Launch this file: ros2 launch oculus_sonar oculus_launch.py
# 2. Open rviz2: rviz2
# 3. Add Image display and set topic to: /oculus/sonar_image_raw
# 
# Available topics:
# - /oculus/sonar_image_raw (sensor_msgs/Image) - for rviz2 visualization
# - /oculus/sonar_image (marine_acoustic_msgs/SonarImage) - full sonar data with metadata