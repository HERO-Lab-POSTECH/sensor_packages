# Imaging Sonar Image Message

std_msgs/Header header

float32[] ranges
float32[] azimuth_angles
uint8[] intensities
uint32 data_size
bool is_bigendian
float32 frequency          # 추가: Hz
float32 azimuth_beamwidth  # 추가: rad
float32 elevation_beamwidth # 추가: rad
