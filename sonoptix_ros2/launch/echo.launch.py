#! /usr/bin/env python3
"""
Sonoptix Echo single-beam imaging sonar driver launch + image_transport republisher.

No launch arguments — all settings hardcoded inline. Edit this file to change
compression level, sonar range, or topic names.

Topics (hardcoded):
  /sensor/sonar/sonoptix/data       (sensor_msgs/Image, raw)
  /sensor/sonar/sonoptix/compressed (sensor_msgs/CompressedImage, png)

Examples:
  ros2 launch sonoptix_ros2 echo.launch.py
"""

# MIT License (c) 2025 ItsKalvik — see LICENSE for full text.

from launch_ros.actions import Node
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PythonExpression


def generate_launch_description():
    # Higher is more compressed but requires more compute [1-9]
    compression_level = 1
    sonar_range = 3  # meters
    data_topic = '/sensor/sonar/sonoptix/data'
    compressed_topic = '/sensor/sonar/sonoptix/compressed'
    image_topic = '/sensor/sonar/sonoptix/image'
    # QoS Config
    reliability = 'best_effort'

    frame_id_arg = DeclareLaunchArgument(
        'frame_id',
        default_value='sonoptix_link',
        description='Frame ID for Sonoptix Echo message headers',
    )
    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time',
        default_value='false',
        description='Honor /clock during bag replay.',
    )
    use_sim_time_expr = PythonExpression([
        "'", LaunchConfiguration('use_sim_time'), "' == 'true'",
    ])

    echo_data = Node(package='sonoptix_ros2',
                     executable='echo',
                     parameters=[{
                         'topic': data_topic,
                         'range': sonar_range,
                         'frame_id': LaunchConfiguration('frame_id'),
                         'use_sim_time': use_sim_time_expr,
                         f'qos_overrides.{data_topic}.publisher.reliability': reliability
                     }],
                     output='screen')

    echo_transport = Node(package='image_transport',
                          executable='republish',
                          name='echo_transport',
                          arguments=['raw', 'compressed'], # input data, output data
                          remappings=[('in', data_topic),
                                      ('out/compressed', compressed_topic)],
                          parameters=[{
                              'out.compressed.format': 'png',
                              'out.compressed.png_level': compression_level,
                              'use_sim_time': use_sim_time_expr,
                              f'qos_overrides.in.subscription.reliability': reliability,
                              f'qos_overrides.{compressed_topic}.publisher.reliability': reliability
                          }],
                          output='screen')
    
    # echo_imager = Node(package='sonoptix_ros2',
    #                    executable='echo_imager',
    #                    parameters=[{
    #                        'data_topic': compressed_topic,
    #                        'image_topic': image_topic,
    #                        'contrast': 10.0,
    #                        f'qos_overrides.{compressed_topic}.subscription.reliability': reliability,
    #                        f'qos_overrides.{image_topic}.publisher.reliability': reliability
    #                    }],
    #                    output='screen')

    # return LaunchDescription([echo_data, echo_transport, echo_imager])
    return LaunchDescription([frame_id_arg, use_sim_time_arg, echo_data, echo_transport])
