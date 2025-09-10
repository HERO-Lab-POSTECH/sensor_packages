# Timestamped light brightness for all lights controlled by the driver.

std_msgs/Header header

# The names of lights that this message is reporting about.
# Valid light "names" will vary by driver.  May be semantic names,
# serial numbers, etc.
string[] lights

# Brightness as a percentage of full light level (0-100)
int8[] percent
