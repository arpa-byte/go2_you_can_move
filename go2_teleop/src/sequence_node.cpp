#include <cmath>
#include <memory>
#include <vector>
#include <thread>   // Required for std::thread
#include <iostream> // Required for std::cin

#include "rclcpp/rclcpp.hpp"
#include "unitree_go/msg/sport_mode_state.hpp"
#include "common/ros2_sport_client.h"

// *** NEW: Added states for waiting on user input ***
enum class SequenceState {
    WAIT_FOR_START, // New state
    INIT,
    FORWARD_1,
    STOP_1,
    ROTATE_1,
    STOP_2,
    FORWARD_2,
    STOP_3,
    BACKWARD,
    STOP_4,
    ROTATE_2,
    STOP_5,
    SITTING,
    WAIT_FOR_STANDUP, // New state
    DONE
};

class SequenceNode : public rclcpp::Node {
public:
    SequenceNode() : Node("go2_sequence_node"), sport_client_(this) {
        // Print the initial warning and instructions
        RCLCPP_WARN(this->get_logger(), "MAKE SURE THE ROBOT IS IN AN OPEN SPACE.");
        RCLCPP_WARN(this->get_logger(), "AWAY FROM ANY WALL, FURNITURE, OR OBSTRUCTION.");
        RCLCPP_INFO(this->get_logger(), "Press [Enter] to begin the sequence...");

        // Start in the waiting state
        current_state_ = SequenceState::WAIT_FOR_START;
        
        state_sub_ = this->create_subscription<unitree_go::msg::SportModeState>(
            "/lf/sportmodestate", 10,
            std::bind(&SequenceNode::state_callback, this, std::placeholders::_1));

        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(50),
            std::bind(&SequenceNode::update_sequence, this));
        
        // *** NEW: Start a thread to listen for user input ***
        input_thread_ = std::thread(&SequenceNode::user_input_loop, this);
    }

