# File: ~/unitree2/ros_go2_test_env/src/go2_behavior/launch/sequence.launch.py

from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    # 1. Include the essential go2_driver launch file. This is required.
    driver_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory('go2_driver'),
                'launch',
                'go2_driver.launch.py'
            )
        )
    )

    # 2. The new sequence controller node
    sequence_node = Node(
        package='go2_behavior',
        executable='sequence_controller',
        name='sequence_controller_node',
        output='screen'
    )

    return LaunchDescription([
        driver_launch,
        sequence_node
    ])