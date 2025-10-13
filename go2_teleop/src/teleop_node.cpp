#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include <rclcpp/rclcpp.hpp>
#include <termios.h>
#include <unistd.h>

#include "common/ros2_sport_client.h"
#include "unitree_go/msg/sport_mode_state.hpp"

// *** NEW: RAII class to guarantee terminal settings are restored ***
class TerminalRestorer {
public:
    TerminalRestorer() {
        // Get the original terminal settings
        tcgetattr(STDIN_FILENO, &original_settings_);
    }

    ~TerminalRestorer() {
        // Restore the original terminal settings on destruction
        RCLCPP_INFO(rclcpp::get_logger("go2_teleop_node"), "Restoring terminal settings.");
        tcsetattr(STDIN_FILENO, TCSANOW, &original_settings_);
    }

private:
    struct termios original_settings_;
};

// *** SIMPLIFIED: getch() no longer manages terminal state ***
int getch() {
    return getchar();
}

// Define the key mappings (no changes here)
const char KEYCODE_W = 'w';
const char KEYCODE_S = 's';
const char KEYCODE_A = 'a';
const char KEYCODE_D = 'd';
const char KEYCODE_Q = 'q';
const char KEYCODE_E = 'e';
const char KEYCODE_1 = '1';
const char KEYCODE_2 = '2';
const char KEYCODE_3 = '3';
const char KEYCODE_4 = '4';
const char KEYCODE_5 = '5';
const char KEYCODE_6 = '6';
const char KEYCODE_SPACE = ' ';

class TeleopNode : public rclcpp::Node {
public:
    TeleopNode() : Node("go2_teleop_node"), sport_client_(this) {
        // *** NEW: Create the restorer object. Its destructor will automatically run on exit. ***
        restorer_ = std::make_unique<TerminalRestorer>();
        
        // *** NEW: Configure the terminal to our desired mode once. ***
        configure_terminal();

        vx_ = 0.0;
        vy_ = 0.0;
        vyaw_ = 0.0;
        linear_speed_ = 0.5;
        strafe_speed_ = 0.5;
        angular_speed_ = 0.7;

        print_instructions();

        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(50),
            std::bind(&TeleopNode::publish_movement_command, this));

        keyboard_thread_ = std::thread(&TeleopNode::keyboard_loop, this);
    }

private:
    void print_instructions() {
      // (No changes here)
      RCLCPP_INFO(this->get_logger(), "---------------------------");
      RCLCPP_INFO(this->get_logger(), " Go2 Keyboard Remote Control (Latching Mode)");
      RCLCPP_INFO(this->get_logger(), "---------------------------");
      RCLCPP_INFO(this->get_logger(), "Movement (press once to start, space to stop):");
      RCLCPP_INFO(this->get_logger(), "   w/s: forward/backward");
      RCLCPP_INFO(this->get_logger(), "   a/d: strafe left/right");
      RCLCPP_INFO(this->get_logger(), "   q/e: turn left/right");
      RCLCPP_INFO(this->get_logger(), "Actions (instantaneous):");
      RCLCPP_INFO(this->get_logger(), "   1-6: Various stand/sit actions");
      RCLCPP_INFO(this->get_logger(), " (space): Stop All Movement");
      RCLCPP_INFO(this->get_logger(), "---------------------------");
      RCLCPP_INFO(this->get_logger(), "Press Ctrl+C in this terminal to quit.");
      RCLCPP_INFO(this->get_logger(), "---------------------------");
    }

    // *** NEW: Function to set the terminal to raw/non-echo mode ***
    void configure_terminal() {
        struct termios new_settings;
        tcgetattr(STDIN_FILENO, &new_settings);
        new_settings.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);
    }

    void publish_movement_command() {
      // (No changes here)
      if (vx_ != 0.0 || vy_ != 0.0 || vyaw_ != 0.0) {
        sport_client_.Move(req_, vx_, vy_, vyaw_);
      }
    }

    void keyboard_loop() {
      // (No changes here)
      while (rclcpp::ok()) {
        int key = getch();
        if (key == EOF) { continue; }
        switch (key) {
          case KEYCODE_W:
            vx_ = linear_speed_; vy_ = 0.0; vyaw_ = 0.0;
            RCLCPP_INFO(this->get_logger(), "Command: Start Moving Forward");
            break;
          case KEYCODE_S:
            vx_ = -linear_speed_; vy_ = 0.0; vyaw_ = 0.0;
            RCLCPP_INFO(this->get_logger(), "Command: Start Moving Backward");
            break;
          case KEYCODE_A:
            vx_ = 0.0; vy_ = strafe_speed_; vyaw_ = 0.0;
            RCLCPP_INFO(this->get_logger(), "Command: Start Strafing Left");
            break;
          case KEYCODE_D:
            vx_ = 0.0; vy_ = -strafe_speed_; vyaw_ = 0.0;
            RCLCPP_INFO(this->get_logger(), "Command: Start Strafing Right");
            break;
          case KEYCODE_Q:
            vx_ = 0.0; vy_ = 0.0; vyaw_ = angular_speed_;
            RCLCPP_INFO(this->get_logger(), "Command: Start Turning Left");
            break;
          case KEYCODE_E:
            vx_ = 0.0; vy_ = 0.0; vyaw_ = -angular_speed_;
            RCLCPP_INFO(this->get_logger(), "Command: Start Turning Right");
            break;
          case KEYCODE_SPACE:
            vx_ = 0.0; vy_ = 0.0; vyaw_ = 0.0;
            RCLCPP_INFO(this->get_logger(), "Command: Stop Move");
            sport_client_.StopMove(req_);
            break;
          case KEYCODE_1:
            RCLCPP_INFO(this->get_logger(), "Command: Balance Stand");
            sport_client_.BalanceStand(req_);
            break;
          case KEYCODE_2:
            RCLCPP_INFO(this->get_logger(), "Command: Stand Down");
            sport_client_.StandDown(req_);
            break;
          case KEYCODE_3:
            RCLCPP_INFO(this->get_logger(), "Command: Stand Up");
            sport_client_.StandUp(req_);
            break;
          case KEYCODE_4:
            RCLCPP_INFO(this->get_logger(), "Command: Recovery Stand");
            sport_client_.RecoveryStand(req_);
            break;
          case KEYCODE_5:
            RCLCPP_INFO(this->get_logger(), "Command: Sit Down");
            sport_client_.Sit(req_);
            break;
          case KEYCODE_6:
            RCLCPP_INFO(this->get_logger(), "Command: Rise from Sit");
            sport_client_.RiseSit(req_);
            break;
        }
      }
    }
  
    SportClient sport_client_;
    unitree_api::msg::Request req_;
    rclcpp::TimerBase::SharedPtr timer_;
    std::thread keyboard_thread_;

    // *** NEW: Member variable for our RAII terminal restorer ***
    std::unique_ptr<TerminalRestorer> restorer_;

    float vx_, vy_, vyaw_;
    float linear_speed_, strafe_speed_, angular_speed_;
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TeleopNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}