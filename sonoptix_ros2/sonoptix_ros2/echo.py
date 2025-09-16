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
from rcl_interfaces.msg import SetParametersResult, ParameterDescriptor, IntegerRange, FloatingPointRange
from rclpy.qos_overriding_options import QoSOverridingOptions

import cv2
from cv_bridge import CvBridge
from sensor_msgs.msg import Image
from std_msgs.msg import Float32, Int32, String, Bool

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
        # Declare parameters with descriptors for range validation
        
        # Integer parameters with ranges
        self.declare_parameter('range', 3, 
            ParameterDescriptor(
                description='Sonar range in meters',
                integer_range=[IntegerRange(from_value=3, to_value=100, step=1)]))
        
        self.declare_parameter('operation_mode', 0,
            ParameterDescriptor(
                description='Operation mode: 0=400kHz, 1=700kHz',
                integer_range=[IntegerRange(from_value=0, to_value=1, step=1)]))
        
        self.declare_parameter('trigger_mode', 0,
            ParameterDescriptor(
                description='Trigger mode: 0=Auto, 1=Software, 2=External',
                integer_range=[IntegerRange(from_value=0, to_value=2, step=1)]))
        
        self.declare_parameter('colormap_index', 1,
            ParameterDescriptor(
                description='Colormap: 0=gray, 1=amber, 2=ironbow, 3=deep, 4=rainbow',
                integer_range=[IntegerRange(from_value=0, to_value=4, step=1)]))
        
        # Float parameters with ranges
        self.declare_parameter('gain', 10.0,
            ParameterDescriptor(
                description='Image gain',
                floating_point_range=[FloatingPointRange(from_value=0.0, to_value=100.0, step=0.1)]))
        
        self.declare_parameter('contrast', 0.0,
            ParameterDescriptor(
                description='Image contrast',
                floating_point_range=[FloatingPointRange(from_value=0.0, to_value=1.0, step=0.01)]))
        
        # String and other parameters without ranges
        self.declare_parameter('ip', '192.168.0.203',
            ParameterDescriptor(description='Sonar IP address'))
        self.declare_parameter('tx_mode', 'auto',
            ParameterDescriptor(description='Transmission mode'))
        self.declare_parameter('power_state', True,
            ParameterDescriptor(description='Sonar power state'))
        self.declare_parameter('topic', '/sensor/sonar/sonoptix/data',
            ParameterDescriptor(description='Output topic name'))
        self.declare_parameter('frame_id', 'echo',
            ParameterDescriptor(description='Frame ID for messages'))
        
        # Get all parameter values
        self.range = self.get_parameter('range').value
        self.ip = self.get_parameter('ip').value
        self.tx_mode = self.get_parameter('tx_mode').value
        self.power_state = self.get_parameter('power_state').value
        self.operation_mode = self.get_parameter('operation_mode').value
        self.trigger_mode = self.get_parameter('trigger_mode').value
        self.gain = self.get_parameter('gain').value
        self.contrast = self.get_parameter('contrast').value
        self.colormap_index = self.get_parameter('colormap_index').value
        self.topic = self.get_parameter('topic').value
        self.frame_id = self.get_parameter('frame_id').value
        
        # Log all parameters
        param_names = ['range', 'ip', 'tx_mode', 'power_state', 'operation_mode', 
                       'trigger_mode', 'gain', 'contrast', 'colormap_index', 'topic', 'frame_id']
        params = self.get_parameters(param_names)
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
        self.api_url = f'http://{self.ip}:8000/api/v2'

        self.get_logger().info(f'Configuring Sonar')

        # Set the sonar range, tx_mode, and enable the transceiver with retry logic
        max_retries = 60  # 60초 동안 재시도
        retry_delay = 1.0  # 1초마다 재시도
        
        for attempt in range(max_retries):
            try:
                # API v2 uses transceiver endpoint with different format
                power_state_str = 'on' if self.power_state else 'off'
                transceiver_data = {
                    "power_state": power_state_str,
                    "range": int(self.range),
                    "tx_mode": self.tx_mode
                }
                
                # TODO: API v2 파라미터 지원 확인 필요
                # v2 API에서 아래 파라미터들이 지원되면 transceiver_data에 추가
                # "trigger_mode": self.trigger_mode,
                # "operation_mode": self.operation_mode
                
                response = requests.put(self.api_url + '/transceiver',
                             json=transceiver_data,
                             timeout=2.0)
                
                if response.status_code == 200:
                    self.get_logger().info('Successfully configured sonar transceiver')
                    break
                else:
                    self.get_logger().warn(f'API returned status {response.status_code}, retrying...')
                    
            except (requests.exceptions.ConnectionError, requests.exceptions.Timeout) as e:
                # Check if it's a retryable error (sonar/network is booting) 
                error_str = str(e)
                if ("Connection refused" in error_str or 
                    "ConnectionRefusedError" in error_str or
                    "timed out" in error_str or
                    "ConnectTimeoutError" in error_str or
                    "No route to host" in error_str):  # Network may still be initializing
                    # Sonar is likely booting up or network is initializing, keep retrying
                    if attempt == 0:
                        self.get_logger().info(f'Waiting for sonar API at {self.api_url} (sonar/network may be initializing)...')
                    elif attempt % 10 == 0:
                        self.get_logger().info(f'Still waiting for sonar API... ({attempt}s elapsed)')
                    
                    if attempt == max_retries - 1:
                        self.get_logger().error(f'Failed to connect to sonar API after {max_retries} seconds')
                        raise
                    
                    rclpy.spin_once(self, timeout_sec=retry_delay)
                    continue
                else:
                    # Other permanent connection errors (e.g., DNS failure, invalid hostname)
                    self.get_logger().error(f'Failed to connect to sonar API: {error_str}')
                    self.get_logger().error(f'Please check the IP address: {self.ip}')
                    raise

        # Set the data stream type to RTSP (API v2 requirement)
        try:
            stream_response = requests.put(self.api_url + '/datastream',
                                         json={"stream_type": 'rtsp'},
                                         timeout=2.0)
            if stream_response.status_code == 200:
                self.get_logger().info('Data stream set to RTSP mode')
            else:
                self.get_logger().warn(f'Failed to set data stream: {stream_response.status_code}')
        except Exception as e:
            self.get_logger().warn(f'Failed to set data stream: {e}')

        # TODO: API v2 config/colormap 파라미터 지원 확인 필요
        # v2 API에서 gain, contrast, colormap이 지원되면 아래 주석을 해제하고
        # 적절한 엔드포인트로 수정하여 사용
        
        # try:
        #     # Set gain and contrast
        #     self.get_logger().info(f'Initial config: gain={self.gain}, contrast={self.contrast}')
        #     config_response = requests.patch(self.api_url + '/config',  # v2 엔드포인트 확인 필요
        #                                    json={
        #                                        "gain": float(self.gain),
        #                                        "contrast": float(self.contrast)
        #                                    },
        #                                    timeout=2.0)
        #     if config_response.status_code == 200:
        #         result = config_response.json()
        #         self.get_logger().info(f'Config set successfully. Server returned: {result}')
        #     else:
        #         self.get_logger().error(f'Config update failed: {config_response.status_code} - {config_response.text}')
        #     
        #     # Set colormap
        #     self.get_logger().info(f'Setting colormap index to {self.colormap_index}')
        #     colormap_response = requests.put(self.api_url + '/colormap/index',  # v2 엔드포인트 확인 필요
        #                                    json={"value": self.colormap_index},
        #                                    timeout=2.0)
        #     if colormap_response.status_code == 200:
        #         colormap_names = ['gray', 'amber', 'ironbow', 'deep', 'rainbow']
        #         result = colormap_response.json()
        #         self.get_logger().info(f'Set colormap to {colormap_names[self.colormap_index]}. Server returned: {result}')
        #     else:
        #         self.get_logger().error(f'Colormap update failed: {colormap_response.status_code} - {colormap_response.text}')
        # except Exception as e:
        #     self.get_logger().warn(f'Failed to set config parameters: {e}')
        
        # 현재는 config 파라미터 설정을 건너뜀 (API v2에서 지원 확인 필요)
        self.get_logger().info('Skipping config/colormap settings (TODO: check API v2 support)')
        
        # Note: RTSP stream is automatically available when transponder is enabled

        # Initialize CV bridge, video capture, and ros2 publisher
        self.br = CvBridge()
        self.publisher = self.create_publisher(Image, self.topic, SENSOR_QOS,
                                               qos_overriding_options=qos_override_opts)
        
        # Parameter publishers for recording
        self.range_pub = self.create_publisher(Float32, '/sensor/sonar/sonoptix/param/range', 10)
        self.ip_pub = self.create_publisher(String, '/sensor/sonar/sonoptix/param/ip', 10)
        self.tx_mode_pub = self.create_publisher(String, '/sensor/sonar/sonoptix/param/tx_mode', 10)
        self.power_state_pub = self.create_publisher(Bool, '/sensor/sonar/sonoptix/param/power_state', 10)
        self.operation_mode_pub = self.create_publisher(Int32, '/sensor/sonar/sonoptix/param/operation_mode', 10)
        self.trigger_mode_pub = self.create_publisher(Int32, '/sensor/sonar/sonoptix/param/trigger_mode', 10)
        self.gain_pub = self.create_publisher(Float32, '/sensor/sonar/sonoptix/param/gain', 10)
        self.contrast_pub = self.create_publisher(Float32, '/sensor/sonar/sonoptix/param/contrast', 10)
        self.colormap_index_pub = self.create_publisher(Int32, '/sensor/sonar/sonoptix/param/colormap_index', 10)
        self.frame_id_pub = self.create_publisher(String, '/sensor/sonar/sonoptix/param/frame_id', 10)
        
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
                
                # Publish parameters for recording
                range_msg = Float32()
                range_msg.data = float(self.range)
                self.range_pub.publish(range_msg)
                
                ip_msg = String()
                ip_msg.data = self.ip
                self.ip_pub.publish(ip_msg)
                
                tx_mode_msg = String()
                tx_mode_msg.data = self.tx_mode
                self.tx_mode_pub.publish(tx_mode_msg)
                
                power_state_msg = Bool()
                power_state_msg.data = self.power_state
                self.power_state_pub.publish(power_state_msg)
                
                operation_mode_msg = Int32()
                operation_mode_msg.data = self.operation_mode
                self.operation_mode_pub.publish(operation_mode_msg)
                
                trigger_mode_msg = Int32()
                trigger_mode_msg.data = self.trigger_mode
                self.trigger_mode_pub.publish(trigger_mode_msg)
                
                gain_msg = Float32()
                gain_msg.data = float(self.gain)
                self.gain_pub.publish(gain_msg)
                
                contrast_msg = Float32()
                contrast_msg.data = float(self.contrast)
                self.contrast_pub.publish(contrast_msg)
                
                colormap_index_msg = Int32()
                colormap_index_msg.data = self.colormap_index
                self.colormap_index_pub.publish(colormap_index_msg)
                
                frame_id_msg = String()
                frame_id_msg.data = self.frame_id
                self.frame_id_pub.publish(frame_id_msg)

                # Allow for params callback to be processed
                rclpy.spin_once(self, timeout_sec=0.01)
        finally:
            # Stop the transceiver before destroying the node
            requests.put(self.api_url + '/transceiver', json={"power_state": 'off'})
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
            if param.name in ['range', 'power_state', 'tx_mode']:
                power_state_str = 'on' if self.power_state else 'off'
                transceiver_data = {
                    "power_state": power_state_str,
                    "range": int(self.range),
                    "tx_mode": self.tx_mode
                }
                
                # TODO: API v2 파라미터 지원 확인 필요
                # v2 API에서 operation_mode, trigger_mode가 지원되면
                # 위의 if 조건에 'operation_mode', 'trigger_mode'를 추가하고
                # transceiver_data에 해당 필드 추가
                
                requests.put(self.api_url + '/transceiver',
                             json=transceiver_data)
                
                # Range 변경 시 RTSP 스트림 재연결
                if param.name == 'range':
                    self.get_logger().info(f'Reconnecting RTSP stream for new range setting')
                    self.connect_rtsp()
            
            # TODO: operation_mode, trigger_mode 파라미터 처리
            # v2 API에서 지원하면 위의 transceiver 업데이트에 포함
            if param.name in ['operation_mode', 'trigger_mode']:
                self.get_logger().info(f'Parameter {param.name} updated but not sent to API v2 (TODO: check support)')
            
            # TODO: API v2 config/colormap 파라미터 지원 확인 필요
            # v2 API에서 gain, contrast, colormap이 지원되면 아래 주석을 해제하고
            # 적절한 엔드포인트로 수정하여 사용
            
            # Configure image parameters
            if param.name in ['gain', 'contrast']:
                self.get_logger().info(f'Parameter {param.name} updated to {param.value} but not sent to API v2 (TODO: check support)')
                # try:
                #     # API 호출 전 현재 값 로깅
                #     self.get_logger().info(f'Sending config update: gain={self.gain}, contrast={self.contrast}')
                #     
                #     response = requests.patch(self.api_url + '/config',  # v2 엔드포인트 확인 필요
                #                  json={
                #                     "gain": float(self.gain),
                #                     "contrast": float(self.contrast)
                #                  },
                #                  timeout=2.0)
                #     
                #     if response.status_code == 200:
                #         # 응답 확인
                #         result_data = response.json()
                #         self.get_logger().info(f'Config update successful. Server returned: {result_data}')
                #     else:
                #         self.get_logger().error(f'Config update failed with status {response.status_code}: {response.text}')
                #         
                # except Exception as e:
                #     self.get_logger().error(f'Failed to update config: {e}')
            
            # Configure colormap
            if param.name == 'colormap_index':
                self.get_logger().info(f'Parameter colormap_index updated to {param.value} but not sent to API v2 (TODO: check support)')
                # try:
                #     self.get_logger().info(f'Sending colormap index: {self.colormap_index}')
                #     
                #     response = requests.put(self.api_url + '/colormap/index',  # v2 엔드포인트 확인 필요
                #                json={"value": self.colormap_index},
                #                timeout=2.0)
                #     
                #     if response.status_code == 200:
                #         colormap_names = ['gray', 'amber', 'ironbow', 'deep', 'rainbow']
                #         result_data = response.json()
                #         self.get_logger().info(f'Set colormap to {colormap_names[self.colormap_index]}. Server returned: {result_data}')
                #     else:
                #         self.get_logger().error(f'Colormap update failed with status {response.status_code}: {response.text}')
                #         
                # except Exception as e:
                #     self.get_logger().error(f'Failed to update colormap: {e}')
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
