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
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy
import os
import sys


def generate_launch_description():
    # Declare launch arguments
    declare_ip = DeclareLaunchArgument(
        'ip',
        default_value='192.168.0.203',
        description='IP address of the Sonoptix sonar'
    )
    
    declare_range = DeclareLaunchArgument(
        'range',
        default_value='10',
        description='Sonar range in meters (minimum 3m)'
    )
    
    declare_operation_mode = DeclareLaunchArgument(
        'operation_mode',
        default_value='0',
        description='Operation mode (0-3, likely frequency selection)'
    )
    
    declare_tx_mode = DeclareLaunchArgument(
        'tx_mode',
        default_value='auto',
        description='Transmission mode (auto/manual)'
    )
    
    declare_power_state = DeclareLaunchArgument(
        'power_state',
        default_value='True',
        description='Initial power state of the sonar'
    )
    
    declare_data_topic = DeclareLaunchArgument(
        'data_topic',
        default_value='/sensor/sonar/sonoptix/data',
        description='Topic name for raw sonar data'
    )
    
    declare_compressed_topic = DeclareLaunchArgument(
        'compressed_topic',
        default_value='/sensor/sonar/sonoptix/compressed',
        description='Topic name for compressed sonar data'
    )
    
    declare_frame_id = DeclareLaunchArgument(
        'frame_id',
        default_value='echo',
        description='Frame ID for the sonar data'
    )
    
    declare_compression_level = DeclareLaunchArgument(
        'compression_level',
        default_value='1',  # 최고 품질 (압축 최소화)
        description='Compression level [1-9], higher is more compressed but requires more compute'
    )
    
    declare_reliability = DeclareLaunchArgument(
        'reliability',
        default_value='best_effort',
        description='QoS reliability setting (best_effort/reliable)'
    )
    
    # Get launch configurations
    ip = LaunchConfiguration('ip')
    sonar_range = LaunchConfiguration('range')
    operation_mode = LaunchConfiguration('operation_mode')
    tx_mode = LaunchConfiguration('tx_mode')
    power_state = LaunchConfiguration('power_state')
    data_topic = LaunchConfiguration('data_topic')
    compressed_topic = LaunchConfiguration('compressed_topic')
    frame_id = LaunchConfiguration('frame_id')
    compression_level = LaunchConfiguration('compression_level')
    reliability = LaunchConfiguration('reliability')

    # QoS profile for image data
    image_qos = QoSProfile(
        reliability=ReliabilityPolicy.BEST_EFFORT,
        durability=DurabilityPolicy.VOLATILE,
        depth=1
    )

    echo_data = Node(package='sonoptix_ros2',
                     executable='echo',
                     parameters=[{
                         'ip': ip,
                         'range': sonar_range,
                         'operation_mode': operation_mode,
                         'tx_mode': tx_mode,
                         'power_state': power_state,
                         'topic': data_topic,
                         'frame_id': frame_id,
                     }],
                     output='screen')

    # High-quality compressed image publisher with proper QoS
    echo_transport = Node(package='image_transport',
                          executable='republish',
                          name='echo_transport',
                          arguments=['raw', 'compressed'], # input data, output data
                          remappings=[('in', data_topic),
                                      ('out/compressed', compressed_topic)],
                          parameters=[{
                              'out.compressed.format': 'png',  # PNG는 무손실 압축
                              'out.compressed.png_level': compression_level,
                              'qos_overrides.in.subscription.reliability': 'best_effort',
                              'qos_overrides.out.compressed.publisher.reliability': 'best_effort',
                          }],
                          output='screen')
    
    # 릴레이 제어 노드 추가 (CH1 for Sonoptix Echo)
    from launch.actions import ExecuteProcess
    
    relay_controller_path = os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        'relay_controller',
        'relay_node.py'
    )
    
    relay_node = ExecuteProcess(
        cmd=[sys.executable, relay_controller_path,
             '--ros-args',
             '-p', 'channel:=1',
             '-p', 'sensor_name:=Sonoptix Echo'],
        name='relay_controller_sonoptix',
        output='screen'
    )

    return LaunchDescription([
        declare_ip,
        declare_range,
        declare_operation_mode,
        declare_tx_mode,
        declare_power_state,
        declare_data_topic,
        declare_compressed_topic,
        declare_frame_id,
        declare_compression_level,
        declare_reliability,
        relay_node,  # 릴레이 제어 노드를 먼저 실행
        echo_data,
        # echo_transport  # QoS 충돌로 인해 일시적으로 비활성화
    ])
