cmake_minimum_required(VERSION 3.8)
project(go2_teleop)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find necessary ROS2 and Unitree packages
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(unitree_go REQUIRED)
find_package(unitree_api REQUIRED)


#--- Teleop Node ---
# Create the executable for our teleop node.
# It is built from our source file AND the .cpp file from the original package.
add_executable(teleop_node
  src/teleop_node.cpp
  /home/arpan/unitree_ros2/example/src/src/common/ros2_sport_client.cpp
)

# Add the include directories required by BOTH source files to our target.
# This is the line that fixes the problem.
target_include_directories(teleop_node PRIVATE
    # This path is for teleop_node.cpp to find "common/ros2_sport_client.h"
    /home/arpan/unitree_ros2/example/src/include
    # This path is for ros2_sport_client.cpp to find "ros2_sport_client.h"
    /home/arpan/unitree_ros2/example/src/include/common
)

# Link the executable against its dependencies
ament_target_dependencies(teleop_node
  "rclcpp"
  "unitree_go"
  "unitree_api"
)

# Define installation rules so 'ros2 run' can find our node
install(TARGETS
  teleop_node
  DESTINATION lib/${PROJECT_NAME}
)

# --- NEW: SEQUENCE NODE ---
add_executable(sequence_node
  src/sequence_node.cpp
  # It also needs the sport client helper
  /home/arpan/unitree_ros2/example/src/src/common/ros2_sport_client.cpp
)
target_include_directories(sequence_node PRIVATE
    # It also needs the same include directories
    /home/arpan/unitree_ros2/example/src/include
    /home/arpan/unitree_ros2/example/src/include/common
)
ament_target_dependencies(sequence_node "rclcpp" "unitree_go" "unitree_api")
install(TARGETS
  sequence_node
  DESTINATION lib/${PROJECT_NAME}
)

# --- Obstacle Avoidance Teleop Node ---
add_executable(obstacle_avoid_teleop
  src/obstacle_avoid_teleop.cpp
)

# Manually vendor nlohmann/json (header-only)
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link your executable to the vendored library and other deps
ament_target_dependencies(obstacle_avoid_teleop 
  "rclcpp" 
  "unitree_api"
)
target_link_libraries(obstacle_avoid_teleop nlohmann_json)

install(TARGETS
  obstacle_avoid_teleop
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()