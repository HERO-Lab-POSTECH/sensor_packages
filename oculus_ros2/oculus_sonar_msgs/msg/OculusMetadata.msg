# OculusMetadata.msg
# Date: 250721
# Author: seungmin kim
# Description: Metadata message for Oculus sonar, including header and time-varying gain (TVG) values.

std_msgs/Header header   # Standard ROS2 message header
float32[] tvg           # Time-varying gain (TVG) values