#! /usr/bin/env python3

#-----------------------------------------------------------------------------------
# MIT License

# Copyright (c) 2025 ItsKalvik

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#-----------------------------------------------------------------------------------

from launch_ros.actions import Node
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    # Declare launch arguments
    declare_data_topic = DeclareLaunchArgument(
        'data_topic',
        default_value='/sensor/sonar/sonoptix/data',
        description='Topic name for decompressed sonar data output'
    )
    
    declare_compressed_topic = DeclareLaunchArgument(
        'compressed_topic',
        default_value='/sensor/sonar/sonoptix/compressed',
        description='Topic name for compressed sonar data input'
    )
    
    declare_reliability = DeclareLaunchArgument(
        'reliability',
        default_value='best_effort',
        description='QoS reliability setting (best_effort/reliable)'
    )
    
    # Get launch configurations
    data_topic = LaunchConfiguration('data_topic')
    compressed_topic = LaunchConfiguration('compressed_topic')
    reliability = LaunchConfiguration('reliability')

    echo_decompress = Node(package='image_transport',
                           executable='republish',
                           arguments=['compressed', 'raw'],
                           remappings=[('in/compressed', compressed_topic),
                                       ('out', data_topic)],
                           output='screen')

    return LaunchDescription([
        declare_data_topic,
        declare_compressed_topic,
        declare_reliability,
        echo_decompress
    ])
