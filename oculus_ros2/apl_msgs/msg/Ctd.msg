# Message for data from a CTD (conductivity, temperature, depth) sensor
#
# The fields match those provided by the Remus Recon protocol, as well
# as those in WHOI's ds_sensor_msgs/Ctd.msg (but without the covariances),
# though the units for pressure are different.

# For some reason, NO_DATA gave me compile errors in the msg_parser.
# TODO: Figure out why, and go back.
float64 CTD_NO_DATA=-9999.0

std_msgs/Header header

# Conductivity: S/m
float64 conductivity

# Temperature: degrees C
float64 temperature

# Pressure: in Pascals
#
# ROS messages expect SI-derived units, which for pressure is 1 Pa = 1 N/m^2
# It is common for marine sensors to use decibar, which is convenient
# since 1 dbar ~= pressure under 1 m of water.
# 1 decibar = 1e4 Pascals
float64 pressure

# Salinity: PSU, aka parts per thousand
float64 salinity

# Speed of Sound: m/s
float64 sound_speed
