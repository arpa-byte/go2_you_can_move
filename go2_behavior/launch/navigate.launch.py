from launch import LaunchDescription
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

    # --- Node 1: The Go2 Driver (REMAPPED/SILENCED) ---
    # We define this explicitly so we can remap its /tf and /odom topics.
    # This prevents it from fighting with our custom odometry bridge.
    driver_node = ComposableNode(
        package='go2_driver',
        plugin='go2_driver::Go2Driver',
        name='go2_driver',
        namespace='',
        remappings=[
            ('/tf', '/tf_driver_ignored'),       # Silence driver TF
            ('/odom', '/odom_driver_ignored'),   # Silence driver Odom
        ]
    )

    # Container for the driver (required structure for go2_driver)
    driver_container = ComposableNodeContainer(
        name='go2_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        composable_node_descriptions=[driver_node],
        output='screen',
    )

    # --- Node 2: Pointcloud to Laserscan (Standard) ---
    # This was in the original driver launch, we keep it.
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

    # --- Node 3: Odometry Bridge (Our "Truth" Source) ---
    # This provides the clean /odom and /tf that we actually want to use.
    odometry_bridge_node = Node(
        package='go2_odometry_bridge',
        executable='go2_odom_bridge_node',
        name='go2_odometry_bridge_node',
        output='screen'
    )

    # --- Node 4: Coordinate Navigator (The Brain) ---
    navigate_node = Node(
        package='go2_behavior',
        executable='coordinate_navigator',
        name='coordinate_navigator_node',
        output='screen',
        parameters=[nav_params_file],
        prefix='gnome-terminal --' # Pop-out terminal for keyboard input
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
        driver_container,
        pointcloud_node,
        odometry_bridge_node,
        navigate_node,
        rviz_node
    ])