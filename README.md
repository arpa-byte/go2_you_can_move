## Build Instructions

1.  **Clone the repository:**
    This repository uses Git submodules to include the `go2_driver` and `go2_interfaces` packages. You must clone it using the `--recurse-submodules` flag to ensure these dependencies are downloaded correctly.

    ```bash
    cd ~/your_ros2_ws/src
    git clone --recurse-submodules https://github.com/arpa-byte/go2_you_can_move.git
    ```
    
    If you have already cloned the repository without the flag, you can initialize the submodules by running this command from inside the repository's root directory:
    ```bash
    git submodule update --init --recursive
    ```

2.  **Navigate to your workspace root:**
    ```bash
    cd ~/your_ros2_ws
    ```

3.  **Install dependencies:**
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
