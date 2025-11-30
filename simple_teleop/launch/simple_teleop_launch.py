# File: src/simple_teleop/launch/simple_teleop_launch.py
# ----------------------------------------------------
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    # ------------------------------------------------
    # 1. Include the original driver launch file
    # ------------------------------------------------
    driver_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory('go2_driver'),
                'launch',
                'go2_driver.launch.py'
            )
        )
    )

    # ------------------------------------------------
    # 2. The teleop node (your simple_teleop_node)
    # ------------------------------------------------
    teleop_node = Node(
        package='simple_teleop',
        executable='simple_teleop_node',
        name='simple_teleop_node',
        output='screen',
        emulate_tty=True
    )

    return LaunchDescription([
        driver_launch,
        teleop_node
    ])