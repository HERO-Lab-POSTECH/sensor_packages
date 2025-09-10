# Adc.msg
#
# Message for raw analog data read from a DIO pin

std_msgs/Header header

# (Volts) Voltage at the output of the instrument
#
# This may differ from voltage at the input pin: if the instrument's
# output range doesn't match the ADC's range and had to be
# level-shifted, e.g. by a voltage divider, the driver will have corrected
# reported voltage for that shift.
float32 voltage

# Raw counts measured by the ADC
#
# Array, because it is common to take multiple readings and average them
# before reporting it as a single measurement.
int32[] counts

# Maximum ADC value
int32 max_counts
