# LeakVoltage.msg
#
# Data from single- or multi-channel leak detectors which measure
# an analog voltage signal.  Typically secondary to LeakDetection.msg
# which gives the main boolean "leak {is|is not} detected" status
# for each channel.
#
std_msgs/Header header

# Supply voltage for detector (typically either 5.0 or 3.3)
#
float32 max_voltage

# If true,  low  voltage is not-a-leak; high voltage is a leak
# If false, high voltage is not-a-leak; low  voltage is a leak
#
# Thresholds for not-leak/leak are assumed to be
# detector-specific
#
bool leak_is_high

# Optional ist of channel names and associated voltages.
#
# Assert:  len(channel) == 0
#  OR
# Assert:  len(channel) == len(leak_voltage)
#
string[] channel
float32[] voltage
