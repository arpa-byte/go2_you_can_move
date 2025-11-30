import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import threading
import sys
import tty
import termios

# --- Configuration for the sequence ---
FORWARD_DURATION = 3.0
STOP_DURATION = 2.0
TURN_DURATION = 2.0
FORWARD_SPEED = 0.4
TURN_SPEED = 0.8

# --- NEW: Keycode for the emergency stop ---
ESTOP_KEYCODE = ' ' # Spacebar

class SequenceControllerNode(Node):
    def __init__(self):
        super().__init__('sequence_controller_node')
        
        self.publisher_ = self.create_publisher(Twist, '/cmd_vel', 10)
        self.timer_ = self.create_timer(0.1, self.timer_callback)
        
        # --- State Machine Variables ---
        self.current_state = 0
        self.state_start_time = self.get_clock().now()
        
        # --- NEW: Emergency Stop State Variables ---
        self.emergency_stop_active = False
        self.lock = threading.Lock() # To safely access the E-stop flag from both threads

        # --- NEW: Start a background thread to listen for keyboard input ---
        self.keyboard_thread = threading.Thread(target=self.keyboard_listener)
        self.keyboard_thread.daemon = True
        self.keyboard_thread.start()

        self.get_logger().info('Sequence Controller has started.')
        self.get_logger().info('>>> PRESS SPACEBAR FOR EMERGENCY STOP <<<')
        self.get_logger().info('State: MOVING_FORWARD')

    def change_state(self, new_state):
        self.current_state = new_state
        self.state_start_time = self.get_clock().now()

    # --- NEW: This function runs in a separate thread to listen for the E-stop key ---
    def keyboard_listener(self):
        # Save the original terminal settings
        old_settings = termios.tcgetattr(sys.stdin)
        try:
            # Set the terminal to character-by-character input
            tty.setcbreak(sys.stdin.fileno())
            while rclpy.ok():
                char = sys.stdin.read(1)
                if char == ESTOP_KEYCODE:
                    with self.lock:
                        if not self.emergency_stop_active:
                            self.emergency_stop_active = True
                            # Use call_soon_threadsafe to log from a non-ROS thread
                            self.get_logger().warn('!!! EMERGENCY STOP ACTIVATED !!!')
                            # Send one final stop command immediately
                            stop_msg = Twist()
                            self.publisher_.publish(stop_msg)
        finally:
            # Always restore the original terminal settings
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)

    def timer_callback(self):
        now = self.get_clock().now()
        elapsed_time = (now - self.state_start_time).nanoseconds / 1e9

        msg = Twist()

        # --- NEW: Check for E-stop at the very beginning of the logic loop ---
        with self.lock:
            if self.emergency_stop_active:
                # If E-stop is active, do nothing but send stop commands.
                self.publisher_.publish(msg) # msg is already zeroed
                return # Exit the function immediately

        # --- State Machine Logic (only runs if E-stop is not active) ---
        if self.current_state == 0:
            msg.linear.x = FORWARD_SPEED
            if elapsed_time > FORWARD_DURATION:
                self.change_state(1)
                self.get_logger().info('State change -> STOPPING')
        elif self.current_state == 1:
            if elapsed_time > STOP_DURATION:
                self.change_state(2)
                self.get_logger().info('State change -> TURNING')
        elif self.current_state == 2:
            msg.angular.z = TURN_SPEED
            if elapsed_time > TURN_DURATION:
                self.change_state(3)
                self.get_logger().info('State change -> FINAL_STOP')
        elif self.current_state == 3:
            pass
        
        self.publisher_.publish(msg)

def main(args=None):
    rclpy.init(args=args)
    node = SequenceControllerNode()
    rclpy.spin(node)
    rclpy.shutdown()

if __name__ == '__main__':
    main()