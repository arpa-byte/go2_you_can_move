from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node, ComposableNodeContainer
from launch_ros.descriptions import ComposableNode
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():

    # --- Config Files ---
    nav_params_file = os.path.join(
        get_package_share_directory('go2_behavior'),
        'config',
        'navigate_params.yaml'
    )

    rviz_config_file = os.path.join(
        get_package_share_directory('go2_behavior'),
        'rviz',
        'navigate.rviz'
    )

    # --- Launch arguments with fixed coordinates ---
    declare_target_x = DeclareLaunchArgument(
        'target_x',
        default_value='0.0',
        description='Target X coordinate for the coordinate_navigator'
    )

    declare_target_y = DeclareLaunchArgument(
        'target_y',
        default_value='0.0',
        description='Target Y coordinate for the coordinate_navigator'
    )

    declare_target_yaw = DeclareLaunchArgument(
        'target_yaw',
        default_value='0.0',
        description='Target yaw (radians) for the coordinate_navigator'
    )

    target_x_config = LaunchConfiguration('target_x')
    target_y_config = LaunchConfiguration('target_y')
    target_yaw_config = LaunchConfiguration('target_yaw')

    # --- Node 1: The Go2 Driver (REMAPPED/SILENCED) ---
    driver_node = ComposableNode(
        package='go2_driver',
        plugin='go2_driver::Go2Driver',
        name='go2_driver',
        namespace='',
        remappings=[
            ('/tf', '/tf_driver_ignored'),
            ('/odom', '/odom_driver_ignored'),
        ]
    )

    driver_container = ComposableNodeContainer(
        name='go2_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        composable_node_descriptions=[driver_node],
        output='screen',
    )

    # --- Node 2: Pointcloud to Laserscan ---
    pointcloud_node = Node(
        package='pointcloud_to_laserscan',
        executable='pointcloud_to_laserscan_node',
        name='pointcloud_to_laserscan',
        namespace='',
        output='screen',
        remappings=[('/cloud_in', '/pointcloud')],
        parameters=[{
            'target_frame': 'radar',
            'transform_tolerance': 0.01,
        }],
    )

    # --- Node 3: Odometry Bridge ---
    odometry_bridge_node = Node(
        package='go2_odometry_bridge',
        executable='go2_odom_bridge_node',
        name='go2_odometry_bridge_node',
        output='screen'
    )

    # --- Node 4: Coordinate Navigator ---
    navigate_node = Node(
        package='go2_behavior',
        executable='coordinate_navigator',
        name='coordinate_navigator_node',
        output='screen',
        parameters=[
            nav_params_file,
            {
                'target_x': target_x_config,
                'target_y': target_y_config,
                'target_yaw': target_yaw_config,
            }
        ]
    )

    # --- Node 5: RViz ---
    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        arguments=['-d', rviz_config_file],
        output='screen'
    )

    return LaunchDescription([
        declare_target_x,
        declare_target_y,
        declare_target_yaw,
        driver_container,
        pointcloud_node,
        odometry_bridge_node,
        navigate_node,
        rviz_node
    ])
