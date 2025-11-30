import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import socket
import threading
import time
import re

HOST = '127.0.0.1'
PORT = 8080
CONTROL_FREQUENCY = 50
LIVENESS_TIMEOUT = 1.0

class PhoneSensorTeleopNode(Node):
    def __init__(self):
        super().__init__('phone_sensor_teleop_node')
        self._declare_parameters()
        self._load_parameters()
        self.cmd_vel_publisher_ = self.create_publisher(Twist, '/cmd_vel', 10)
        self._is_enabled = False
        self._latest_pitch = 0.0
        self._latest_roll = 0.0
        self._last_msg_time = 0.0
        self._lock = threading.Lock()
        self._last_command_log_msg = ""
        self.control_timer = self.create_timer(1.0 / CONTROL_FREQUENCY, self.control_loop_callback)
        self.get_logger().info("Phone Sensor Teleop Node has started with strafe logic.")
        self.get_logger().info(f"Loaded parameters. Listening on {HOST}:{PORT}")
        self.server_thread = threading.Thread(target=self._tcp_server_thread, daemon=True)
        self.server_thread.start()
    
    def _declare_parameters(self):
        self.declare_parameter('tuning.forward_speed', 0.5)
        self.declare_parameter('tuning.backward_speed', -0.5)
        self.declare_parameter('tuning.turn_speed', 0.8)
        # --- NEW: Declare the strafe speed parameter ---
        self.declare_parameter('tuning.strafe_speed', 0.4)
        self.declare_parameter('mapping.pitch_deadzone_min', -30.0)
        self.declare_parameter('mapping.pitch_deadzone_max', 13.0)
        self.declare_parameter('mapping.roll_deadzone_abs', 25.0)

    def _load_parameters(self):
        self.fwd_speed_ = self.get_parameter('tuning.forward_speed').get_parameter_value().double_value
        self.bwd_speed_ = self.get_parameter('tuning.backward_speed').get_parameter_value().double_value
        self.turn_speed_ = self.get_parameter('tuning.turn_speed').get_parameter_value().double_value
        # --- NEW: Load the strafe speed parameter ---
        self.strafe_speed_ = self.get_parameter('tuning.strafe_speed').get_parameter_value().double_value
        self.pitch_min_ = self.get_parameter('mapping.pitch_deadzone_min').get_parameter_value().double_value
        self.pitch_max_ = self.get_parameter('mapping.pitch_deadzone_max').get_parameter_value().double_value
        self.roll_abs_ = self.get_parameter('mapping.roll_deadzone_abs').get_parameter_value().double_value

    def _tcp_server_thread(self):
        # This function is unchanged
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind((HOST, PORT))
            s.listen()
            while rclpy.ok():
                self.get_logger().info('Server thread waiting for a connection...')
                try:
                    conn, addr = s.accept()
                    with conn:
                        self.get_logger().info(f"Connected by {addr}")
                        self._last_msg_time = time.time()
                        while rclpy.ok():
                            data_bytes = conn.recv(1024)
                            if not data_bytes:
                                self.get_logger().warn("Connection closed by client.")
                                self._last_command_log_msg = ""
                                break
                            self._parse_and_update_state(data_bytes.decode('utf-8'))
                except Exception as e:
                    self.get_logger().error(f"Server thread error: {e}")
                    time.sleep(1)

    def _parse_and_update_state(self, data_string):
        # This function is unchanged
        try:
            enabled_match = re.search(r'enabled:(\d)', data_string)
            orient_match = re.search(r'orient:[-.\d]+,pitch:([-.\d]+),roll:([-.\d]+)', data_string)
            if enabled_match and orient_match:
                with self._lock:
                    self._is_enabled = (enabled_match.group(1) == '1')
                    self._latest_pitch = float(orient_match.group(1))
                    self._latest_roll = float(orient_match.group(2))
                    self._last_msg_time = time.time()
            else:
                self.get_logger().warn(f"Could not fully parse data string: {data_string}", throttle_duration_sec=2)
        except (ValueError, IndexError) as e:
            self.get_logger().warn(f"Error parsing data '{data_string}': {e}")

    def control_loop_callback(self):
        with self._lock:
            is_enabled = self._is_enabled
            pitch = self._latest_pitch
            roll = self._latest_roll
            last_msg_time = self._last_msg_time

        is_alive = (time.time() - last_msg_time) < LIVENESS_TIMEOUT
        
        # --- MODIFIED: Add 'vy' for sideways movement ---
        vx, vy, vyaw = 0.0, 0.0, 0.0
        command_log_msg = "Stationary"

        if is_enabled and is_alive:
            is_forward = pitch > self.pitch_max_
            is_backward = pitch < self.pitch_min_
            # Rename these for clarity based on your new request
            is_left_cmd = roll > self.roll_abs_
            is_right_cmd = roll < -self.roll_abs_

            # Diagonal movements (turning while moving) are unchanged
            if is_forward and is_left_cmd:
                vx = self.fwd_speed_
                vyaw = self.turn_speed_
                command_log_msg = "Moving Forward-Left"
            elif is_forward and is_right_cmd:
                vx = self.fwd_speed_
                vyaw = -self.turn_speed_
                command_log_msg = "Moving Forward-Right"
            elif is_backward and is_left_cmd:
                vx = self.bwd_speed_
                vyaw = -self.turn_speed_ # Your original logic
                command_log_msg = "Moving Backward-Left"
            elif is_backward and is_right_cmd:
                vx = self.bwd_speed_
                vyaw = self.turn_speed_ # Your original logic
                command_log_msg = "Moving Backward-Right"
            # Straight movements
            elif is_forward:
                vx = self.fwd_speed_
                command_log_msg = "Moving Forward"
            elif is_backward:
                vx = self.bwd_speed_
                command_log_msg = "Moving Backward"
            # --- MODIFIED: Change turning to strafing ---
            elif is_left_cmd:
                vy = self.strafe_speed_  # Positive linear.y is LEFT
                command_log_msg = "Moving Left"
            elif is_right_cmd:
                vy = -self.strafe_speed_ # Negative linear.y is RIGHT
                command_log_msg = "Moving Right"
        
        if command_log_msg != self._last_command_log_msg:
            self.get_logger().info(f"New Command: {command_log_msg}")
            self._last_command_log_msg = command_log_msg
        
        cmd_msg = Twist()
        cmd_msg.linear.x = vx
        # --- MODIFIED: Use the new 'vy' variable ---
        cmd_msg.linear.y = vy
        cmd_msg.angular.z = vyaw
        self.cmd_vel_publisher_.publish(cmd_msg)

def main(args=None):
    # This function is unchanged
    rclpy.init(args=args)
    node = PhoneSensorTeleopNode()
    rclpy.spin(node)
    rclpy.shutdown()

if __name__ == '__main__':
    main()