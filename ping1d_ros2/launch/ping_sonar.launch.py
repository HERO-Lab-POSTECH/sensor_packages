"""
Ping1D single-beam altitude sonar driver launch.

Arguments:
  device         : Serial device path                          (default: '/dev/ttyUSB0')
  speed_of_sound : Speed of sound in mm/s                      (default: 1450000)
  interval       : Ping interval in ms (50-200)                (default: 100)
  gain           : Gain setting (0-6)                          (default: 1)
  scan_start     : Scan start distance in mm (30-200)          (default: 100)
  scan_length    : Scan length in mm (2000-10000)              (default: 3000)
  mode_auto      : Auto mode: 0=manual, 1=auto                 (default: 0)
  use_rviz       : Launch RViz for visualization               (default: true)

Examples:
  ros2 launch ping1d_sonar ping_sonar.launch.py
  ros2 launch ping1d_sonar ping_sonar.launch.py device:=/dev/ping1d use_rviz:=false
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration

from launch_ros.actions import Node


def generate_launch_description():

    ping1d_node = Node(
        package='ping1d_sonar',
        executable='ping1d_node',
        output="screen",
        parameters=[{
            'port': LaunchConfiguration('device'),
            'speed': LaunchConfiguration('speed_of_sound'),
            'interval_num': LaunchConfiguration('interval'),
            'gain_num': LaunchConfiguration('gain'),
            'scan_start': LaunchConfiguration('scan_start'),
            'scan_lenght': LaunchConfiguration('scan_length'),
            'mode_auto': LaunchConfiguration('mode_auto'),
            'frame_id': LaunchConfiguration('frame_id'),
        }]
    )

    base_to_range = Node(
            ## Configure the TF of the robot to the origin of the map coordinates
            package='tf2_ros',
            executable='static_transform_publisher',
            output='screen',
            arguments=['0.0', '0.0', '0.0', '0', '0.0', '0.0', 'base_link', 'range_link']
    )

    return LaunchDescription([
        # Launch arguments
        DeclareLaunchArgument(
            'device',
            default_value='/dev/ttyUSB0',  # Original default value from source code
            description='Serial device for Ping1D (e.g., /dev/ttyUSB0, /dev/ping)'
        ),
        DeclareLaunchArgument(
            'speed_of_sound',
            default_value='1450000',
            description='Speed of sound in mm/s (default: 1450000 for water)'
        ),
        DeclareLaunchArgument(
            'interval',
            default_value='100',
            description='Ping interval in ms (50-200)'
        ),
        DeclareLaunchArgument(
            'gain',
            default_value='1',
            description='Gain setting (0-6)'
        ),
        DeclareLaunchArgument(
            'scan_start',
            default_value='100',
            description='Scan start distance in mm (30-200)'
        ),
        DeclareLaunchArgument(
            'scan_length',
            default_value='3000',
            description='Scan length in mm (2000-10000)'
        ),
        DeclareLaunchArgument(
            'mode_auto',
            default_value='0',
            description='Auto mode: 0=manual, 1=auto'
        ),
        DeclareLaunchArgument(
            'use_rviz',
            default_value='true',
            description='Launch RViz for visualization'
        ),
        DeclareLaunchArgument(
            'frame_id',
            default_value='ping1d_link',
            description='Frame ID for Ping1D sonar message headers'
        ),
        # Nodes
        ping1d_node,
        base_to_range,
    ])
