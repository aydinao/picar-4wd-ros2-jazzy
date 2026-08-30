# Bring up the PiCar under ros2_control.
#
#   ros2 launch picar_ros picar.launch.py
#   ros2 launch picar_ros picar.launch.py use_mock_hardware:=true   # no robot needed
#
# Needs the HAT battery-powered and the MCU out of reset (see the repo README).
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, RegisterEventHandler
from launch.conditions import IfCondition
from launch.event_handlers import OnProcessExit
from launch.substitutions import Command, FindExecutable, LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    args = [
        DeclareLaunchArgument(
            "use_mock_hardware",
            default_value="false",
            description="Use mock_components/GenericSystem instead of the real HAT.",
        ),
        DeclareLaunchArgument(
            "prefix", default_value="", description="Prefix for joint and link names."
        ),
        DeclareLaunchArgument(
            "start_controller",
            default_value="true",
            description="Also spawn the diff drive controller.",
        ),
    ]

    use_mock_hardware = LaunchConfiguration("use_mock_hardware")
    prefix = LaunchConfiguration("prefix")
    start_controller = LaunchConfiguration("start_controller")

    # ParameterValue(..., value_type=str) is required: without it launch tries
    # to parse the generated URDF as YAML and fails.
    robot_description = {
        "robot_description": ParameterValue(Command([
            FindExecutable(name="xacro"), " ",
            PathJoinSubstitution([
                FindPackageShare("picar_ros"), "description", "urdf", "picar.urdf.xacro"
            ]),
            " prefix:=", prefix,
            " use_mock_hardware:=", use_mock_hardware,
        ]), value_type=str)
    }

    controllers = PathJoinSubstitution([
        FindPackageShare("picar_ros"), "bringup", "config", "picar_controllers.yaml"
    ])

    # Owns the control loop. It is what calls read() and write() on the
    # hardware component, at controller_manager's update_rate.
    control_node = Node(
        package="controller_manager",
        executable="ros2_control_node",
        parameters=[controllers],
        output="both",
    )

    # Publishes TF from the URDF, and the /robot_description topic that
    # controller_manager reads its hardware description from.
    robot_state_pub = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        output="both",
        parameters=[robot_description],
    )

    jsb_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["joint_state_broadcaster", "--controller-manager", "/controller_manager"],
    )

    # Spawned only after the broadcaster is up, so joint states exist before
    # anything starts commanding wheels.
    base_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["picar_base_controller", "--controller-manager", "/controller_manager"],
        condition=IfCondition(start_controller),
    )

    return LaunchDescription(args + [
        control_node,
        robot_state_pub,
        jsb_spawner,
        RegisterEventHandler(
            OnProcessExit(target_action=jsb_spawner, on_exit=[base_spawner])
        ),
    ])
