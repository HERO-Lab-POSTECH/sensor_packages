# Message for sending commands to a power management node.
# There may be a number of output pins managed by the same node.

std_msgs/Header header

int8 ON=1
int8 OFF=0

# Which channel(s) this message is commanding
string[] channels

# Whether the corresponding pin should be ON/OFF
# (using int8 to enable using constants; rosmsg doesn't allow `bool ON=true`)
# len(channels) == len(commands)
int8[] commands
