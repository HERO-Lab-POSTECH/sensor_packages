import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Image, PointCloud2
from std_msgs.msg import Header
from sensor_msgs_py import point_cloud2
import numpy as np
import math

class Ping360PointCloudFromImage(Node):
    def __init__(self):
        super().__init__('ping360_pointcloud_from_image')

        # Declare parameters for topic names and frame_id
        self.declare_parameter('input_topic', '/sensor/sonar/ping360/image')
        self.declare_parameter('output_topic', '/sensor/sonar/ping360/pointcloud')
        self.declare_parameter('frame_id', 'ping360_link')

        input_topic = self.get_parameter('input_topic').value
        output_topic = self.get_parameter('output_topic').value
        self.frame_id = self.get_parameter('frame_id').value

        # Use sensor QoS for consistency across packages
        self.subscription = self.create_subscription(
            Image,
            input_topic,
            self.image_callback,
            qos_profile_sensor_data
        )

        self.publisher = self.create_publisher(
            PointCloud2,
            output_topic,
            qos_profile_sensor_data
        )

        # ping360 설정 (예시값)
        self.num_beams = 500       # 각도 해상도 (500줄)
        self.num_bins = 500        # 거리 해상도 (500칸)
        self.angle_start = 0.0     # 시작 각도 [rad]
        self.angle_end = 2 * math.pi  # 끝 각도 [rad]
        self.max_range = 10.0      # 최대 탐지 거리 (m)

    def image_callback(self, msg: Image):
        img = np.array(msg.data, dtype=np.uint8).reshape((msg.height, msg.width))

        points = []
        for r in range(self.num_bins):
            for t in range(self.num_beams):
                intensity = img[r, t]
                if intensity < 10:
                    continue  # 너무 약한 반사는 무시

                # 거리 및 각도 계산
                distance = (r / self.num_bins) * self.max_range
                angle = self.angle_start + (t / self.num_beams) * (self.angle_end - self.angle_start)

                x = distance * math.cos(angle)
                y = distance * math.sin(angle)
                z = 0.0
                points.append((x, y, z))

        # PointCloud2 메시지 생성
        header = Header()
        header.stamp = self.get_clock().now().to_msg()
        header.frame_id = self.frame_id

        pc2_msg = point_cloud2.create_cloud_xyz32(header, points)
        self.publisher.publish(pc2_msg)

def main(args=None):
    rclpy.init(args=args)
    node = Ping360PointCloudFromImage()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()
    
    

if __name__ == '__main__':
    main()

