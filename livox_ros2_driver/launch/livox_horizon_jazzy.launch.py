"""Launch a legacy Livox Horizon with ROS2 Jazzy (CustomMsg + IMU).

With empty lidar_config and invalid command-line broadcast code, the legacy
SDK automatically connects a detected Livox. Use an allowlist when multiple
Livox sensors are on the Ethernet network.
"""
from pathlib import Path
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    pkg_share = Path(get_package_share_directory("livox_ros2_driver"))
    default_config = str(pkg_share / "config" / "livox_horizon_jazzy.json")
    return LaunchDescription([
        DeclareLaunchArgument("user_config_path", default_value=default_config),
        DeclareLaunchArgument("frame_id", default_value="livox_frame"),
        DeclareLaunchArgument(
            "esp_sync_enabled", default_value="false",
            description="Enable the horizon_flir_sync ESP32/UTC bridge",
        ),
        Node(
            package="livox_ros2_driver",
            executable="livox_ros2_driver_node",
            name="livox_horizon",
            output="screen",
            parameters=[{
                "esp_sync_enabled": ParameterValue(
                    LaunchConfiguration("esp_sync_enabled"), value_type=bool),
                "xfer_format": 1,       # CustomMsg with per-point offset_time (ns)
                "multi_topic": 0,
                "data_src": 0,
                "publish_freq": 10.0,
                "output_data_type": 0,
                "frame_id": LaunchConfiguration("frame_id"),
                "user_config_path": LaunchConfiguration("user_config_path"),
                "cmdline_input_bd_code": "000000000000001",
                "lvx_file_path": "/tmp/livox_unused.lvx",
            }],
        ),
    ])