private:
    // *** NEW: This function runs in a separate thread to handle user input ***
    void user_input_loop() {
        while(rclcpp::ok()) {
            // This waits for the user to press Enter
            if (std::cin.get() == '\n') {
                if (current_state_ == SequenceState::WAIT_FOR_START) {
                    RCLCPP_INFO(this->get_logger(), "Enter pressed. Initializing sequence...");
                    // Transition to INIT, which waits for the first state message
                    current_state_ = SequenceState::INIT;
                } else if (current_state_ == SequenceState::WAIT_FOR_STANDUP) {
                    RCLCPP_INFO(this->get_logger(), "Enter pressed. Commanding robot to stand...");
                    // Transition to DONE, which will execute RiseSit and shutdown
                    current_state_ = SequenceState::DONE;
                }
            }
        }
    }

    void state_callback(const unitree_go::msg::SportModeState::SharedPtr msg) {
        current_pos_ = {msg->position[0], msg->position[1], msg->position[2]};
        current_rpy_ = {msg->imu_state.rpy[0], msg->imu_state.rpy[1], msg->imu_state.rpy[2]};

        // Only initialize if we are in the INIT state (i.e., after user pressed Enter)
        if (current_state_ == SequenceState::INIT && !initial_pose_received_) {
            start_pos_ = current_pos_;
            start_rpy_ = current_rpy_;
            initial_rpy_ = current_rpy_[2]; // Store the absolute initial orientation
            initial_pose_received_ = true;
            RCLCPP_INFO(this->get_logger(), "Initial pose received. Starting sequence.");
            current_state_ = SequenceState::FORWARD_1; // Start the first action
        }
    }

    void update_sequence() {
        if (!initial_pose_received_ && current_state_ > SequenceState::INIT) {
            return; 
        }

        float distance_traveled = 0.0;
        float angle_turned = 0.0;

        switch (current_state_) {
            // The first few states are unchanged...
            case SequenceState::FORWARD_1:
                RCLCPP_INFO_ONCE(this->get_logger(), "State: FORWARD_1 (2 meters)");
                sport_client_.Move(req_, linear_speed_, 0.0, 0.0);
                distance_traveled = get_distance(start_pos_, current_pos_);
                if (distance_traveled >= 2.0) {
                    transition_to_state(SequenceState::STOP_1);
                }
                break;

            case SequenceState::ROTATE_1:
                RCLCPP_INFO_ONCE(this->get_logger(), "State: ROTATE_1 (180 deg clockwise)");
                sport_client_.Move(req_, 0.0, 0.0, -angular_speed_);
                angle_turned = std::abs(normalize_angle(current_rpy_[2] - start_rpy_[2]));
                if (angle_turned >= M_PI - 0.1) {
                    transition_to_state(SequenceState::STOP_2);
                }
                break;

            case SequenceState::FORWARD_2:
                RCLCPP_INFO_ONCE(this->get_logger(), "State: FORWARD_2 (4 meters)");
                sport_client_.Move(req_, linear_speed_, 0.0, 0.0);
                distance_traveled = get_distance(start_pos_, current_pos_);
                if (distance_traveled >= 4.0) {
                    transition_to_state(SequenceState::STOP_3);
                }
                break;

            case SequenceState::BACKWARD:
                RCLCPP_INFO_ONCE(this->get_logger(), "State: BACKWARD (2 meters)");
                sport_client_.Move(req_, -linear_speed_, 0.0, 0.0);
                distance_traveled = get_distance(start_pos_, current_pos_);
                if (distance_traveled >= 2.0) {
                    transition_to_state(SequenceState::STOP_4);
                }
                break;

            case SequenceState::ROTATE_2:
                RCLCPP_INFO_ONCE(this->get_logger(), "State: ROTATE_2 (180 deg counter-clockwise)");
                sport_client_.Move(req_, 0.0, 0.0, angular_speed_);
                angle_turned = std::abs(normalize_angle(current_rpy_[2] - initial_rpy_));
                if (angle_turned <= 0.1) {
                    transition_to_state(SequenceState::STOP_5);
                }
                break;
            
            // *** MODIFIED: SITTING state now transitions to WAIT_FOR_STANDUP ***
            case SequenceState::SITTING:
                 RCLCPP_INFO_ONCE(this->get_logger(), "State: SITTING");
                 sport_client_.Sit(req_);
                 RCLCPP_INFO(this->get_logger(), "Sequence completed successfully.");
                 RCLCPP_INFO(this->get_logger(), "Robot is sitting. Press [Enter] to stand up and exit.");
                 current_state_ = SequenceState::WAIT_FOR_STANDUP; // Transition to waiting state
                 break;
            
            // *** MODIFIED: DONE state now executes RiseSit before shutting down ***
            case SequenceState::DONE:
                RCLCPP_INFO_ONCE(this->get_logger(), "Executing RiseSit()...");
                sport_client_.RiseSit(req_);
                // Give it a moment to send the command before shutting down
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                RCLCPP_INFO_ONCE(this->get_logger(), "Exiting.");
                timer_->cancel();
                rclcpp::shutdown();
                break;

            // Stop states logic is updated to handle the new final transition
            case SequenceState::STOP_1:
            case SequenceState::STOP_2:
            case SequenceState::STOP_3:
            case SequenceState::STOP_4:
            case SequenceState::STOP_5:
                RCLCPP_INFO_ONCE(this->get_logger(), "State: STOPPING");
                sport_client_.StopMove(req_);
                stop_counter_++;
                if (stop_counter_ > 20) { 
                    if (current_state_ == SequenceState::STOP_1) transition_to_state(SequenceState::ROTATE_1);
                    else if (current_state_ == SequenceState::STOP_2) transition_to_state(SequenceState::FORWARD_2);
                    else if (current_state_ == SequenceState::STOP_3) transition_to_state(SequenceState::BACKWARD);
                    else if (current_state_ == SequenceState::STOP_4) transition_to_state(SequenceState::ROTATE_2);
                    else if (current_state_ == SequenceState::STOP_5) transition_to_state(SequenceState::SITTING);
                }
                break;
            
            // The new waiting states do nothing in the main loop; they wait for the input thread
            case SequenceState::WAIT_FOR_START:
            case SequenceState::INIT:
            case SequenceState::WAIT_FOR_STANDUP:
                // Do nothing
                break;
        }
    }
    
    // Helper functions are unchanged...
    void transition_to_state(SequenceState next_state) {
        RCLCPP_INFO(this->get_logger(), "Transitioning to state %d", static_cast<int>(next_state));
        sport_client_.StopMove(req_);
        start_pos_ = current_pos_;
        start_rpy_ = current_rpy_;
        stop_counter_ = 0;
        current_state_ = next_state;
    }
    
    float get_distance(const std::vector<float>& p1, const std::vector<float>& p2) {
        return std::sqrt(std::pow(p2[0] - p1[0], 2) + std::pow(p2[1] - p1[1], 2));
    }

    float normalize_angle(float angle) {
        while (angle > M_PI) angle -= 2.0 * M_PI;
        while (angle < -M_PI) angle += 2.0 * M_PI;
        return angle;
    }

    // Member Variables
    SequenceState current_state_;
    SportClient sport_client_;
    unitree_api::msg::Request req_;
    rclcpp::Subscription<unitree_go::msg::SportModeState>::SharedPtr state_sub_;
    rclcpp::TimerBase::SharedPtr timer_;
    std::thread input_thread_; // The thread for user input

    bool initial_pose_received_ = false;
    std::vector<float> current_pos_{0, 0, 0};
    std::vector<float> current_rpy_{0, 0, 0};
    std::vector<float> start_pos_{0, 0, 0};
    std::vector<float> start_rpy_{0, 0, 0};
    float initial_rpy_ = 0.0;

    float linear_speed_ = 0.3;
    float angular_speed_ = 0.5;
    int stop_counter_ = 0;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<SequenceNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}