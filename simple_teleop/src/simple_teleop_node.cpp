#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <termios.h>
#include <unistd.h>

// RAII class to restore terminal settings
class TerminalRestorer {
public:
    TerminalRestorer() {
        tcgetattr(STDIN_FILENO, &original_settings_);
    }
    ~TerminalRestorer() {
        RCLCPP_INFO(rclcpp::get_logger("simple_teleop_node"), "Restoring terminal settings.");
        tcsetattr(STDIN_FILENO, TCSANOW, &original_settings_);
    }
private:
    struct termios original_settings_;
};

int getch() {
    return getchar();
}

const char KEYCODE_W = 'w';
const char KEYCODE_S = 's';
const char KEYCODE_A = 'a';
const char KEYCODE_D = 'd';
const char KEYCODE_Q = 'q';
const char KEYCODE_E = 'e';
const char KEYCODE_SPACE = ' ';

class SimpleTeleopNode : public rclcpp::Node {
public:
    SimpleTeleopNode() : Node("simple_teleop_node") {
        restorer_ = std::make_unique<TerminalRestorer>();
        configure_terminal();

        cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);
        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(50),
            std::bind(&SimpleTeleopNode::publish_cmd_vel, this));

        keyboard_thread_ = std::thread(&SimpleTeleopNode::keyboard_loop, this);
        print_instructions();
    }

private:
    void print_instructions() {
        RCLCPP_INFO(this->get_logger(), "---------------------------");
        RCLCPP_INFO(this->get_logger(), " Simple Go2 Keyboard Control");
        RCLCPP_INFO(this->get_logger(), "---------------------------");
        RCLCPP_INFO(this->get_logger(), "Movement:");
        RCLCPP_INFO(this->get_logger(), "   w: Move Forward");
        RCLCPP_INFO(this->get_logger(), "   s: Move Backward");
        RCLCPP_INFO(this->get_logger(), "   a: Move Left");
        RCLCPP_INFO(this->get_logger(), "   d: Move Right");
        RCLCPP_INFO(this->get_logger(), "   q: Turn Left");
        RCLCPP_INFO(this->get_logger(), "   e: Turn Right");
        RCLCPP_INFO(this->get_logger(), "   space: Stop");
        RCLCPP_INFO(this->get_logger(), "---------------------------");
        RCLCPP_INFO(this->get_logger(), "Press Ctrl+C to quit.");
        RCLCPP_INFO(this->get_logger(), "---------------------------");
    }

    void configure_terminal() {
        struct termios new_settings;
        tcgetattr(STDIN_FILENO, &new_settings);
        new_settings.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);
    }

    void publish_cmd_vel() {
        auto message = geometry_msgs::msg::Twist();
        message.linear.x = vx_;
        message.linear.y = vy_;
        message.angular.z = vyaw_;
        cmd_vel_pub_->publish(message);
    }

    void keyboard_loop() {
        while (rclcpp::ok()) {
            int key = getch();
            if (key == EOF) continue;
            switch (key) {
                case KEYCODE_W:
                    vx_ = 0.5; vy_ = 0.0; vyaw_ = 0.0;
                    RCLCPP_INFO(this->get_logger(), "Moving Forward");
                    break;
                case KEYCODE_S:
                    vx_ = -0.5; vy_ = 0.0; vyaw_ = 0.0;
                    RCLCPP_INFO(this->get_logger(), "Moving Backward");
                    break;
                case KEYCODE_A:
                    vx_ = 0.0; vy_ = 0.5; vyaw_ = 0.0;
                    RCLCPP_INFO(this->get_logger(), "Moving Left");
                    break;
                case KEYCODE_D:
                    vx_ = 0.0; vy_ = -0.5; vyaw_ = 0.0;
                    RCLCPP_INFO(this->get_logger(), "Moving Right");
                    break;
                case KEYCODE_Q:
                    vx_ = 0.0; vy_ = 0.0; vyaw_ = 0.5;
                    RCLCPP_INFO(this->get_logger(), "Turning Left");
                    break;
                case KEYCODE_E:
                    vx_ = 0.0; vy_ = 0.0; vyaw_ = -0.5;
                    RCLCPP_INFO(this->get_logger(), "Turning Right");
                    break;
                case KEYCODE_SPACE:
                    vx_ = 0.0; vy_ = 0.0; vyaw_ = 0.0;
                    RCLCPP_INFO(this->get_logger(), "Stopping");
                    break;
            }
        }
    }

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
    rclcpp::TimerBase::SharedPtr timer_;
    std::thread keyboard_thread_;
    std::unique_ptr<TerminalRestorer> restorer_;

    float vx_ = 0.0, vy_ = 0.0, vyaw_ = 0.0;
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<SimpleTeleopNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}