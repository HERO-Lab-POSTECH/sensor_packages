#! /usr/bin/env python3
"""
Sonoptix Echo image_transport decompress (compressed → raw).

No launch arguments — topic names hardcoded. Used to invert echo.launch.py's
compress step on a viewer/consumer node.

Topics (hardcoded):
  /sensor/sonar/sonoptix/compressed (input)
  /sensor/sonar/sonoptix/data       (output, raw)

Examples:
  ros2 launch sonoptix_ros2 echo_decompress.launch.py
"""

# MIT License (c) 2025 ItsKalvik — see LICENSE for full text.

from launch_ros.actions import Node
from launch import LaunchDescription


def generate_launch_description():
    data_topic = '/sensor/sonar/sonoptix/data'
    compressed_topic = '/sensor/sonar/sonoptix/compressed'
    # QoS Config
    reliability = 'best_effort'

    echo_decompress = Node(package='image_transport',
                           executable='republish',
                           arguments=['compressed', 'raw'],
                           remappings=[('in/compressed', compressed_topic),
                                       ('out', data_topic)],
                          parameters=[{
                              'qos_overrides./parameter_events.publisher.reliability': reliability
                          }],
                           output='screen')

    return LaunchDescription([echo_decompress])
