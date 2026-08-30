"""Consistency checks across the URDF, the ros2_control block and the
controller config.

These three files have to agree on joint names and interfaces, and nothing in
the build enforces that. A mismatch is silent until runtime, where it surfaces
as a controller failing to claim an interface -- a long way from the typo.
"""
import os
import xml.etree.ElementTree as ET

import pytest
import xacro
import yaml

HERE = os.path.dirname(__file__)
PKG = os.path.dirname(HERE)
URDF_XACRO = os.path.join(PKG, "description", "urdf", "picar.urdf.xacro")
CONTROLLERS = os.path.join(PKG, "bringup", "config", "picar_controllers.yaml")

# Params on_init() fetches with .at(), which throws if any is missing.
REQUIRED_HW_PARAMS = {
    "i2c_bus", "i2c_address", "pwm_frequency_hz", "gpio_chip",
    "reset_gpio", "left_encoder_gpio", "right_encoder_gpio",
    "encoder_slots_per_rev",
}
REQUIRED_JOINT_PARAMS = {"pwm_channel", "dir_gpio", "reversed", "encoder_side"}


@pytest.fixture(scope="module")
def robot():
    """The generated URDF. Fails the whole module if the xacro is broken."""
    return ET.fromstring(xacro.process_file(URDF_XACRO).toprettyxml())


@pytest.fixture(scope="module")
def controllers():
    with open(CONTROLLERS) as f:
        return yaml.safe_load(f)


def urdf_joint_names(robot):
    return {j.get("name") for j in robot.findall("joint")}


def ros2_control_joints(robot):
    block = robot.find("ros2_control")
    assert block is not None, "no <ros2_control> block in the generated URDF"
    return {j.get("name"): j for j in block.findall("joint")}


def test_xacro_generates_a_named_robot(robot):
    assert robot.tag == "robot"
    assert robot.get("name")


def test_every_ros2_control_joint_exists_in_the_kinematic_tree(robot):
    """A ros2_control joint with no URDF joint is claimed but never moves."""
    missing = set(ros2_control_joints(robot)) - urdf_joint_names(robot)
    assert not missing, f"declared to ros2_control but absent from the URDF: {missing}"


def test_controller_wheel_names_all_exist(robot, controllers):
    """Catches a joint renamed in one file and not the other."""
    params = controllers["picar_base_controller"]["ros__parameters"]
    named = set(params["left_wheel_names"]) | set(params["right_wheel_names"])
    known = set(ros2_control_joints(robot))
    assert named <= known, f"controller names unknown to ros2_control: {named - known}"
    assert len(named) == 4, "four wheels, two per side"


def test_each_wheel_declares_the_interfaces_the_controller_needs(robot):
    for name, joint in ros2_control_joints(robot).items():
        commands = {c.get("name") for c in joint.findall("command_interface")}
        states = {s.get("name") for s in joint.findall("state_interface")}
        assert "velocity" in commands, f"{name}: no velocity command interface"
        assert {"position", "velocity"} <= states, f"{name}: missing state interfaces"


def test_required_hardware_parameters_are_present(robot):
    """on_init() uses .at(), so a missing param throws at startup."""
    block = robot.find("ros2_control")
    hw = block.find("hardware")
    present = {p.get("name") for p in hw.findall("param")}
    assert REQUIRED_HW_PARAMS <= present, f"missing: {REQUIRED_HW_PARAMS - present}"


def test_required_per_joint_parameters_are_present(robot):
    for name, joint in ros2_control_joints(robot).items():
        present = {p.get("name") for p in joint.findall("param")}
        assert REQUIRED_JOINT_PARAMS <= present, \
            f"{name} missing: {REQUIRED_JOINT_PARAMS - present}"
