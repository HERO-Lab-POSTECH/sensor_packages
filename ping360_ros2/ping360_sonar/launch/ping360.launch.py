from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
import os
import sys

def launch_setup(context, *args, **kwargs):
    node_type = LaunchConfiguration('node_type').perform(context)
    device = LaunchConfiguration('device').perform(context)
    baudrate = int(LaunchConfiguration('baudrate').perform(context))
    connection_type = LaunchConfiguration('connection_type').perform(context)
    udp_address = LaunchConfiguration('udp_address').perform(context)
    udp_port = int(LaunchConfiguration('udp_port').perform(context))
    
    executable = 'ping360.py' if node_type == 'python' else 'ping360_node'
    
    # 릴레이 제어 노드 추가 (CH2 for Ping360)
    from launch.actions import ExecuteProcess
    
    relay_controller_path = os.path.join(
        os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
        'relay_controller',
        'relay_node.py'
    )
    
    return [
        ExecuteProcess(
            cmd=[sys.executable, relay_controller_path,
                 '--ros-args',
                 '-p', 'channel:=2',
                 '-p', 'sensor_name:=Ping360'],
            name='relay_controller_ping360',
            output='screen'
        ),
        Node(
            package='ping360_sonar',
            executable=executable,
            name='ping360',
            parameters=[{
                'device': device,
                'baudrate': baudrate,
                'connection_type': connection_type,
                'udp_address': udp_address,
                'udp_port': udp_port
            }],
            output='screen'
        )
    ]

def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'node_type',
            default_value='cpp',
            description='Node type to launch: cpp or python'
        ),
        DeclareLaunchArgument(
            'device',
            default_value='/dev/ping360',  # Original default value from source code
            description='Serial device for Ping360 (e.g., /dev/ttyUSB0, /dev/ping360)'
        ),
        DeclareLaunchArgument(
            'baudrate',
            default_value='115200',
            description='Serial baudrate for Ping360'
        ),
        DeclareLaunchArgument(
            'connection_type',
            default_value='udp',
            description='Connection type: serial or udp'
        ),
        DeclareLaunchArgument(
            'udp_address',
            default_value='192.168.0.201',
            description='UDP address (only for udp connection)'
        ),
        DeclareLaunchArgument(
            'udp_port',
            default_value='12345',
            description='UDP port (only for udp connection)'
        ),
        OpaqueFunction(function=launch_setup)
    ]) 