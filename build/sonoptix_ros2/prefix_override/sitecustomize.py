import sys
if sys.prefix == '/usr':
    sys.real_prefix = sys.prefix
    sys.prefix = sys.exec_prefix = '/home/hero/ros2_ws/src/sensor_packages/install/sonoptix_ros2'
