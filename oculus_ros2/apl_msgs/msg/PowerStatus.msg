# Message for power management node to report current status
# of all controlled pins.

std_msgs/Header header

int8 ON=1
int8 OFF=0

# All arrays must be supplied:
#    len(channels) == len(pins) == len(status)

# Descriptive name of each controlled channel.
string[] channels

# Pin number; exact meaning is up to the driver.
#
# On single-board computers, the physical pin number on the 40-pin header
# will differ from the pin number used by e.g. wiringPi for accessing the pins.
int8[] pins

# Pin status
#
# This is an int8 so it can be used with a constant;
# rosmsg doesn't allow `bool ON=true`
int8[] status
