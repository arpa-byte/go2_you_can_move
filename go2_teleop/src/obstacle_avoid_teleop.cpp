#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include <rclcpp/rclcpp.hpp>
#include <termios.h>
#include <unistd.h>
#include "nlohmann/json.hpp"

#include "unitree_api/msg/request.hpp"

class TerminalRestorer {
public:
    TerminalRestorer() { tcgetattr(STDIN_FILENO, &original_settings_); }
    ~TerminalRestorer() {
        RCLCPP_INFO(rclcpp::get_logger("go2_obstacle_avoid_teleop"), "Restoring terminal settings.");
        tcsetattr(STDIN_FILENO, TCSANOW, &original_settings_);
    }
private:
    struct termios original_settings_;
};

int getch() { return getchar(); }

// Add key for LiDAR control
const char KEYCODE_W = 'w';
const char KEYCODE_S = 's';
const char KEYCODE_A = 'a';
const char KEYCODE_D = 'd';
const char KEYCODE_Q = 'q';
const char KEYCODE_E = 'e';
const char KEYCODE_SPACE = ' ';
const char KEYCODE_7 = '7'; // ACTIVATE Obstacle Avoidance Mode
const char KEYCODE_8 = '8'; // DEACTIVATE (return to Sport Mode)
const char KEYCODE_9 = '9'; // ENABLE LiDARs

class ObstacleAvoidTeleopNode : public rclcpp::Node {
public:
    ObstacleAvoidTeleopNode() : Node("go2_obstacle_avoid_teleop") {
        restorer_ = std::make_unique<TerminalRestorer>();
        configure_terminal();

        obs_avoid_pub_ = this->create_publisher<unitree_api::msg::Request>("/api/obstacles_avoid/request", 10);
        motion_switch_pub_ = this->create_publisher<unitree_api::msg::Request>("/api/motion_switcher/request", 10);
        // *** NEW: Publisher for the LiDAR switch ***
        lidar_switch_pub_ = this->create_publisher<unitree_api::msg::Request>("/utlidar/switch", 10);

        vx_ = 0.0; vy_ = 0.0; vyaw_ = 0.0;
        linear_speed_ = 0.4; strafe_speed_ = 0.3; angular_speed_ = 0.6;

        print_instructions();

        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(50),
            std::bind(&ObstacleAvoidTeleopNode::publish_movement_command, this));
        keyboard_thread_ = std::thread(&ObstacleAvoidTeleopNode::keyboard_loop, this);
    }

