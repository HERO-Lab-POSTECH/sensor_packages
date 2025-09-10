# apl_msgs

Messages (and related utilities) that are used across multiple nodes and don't
fit anywhere else.

This package is a hybrid ROS1/ROS2 package and should build in both environments with catkin/colcon resp.

Tested in: noetic, humble, jazzy, rolling.


# RawData message type

A message type for publishing raw binary data, typically used to record raw, unparsed binary interchange with network sockets, serial ports, etc.

## Best practices

* RawData should be published on a topic called `raw_data` which exists as a top-level topic alongside other processed sensor outputs from a given driver.
* RawData should include **all** binary data, including unparsed or dropped packets.
* The origin of a given RawData stream is encoded in the topic name (e.g. `/oculus/raw_data` is the RawData from the Oculus driver)
* The header `frame_id` is optional -- it shouldn't be required to contextualize the data (the topic name does this)
* The header timestamp is assumed to fall immediately **after** the tranmission/reception of the data.
