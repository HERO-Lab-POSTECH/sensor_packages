# Instruct a light driver to set the output level of multiple
# lights.

std_msgs/Header header

# Which channel(s) this message is commanding.
# Valid light "names" will vary by driver.  May be semantic names,
# serial numbers, etc.
# It is common (but not requried) for drivers to support "all" for
# a command to effect all lights, in which case the convention is
# for len(percent) == 1
string[] lights

# Brightness as a percentage of full light level (0-100)
#
# If len(channels) != len(percent) the packet is
# invalid and should be ignored
int8[] percent
