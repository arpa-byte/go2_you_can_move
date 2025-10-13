cmake_minimum_required(VERSION 3.5)
project(simple_teleop)

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)

# Add executable
add_executable(simple_teleop_node src/simple_teleop_node.cpp)

# Link libraries
ament_target_dependencies(simple_teleop_node rclcpp geometry_msgs)

# Install
install(TARGETS
  simple_teleop_node
  DESTINATION lib/${PROJECT_NAME})

ament_package()