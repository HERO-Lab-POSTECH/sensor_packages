# name : Should contain one or more axis names (as defined by the
# recipient).  Can be only a subset of a driver's defined axes.
#
# position : Must contain the same number of entries as "name"
# Specifies commanded position for each listed axis in the same order.
# Position is in radians or m as appropriate for the axis
#
# If len(name) != len(position), message is ignored
# If any of the names are unrecognized, the whole message is ignored

string[] name
float64[] position
