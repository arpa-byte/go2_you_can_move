from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='simple_teleop',
            executable='simple_teleop_node',
            name='simple_teleop_node',
            output='screen'
        )
    ])