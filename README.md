# GO2 You Can Move

This repository contains a collection of ROS 2 packages for controlling and interacting with the Unitree Go2 robot.

## Pre-requisites

Before building this workspace, please ensure you have the following dependencies installed and sourced correctly in your ROS 2 environment.

*   **ROS 2:** This workspace is intended for use with ROS 2 (Humble/Iron).
*   **unitree_ros2:** The official ROS 2 wrapper for the Unitree SDK. This can be found here: [https://github.com/unitreerobotics/unitree_ros2](https://github.com/unitreerobotics/unitree_ros2)
*   **go2_driver:** The specific driver package for the Go2 robot.

## Build Instructions

1.  **Clone the repository:**
    Clone this repository into your ROS 2 workspace's `src` directory.

    ```bash
    cd ~/your_ros2_ws/src
    git clone https://github.com/arpa-byte/go2_you_can_move.git
    ```

2.  **Navigate to your workspace root:**
    ```bash
    cd ~/your_ros2_ws
    ```

3.  **Install dependencies (if any):**
    ```bash
    rosdep install --from-paths src -y --ignore-src
    ```

4.  **Build the workspace:**
    Use `colcon` to build the packages.

    ```bash
    colcon build --symlink-install
    ```

5.  **Source the workspace:**
    After the build is complete, source the new environment setup file.

    ```bash
    source install/setup.bash
    ```

You can now run the launch files or nodes provided by the packages in this repository.
