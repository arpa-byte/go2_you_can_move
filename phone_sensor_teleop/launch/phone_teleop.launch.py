from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, ExecuteProcess
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    # --- Get path to the parameter file ---
    phone_params_file = os.path.join(
        get_package_share_directory('phone_sensor_teleop'),
        'config',
        'phone_params.yaml'
    )

    # 1. Include the essential go2_driver launch file
    driver_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory('go2_driver'),
                'launch',
                'go2_driver.launch.py'
            )
        )
    )

    # 2. The phone sensor teleop node, now with parameters
    phone_teleop_node = Node(
        package='phone_sensor_teleop',
        executable='phone_sensor_node',
        name='phone_sensor_teleop_node',
        output='screen',
        parameters=[phone_params_file]  # <-- This is the crucial addition
    )
    
    # 3. The adb reverse command action
    adb_reverse_cmd = ExecuteProcess(
        cmd=['adb', 'reverse', 'tcp:8080', 'tcp:8080'],
        name='adb_reverse_tcp',
        output='screen'
    )

    return LaunchDescription([
        driver_launch,
        phone_teleop_node,
        adb_reverse_cmd
    ])