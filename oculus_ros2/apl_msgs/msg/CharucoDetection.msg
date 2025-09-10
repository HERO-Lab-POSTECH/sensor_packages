# Results of OpenCV's charuco detection, so we can log positions
# without logging entire images.

# Header should be copied directly from source image
std_msgs/Header header

# APL-specific charuco board name, as defined in charuco_utils
string board_name

# Include info on which tags were detected.
# Since this isn't paired with an actual camera image, we only include tags,
# not actual image coorinates of their corners.
int32[] aruco_ids
int32[] charuco_ids

# Vector and rotation from camera origin to charuco board
# These are the output of cv2.aruco.estimatePoseCharucoBoard
# Will be empty if no board was detected.
float64[] rvec
float64[] tvec
