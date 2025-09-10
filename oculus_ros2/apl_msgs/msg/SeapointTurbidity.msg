# Turbidity reading from Seapoint turbidity sensor,
# including sensor-specific meta-information
#
# See also the Seapoint turbidity sensor manual:
#    http://www.seapoint.com/pdf/stm_um.pdf
std_msgs/Header header

# Voltage output from Seapoint ( 0.0 - 5.0V )
float32 voltage

# Digital control signals which set sensor sensitivity
# (see max_turbidity and ntu below)
bool gain_a
bool gain_b

# Max value for Seapoint's current sensitivity setting
# (configured by gain[A,B] above)
#
# Included redundantly with the gain[A,B] settings above to
# allow both re-calculation of NTU from voltage without
# encoding the truth table and to allow
# re-calculation of max_turbidity if a driver's
# truth table is somehow incorrect.
#
#  gain A      gain B      max_turbidity       sensitivity
#    +5V          +5V          25 NTU            5 NTU/V
#    +5            0          125 NTU           25 NTU/V
#     0           +5          500 NTU          100 NTU/V
#     0            0         4000 NTU          500 NTU/V**
#
# ** the sensor is non-linear above 1250 NTU
#
uint32 max_turbidity

# NTU value calculated as:
#
#   ntu = voltage * sensitivity
#
# where sensitivity is defined above
float32 ntu
