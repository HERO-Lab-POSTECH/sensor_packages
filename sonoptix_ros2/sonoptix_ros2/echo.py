#! /usr/bin/env python3

#-----------------------------------------------------------------------------------
# MIT License

# Copyright (c) 2025 ItsKalvik

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#-----------------------------------------------------------------------------------

"""
This script is a ROS 2 node that interfaces with a Sonoptix sonar.
It captures data from the sonar, processes it, and publishes it as a ROS 2 Image message.
The node also provides a service to control the sonar's power state, range, and other parameters.
"""

import rclpy
from rclpy import qos
from rclpy.node import Node
from rcl_interfaces.msg import SetParametersResult
from rclpy.qos_overriding_options import QoSOverridingOptions

import cv2
from cv_bridge import CvBridge
from sensor_msgs.msg import Image

import requests


class EchoNode(Node):
    """
    The main class for the sonar node.
    """
    def __init__(self, node_name='echo'):
        """
        Initializes the node, parameters, publisher, and services.
        """
        super().__init__(node_name)

        # --- Parameters ---
        # Declare and get parameters for the node.
        params = {
            'range': [3, int],  # 기본 3m (최소값이 3m인 것 같음)
            'ip': ['192.168.0.203', str],  # 현재 센서 IP로 업데이트
            'tx_mode': ['auto', str],
            'power_state': [True, bool],
            'operation_mode': [0, int],  # 0: 400kHz, 1: 700kHz (추정)
            'topic': ['/sensor/sonar/sonoptix/data', str],
            'frame_id': ['echo', str],
        }

        for param, [value, dtype] in params.items():
            self.declare_parameter(param, value)
            exec(f"self.{param}:dtype = self.get_parameter(param).value")
        params = self.get_parameters(params.keys())
        for param in params:
            self.get_logger().info(f'{param.name}: {param.value}')

        qos_override_opts = QoSOverridingOptions(
            policy_kinds=(
                qos.QoSPolicyKind.HISTORY,
                qos.QoSPolicyKind.DEPTH,
                qos.QoSPolicyKind.RELIABILITY,
                )
        )
        # Use BEST_EFFORT QoS to match image_transport republish
        SENSOR_QOS = qos.QoSProfile(
            reliability=qos.ReliabilityPolicy.BEST_EFFORT,
            durability=qos.DurabilityPolicy.VOLATILE,
            depth=1
        )

        # Handle parameter updates
        self.param_handler_ptr_ = self.add_on_set_parameters_callback(
            self.set_param_callback)

        self.rtsp_url = f'rtsp://{self.ip}:8554/raw'
        self.api_url = f'http://{self.ip}:8000/api/v1'

        self.get_logger().info(f'Configuring Sonar')

        # Set the sonar range, tx_mode, and enable the transponder with retry logic
        state = 'on' if self.power_state else 'off'
        max_retries = 60  # 60초 동안 재시도
        retry_delay = 1.0  # 1초마다 재시도
        
        for attempt in range(max_retries):
            try:
                response = requests.patch(self.api_url + '/transponder',
                             json={
                                   "enable": self.power_state,
                                   "sonar_range": float(self.range),
                                   "trigger_mode": 0,  # 0: Auto (continuous), 1: Software, 2: External
                                   "operation_mode": self.operation_mode
                               },
                             timeout=2.0)
                
                if response.status_code == 200:
                    self.get_logger().info('Successfully configured sonar transponder')
                    break
                else:
                    self.get_logger().warn(f'API returned status {response.status_code}, retrying...')
                    
            except (requests.exceptions.ConnectionError, requests.exceptions.Timeout) as e:
                # Check if it's "Connection refused" (sonar is booting) vs other errors
                error_str = str(e)
                if "Connection refused" in error_str or "ConnectionRefusedError" in error_str:
                    # Sonar is likely booting up, keep retrying
                    if attempt == 0:
                        self.get_logger().info(f'Waiting for sonar API at {self.api_url} (sonar may be booting)...')
                    elif attempt % 10 == 0:
                        self.get_logger().info(f'Still waiting for sonar API... ({attempt}s elapsed)')
                    
                    if attempt == max_retries - 1:
                        self.get_logger().error(f'Failed to connect to sonar API after {max_retries} seconds')
                        raise
                    
                    rclpy.spin_once(self, timeout_sec=retry_delay)
                    continue
                else:
                    # Other connection errors (e.g., no route to host, wrong IP)
                    self.get_logger().error(f'Failed to connect to sonar API: {error_str}')
                    self.get_logger().error(f'Please check the IP address: {self.ip}')
                    raise

        # Note: RTSP stream is automatically available when transponder is enabled

        # Initialize CV bridge, video capture, and ros2 publisher
        self.br = CvBridge()
        self.publisher = self.create_publisher(Image, self.topic, SENSOR_QOS,
                                               qos_overriding_options=qos_override_opts)
        
        # RTSP 연결 재시도 로직
        self.cap = None
        self.connect_rtsp(initial=True)

        # Read and publish sonar data when available
        self.last_frame_time = self.get_clock().now()
        reconnect_timeout = 5.0  # 5초 동안 프레임이 없으면 재연결
        
        try:
            while True:
                if not self.power_state:
                    rclpy.spin_once(self, timeout_sec=1.0)
                    continue
                
                # RTSP 연결 상태 확인
                if self.cap is None or not self.cap.isOpened():
                    self.get_logger().warn('RTSP disconnected, attempting to reconnect...')
                    if not self.connect_rtsp():
                        rclpy.spin_once(self, timeout_sec=5.0)
                        continue
                
                ret, frame = self.cap.read()
                if not ret:
                    # 프레임 읽기 실패 - 타임아웃 체크
                    current_time = self.get_clock().now()
                    time_diff = (current_time - self.last_frame_time).nanoseconds / 1e9
                    
                    if time_diff > reconnect_timeout:
                        self.get_logger().warn(f'No frames for {time_diff:.1f}s, reconnecting RTSP...')
                        self.connect_rtsp()
                        self.last_frame_time = current_time
                    continue
                
                # 프레임 수신 성공
                self.last_frame_time = self.get_clock().now()
                
                # 프레임 정보 로깅 (디버깅용 - 나중에 제거 가능)
                if not hasattr(self, 'last_shape') or self.last_shape != frame.shape:
                    self.get_logger().info(f'Frame shape changed: {frame.shape}')
                    self.last_shape = frame.shape
                    
                # Raw data 보존 - 필터링 없이 원본 그대로
                # 프레임이 이미 grayscale인지 확인
                if len(frame.shape) == 3 and frame.shape[2] == 3:
                    try:
                        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                    except Exception as e:
                        self.get_logger().error(f'Color conversion failed: {e}')
                        continue
                elif len(frame.shape) == 2:
                    pass  # Already grayscale
                else:
                    self.get_logger().warn(f'Unexpected frame shape: {frame.shape}')
                    continue
                
                # 범위 정보를 메타데이터로 저장
                frame[0, 0] = self.range
                
                # Raw data 그대로 퍼블리시
                frame = self.br.cv2_to_imgmsg(frame, encoding='mono8')
                frame.header.stamp = self.get_clock().now().to_msg()
                frame.header.frame_id = self.frame_id

                self.publisher.publish(frame)

                # Allow for params callback to be processed
                rclpy.spin_once(self, timeout_sec=0.01)
        finally:
            # Stop the transponder before destroying the node
            requests.patch(self.api_url + '/transponder', json={"enable": False})
            self.get_logger().info(f'Transceiver disabled')
            self.destroy_node()
            rclpy.shutdown()

    def connect_rtsp(self, initial=False):
        """RTSP 스트림 연결"""
        try:
            self.get_logger().info(f'Accessing RTSP stream')
            if self.cap is not None:
                self.cap.release()
                
            # 기본 백엔드 사용
            self.cap = cv2.VideoCapture(self.rtsp_url)
            self.cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
            # 연결 타임아웃 설정 시도
            self.cap.set(cv2.CAP_PROP_OPEN_TIMEOUT_MSEC, 10000)  # 10초 타임아웃
            self.cap.set(cv2.CAP_PROP_READ_TIMEOUT_MSEC, 10000)
            
            # 짧은 대기 후 연결 확인
            rclpy.spin_once(self, timeout_sec=1.0)
            
            # 연결 확인 - 실제 프레임을 읽어봄
            for _ in range(3):  # 3번 시도
                ret, test_frame = self.cap.read()
                if ret and test_frame is not None:
                    self.get_logger().info('RTSP stream connected successfully')
                    self.last_frame_time = self.get_clock().now()
                    return True
                rclpy.spin_once(self, timeout_sec=0.5)
                
            self.get_logger().warn('RTSP connected but no frames received')
                
        except Exception as e:
            self.get_logger().error(f'RTSP connection error: {e}')
        
        if initial:
            self.get_logger().error('Failed to connect to RTSP stream')
        return False
    
    def set_param_callback(self, params):
        """
        This function is the callback for when parameters are changed.
        It updates the node's attributes and sends the new settings to the sonar.
        """
        result = SetParametersResult(successful=True)
        for param in params:
            # QoS setting are handled by QoSOverridingOptions
            if "qos" in param.name:
                continue
            exec(f"self.flag = self.{param.name} != param.value")
            if self.flag:
                exec(f"self.{param.name} = param.value")
                self.get_logger().info(f'Updated {param.name}: {param.value}')

            # Configure sonar if necessary
            if param.name in ['range', 'power_state', 'operation_mode']:
                # Sonoptix Echo는 3m 미만에서 RTSP 프레임을 보내지 않음
                if param.name == 'range' and self.range < 3:
                    self.get_logger().warn(f'Range {self.range}m is below minimum (3m). Setting to 3m.')
                    self.range = 3
                    
                requests.patch(self.api_url + '/transponder',
                             json={
                                "enable": self.power_state,
                                "sonar_range": float(self.range),
                                "operation_mode": self.operation_mode
                             })
                # Range 또는 operation_mode 변경 시 RTSP 스트림 재연결
                if param.name in ['range', 'operation_mode']:
                    self.get_logger().info(f'Reconnecting RTSP stream for new settings')
                    self.connect_rtsp()
        return result


def main(args=None):
    """
    The main function to run the node.
    """
    rclpy.init(args=args)
    echo = EchoNode()
    rclpy.spin(echo)
    echo.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
