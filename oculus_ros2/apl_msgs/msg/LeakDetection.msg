# LeakDetection.msg
#
# Message for leak detection coming from one or more digital I/O pins
# The driver will be configured to determine whether leak is high/low

std_msgs/Header header

# Optional name distinguishing each leak sensor
string[] channel
# Whether a leak is detected
bool[] leak