private:
    void print_instructions() {
      RCLCPP_INFO(this->get_logger(), "----------------------------------------------------");
      RCLCPP_INFO(this->get_logger(), "            Go2 Obstacle Avoidance Test");
      RCLCPP_INFO(this->get_logger(), "----------------------------------------------------");
      RCLCPP_INFO(this->get_logger(), "STEP 1: PRESS '9' to ENABLE the LiDARs. (You should hear them spin up)");
      RCLCPP_INFO(this->get_logger(), "STEP 2: PRESS '7' to SWITCH TO obstacle avoidance mode.");
      RCLCPP_INFO(this->get_logger(), "STEP 3: Use WASD/QE to drive the robot.");
      RCLCPP_INFO(this->get_logger(), "----------------------------------------------------");
      RCLCPP_INFO(this->get_logger(), "   9: ENABLE LiDARs");
      RCLCPP_INFO(this->get_logger(), "   7: SWITCH TO Obstacle Avoidance Mode");
      RCLCPP_INFO(this->get_logger(), "   8: SWITCH TO Normal Sport Mode (disables avoidance)");
      RCLCPP_INFO(this->get_logger(), " (space): Stop All Movement");
      RCLCPP_INFO(this->get_logger(), "----------------------------------------------------");
    }

    void configure_terminal() {
        struct termios s;
        tcgetattr(STDIN_FILENO, &s);
        s.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &s);
    }

    void publish_movement_command() {
        nlohmann::json json_params;
        json_params["vx"] = vx_;
        json_params["vy"] = vy_;
        json_params["vyaw"] = vyaw_;

        unitree_api::msg::Request request_msg;
        request_msg.parameter = json_params.dump();
        request_msg.header.identity.api_id = 1005;

        obs_avoid_pub_->publish(request_msg);
    }

    void switch_motion_mode(int mode) {
        nlohmann::json json_params;
        json_params["motion_id"] = mode;

        unitree_api::msg::Request request_msg;
        request_msg.parameter = json_params.dump();
        request_msg.header.identity.api_id = 3001;

        motion_switch_pub_->publish(request_msg);
    }
    
    // *** NEW: Function to enable/disable the LiDAR service ***
    void switch_lidar(bool enable) {
        nlohmann::json json_params;
        json_params["id"] = 0; // ID of the front LiDAR
        json_params["enable"] = enable;

        unitree_api::msg::Request request_msg;
        request_msg.parameter = json_params.dump();
        request_msg.header.identity.api_id = 2101; // API ID for LiDAR switch

        lidar_switch_pub_->publish(request_msg);
    }

    void keyboard_loop() {
      while (rclcpp::ok()) {
        int key = getch();
        if (key == EOF) { continue; }
        switch (key) {
          case KEYCODE_W: vx_ = linear_speed_; vy_ = 0.0; vyaw_ = 0.0; RCLCPP_INFO(this->get_logger(), "Set Velocity: FORWARD"); break;
          case KEYCODE_S: vx_ = -linear_speed_; vy_ = 0.0; vyaw_ = 0.0; RCLCPP_INFO(this->get_logger(), "Set Velocity: BACKWARD"); break;
          case KEYCODE_A: vx_ = 0.0; vy_ = strafe_speed_; vyaw_ = 0.0; RCLCPP_INFO(this->get_logger(), "Set Velocity: STRAFE LEFT"); break;
          case KEYCODE_D: vx_ = 0.0; vy_ = -strafe_speed_; vyaw_ = 0.0; RCLCPP_INFO(this->get_logger(), "Set Velocity: STRAFE RIGHT"); break;
          case KEYCODE_Q: vx_ = 0.0; vy_ = 0.0; vyaw_ = angular_speed_; RCLCPP_INFO(this->get_logger(), "Set Velocity: TURN LEFT"); break;
          case KEYCODE_E: vx_ = 0.0; vy_ = 0.0; vyaw_ = -angular_speed_; RCLCPP_INFO(this->get_logger(), "Set Velocity: TURN RIGHT"); break;
          case KEYCODE_SPACE: vx_ = 0.0; vy_ = 0.0; vyaw_ = 0.0; RCLCPP_INFO(this->get_logger(), "Set Velocity: STOP"); break;

          case KEYCODE_7:
            RCLCPP_INFO(this->get_logger(), "COMMAND: Switching to Obstacle Avoidance Mode (3)");
            switch_motion_mode(3);
            break;
          case KEYCODE_8:
            RCLCPP_INFO(this->get_logger(), "COMMAND: Switching to Normal Sport Mode (1)");
            switch_motion_mode(1);
            break;
          // *** NEW: Call the LiDAR switch function ***
          case KEYCODE_9:
            RCLCPP_INFO(this->get_logger(), "COMMAND: ENABLING LiDARs");
            switch_lidar(true);
            break;
        }
      }
    }
  
    // Member Variables
    rclcpp::Publisher<unitree_api::msg::Request>::SharedPtr obs_avoid_pub_;
    rclcpp::Publisher<unitree_api::msg::Request>::SharedPtr motion_switch_pub_;
    rclcpp::Publisher<unitree_api::msg::Request>::SharedPtr lidar_switch_pub_; // New publisher
    rclcpp::TimerBase::SharedPtr timer_;
    std::thread keyboard_thread_;
    std::unique_ptr<TerminalRestorer> restorer_;

    float vx_, vy_, vyaw_;
    float linear_speed_, strafe_speed_, angular_speed_;
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<ObstacleAvoidTeleopNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}