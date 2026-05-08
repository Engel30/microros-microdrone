"""
Launch unico tethered per il drone:
- micro-ROS agent in ascolto UDP4 su porta configurabile
- foxglove_bridge per Foxglove Studio (websocket su 8765)

Uso:
    ros2 launch drone_bringup drone.launch.py
    ros2 launch drone_bringup drone.launch.py uros_port:=8889 fg_port:=8766
"""
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    uros_port = LaunchConfiguration('uros_port')
    uros_verbose = LaunchConfiguration('uros_verbose')
    fg_port = LaunchConfiguration('fg_port')

    return LaunchDescription([
        DeclareLaunchArgument('uros_port', default_value='8888',
                              description='Porta UDP del micro-ROS agent'),
        DeclareLaunchArgument('uros_verbose', default_value='4',
                              description='Verbose level agent (0-6)'),
        DeclareLaunchArgument('fg_port', default_value='8765',
                              description='Porta WebSocket foxglove_bridge'),

        Node(
            package='micro_ros_agent',
            executable='micro_ros_agent',
            name='micro_ros_agent',
            output='screen',
            arguments=[
                'udp4',
                '--port', uros_port,
                '-v', uros_verbose,
            ],
        ),

        Node(
            package='foxglove_bridge',
            executable='foxglove_bridge',
            name='foxglove_bridge',
            output='screen',
            parameters=[{
                'port': fg_port,
                'address': '0.0.0.0',
                'tls': False,
                'use_sim_time': False,
            }],
        ),
    ])
