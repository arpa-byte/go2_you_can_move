import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from visualization_msgs.msg import Marker
import math
import threading
import sys
import tty
import termios

ESTOP_KEYCODE = ' ' # Spacebar
START_KEYCODE = '\n' # Enter key

class CoordinateNavigatorNode(Node):
    def __init__(self):
        super().__init__('coordinate_navigator_node')

        self._declare_parameters()
        self._load_parameters()

        # --- State Machine ---
        # New initial state
        self.state = 'WAITING_FOR_START'
        self.current_x = 0.0
        self.current_y = 0.0
        self.current_yaw = 0.0
        self.odom_received = False

        # --- Flags ---
        self.emergency_stop_active = False
        self.start_command_received = False
        self.lock = threading.Lock()

        # --- Threading ---
        self.keyboard_thread = threading.Thread(target=self.keyboard_listener)
        self.keyboard_thread.daemon = True
        self.keyboard_thread.start()

        # --- Publishers & Subscribers ---
        self.publisher_ = self.create_publisher(Twist, '/cmd_vel', 10)
        # Use a "latched" QoS profile or just repeat publish for markers
        self.marker_pub_ = self.create_publisher(Marker, '/visualization_marker', 10)
        self.odom_subscriber_ = self.create_subscription(
            Odometry, '/odom', self.odom_callback, 10)
        
        self.timer_ = self.create_timer(0.1, self.control_loop)

        self.get_logger().info(f"Target Goal: ({self.goal_x_:.2f}, {self.goal_y_:.2f})")
        self.get_logger().info(">>> PRESS 'ENTER' TO START <<<")
        self.get_logger().info(">>> PRESS 'SPACEBAR' FOR E-STOP <<<")

    def _declare_parameters(self):
        self.declare_parameter('goal_x', 0.0)
        self.declare_parameter('goal_y', 0.0)
        self.declare_parameter('linear_speed', 0.4)
        self.declare_parameter('angular_speed', 0.5)
        self.declare_parameter('distance_tolerance', 0.1)
        self.declare_parameter('angle_tolerance', 0.05)

    def _load_parameters(self):
        self.goal_x_ = self.get_parameter('goal_x').get_parameter_value().double_value
        self.goal_y_ = self.get_parameter('goal_y').get_parameter_value().double_value
        self.linear_speed_ = self.get_parameter('linear_speed').get_parameter_value().double_value
        self.angular_speed_ = self.get_parameter('angular_speed').get_parameter_value().double_value
        self.dist_tolerance_ = self.get_parameter('distance_tolerance').get_parameter_value().double_value
        self.angle_tolerance_ = self.get_parameter('angle_tolerance').get_parameter_value().double_value

    def keyboard_listener(self):
        old_settings = termios.tcgetattr(sys.stdin)
        try:
            tty.setcbreak(sys.stdin.fileno())
            while rclpy.ok():
                char = sys.stdin.read(1)
                with self.lock:
                    if char == ESTOP_KEYCODE:
                        if not self.emergency_stop_active:
                            self.emergency_stop_active = True
                            self.get_logger().warn('!!! EMERGENCY STOP ACTIVATED !!!')
                            self.publisher_.publish(Twist())
                    elif char == START_KEYCODE:
                        if not self.start_command_received and not self.emergency_stop_active:
                            self.start_command_received = True
                            self.get_logger().info('Start command received. Navigation beginning...')
        finally:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)

    def odom_callback(self, msg: Odometry):
        self.current_x = msg.pose.pose.position.x
        self.current_y = msg.pose.pose.position.y
        self.current_yaw = self.euler_from_quaternion(msg.pose.pose.orientation)
        self.odom_received = True

    def control_loop(self):
        # Continuously publish the marker so it appears in RViz whenever it opens
        self.publish_goal_marker()

        with self.lock:
            if self.emergency_stop_active:
                self.publisher_.publish(Twist())
                return
            # Check if we are allowed to start
            if self.state == 'WAITING_FOR_START':
                if self.start_command_received:
                    self.state = 'TURNING_TO_GOAL'
                else:
                    return # Do nothing until start command

        if not self.odom_received:
            self.get_logger().info('Waiting for odometry data...', throttle_duration_sec=2)
            return

        msg = Twist()
        dx = self.goal_x_ - self.current_x
        dy = self.goal_y_ - self.current_y
        distance_to_goal = math.sqrt(dx**2 + dy**2)
        
        if self.state == 'GOAL_REACHED':
            self.publisher_.publish(msg)
            return

        target_angle = math.atan2(dy, dx)
        angle_error = self.normalize_angle(target_angle - self.current_yaw)
        
        self.get_logger().info(
            f"State: {self.state} | Pos: ({self.current_x:.2f}, {self.current_y:.2f}) | "
            f"Yaw: {math.degrees(self.current_yaw):.1f} | Err: {math.degrees(angle_error):.1f}",
            throttle_duration_sec=0.5
        )

        if self.state == 'TURNING_TO_GOAL':
            if abs(angle_error) > self.angle_tolerance_:
                msg.angular.z = self.angular_speed_ if angle_error > 0 else -self.angular_speed_
            else:
                self.get_logger().info('State change -> MOVING_TO_GOAL')
                self.state = 'MOVING_TO_GOAL'

        elif self.state == 'MOVING_TO_GOAL':
            if distance_to_goal > self.dist_tolerance_:
                # Re-check angle while moving
                if abs(angle_error) > self.angle_tolerance_ * 2.0:
                    self.get_logger().info('Correcting course -> TURNING_TO_GOAL')
                    self.state = 'TURNING_TO_GOAL'
                else:
                    msg.linear.x = self.linear_speed_
            else:
                self.get_logger().info(f'!!! GOAL REACHED at ({self.current_x:.2f}, {self.current_y:.2f}) !!!')
                self.state = 'GOAL_REACHED'
        
        self.publisher_.publish(msg)

    def publish_goal_marker(self):
        marker = Marker()
        marker.header.frame_id = "odom"
        marker.header.stamp = self.get_clock().now().to_msg()
        marker.ns = "goal"
        marker.id = 0
        marker.type = Marker.SPHERE
        marker.action = Marker.ADD
        marker.pose.position.x = self.goal_x_
        marker.pose.position.y = self.goal_y_
        marker.pose.position.z = 0.2
        marker.scale.x = 0.3
        marker.scale.y = 0.3
        marker.scale.z = 0.3
        marker.color.a = 1.0 # Alpha must be 1.0 to be visible
        marker.color.r = 1.0 # Red
        marker.color.g = 0.0
        marker.color.b = 0.0
        self.marker_pub_.publish(marker)

    def euler_from_quaternion(self, q):
        t3 = +2.0 * (q.w * q.z + q.x * q.y)
        t4 = +1.0 - 2.0 * (q.y * q.y + q.z * q.z)
        return math.atan2(t3, t4)

    def normalize_angle(self, angle):
        while angle > math.pi: angle -= 2.0 * math.pi
        while angle < -math.pi: angle += 2.0 * math.pi
        return angle

def main(args=None):
    rclpy.init(args=args)
    node = CoordinateNavigatorNode()
    rclpy.spin(node)
    rclpy.shutdown()

if __name__ == '__main__':
    main()