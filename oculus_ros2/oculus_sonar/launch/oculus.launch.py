#!/usr/bin/env python3
"""
oculus.launch.py — Unified ROS2 launch entry point for all Oculus sonar models.

See `oculus_sonar/README.md` for the full argument table, examples, and topic
list. This file declares launch arguments and delegates container assembly to
helpers in `_oculus_common.py`.
"""

import os
import sys

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration

# Make sibling _oculus_common importable when this file is executed by ros2 launch
# (ros2 launch sets __file__ to the install/share/.../launch/ path).
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))
from _oculus_common import make_sonar_container, topic_namespace  # noqa: E402


def _collect_driver_overrides(context) -> dict:
    """Read driver-param launch arguments and return a dict of only the
    non-empty ones, with appropriate type conversion.

    Empty string ('') means "fall back to config YAML default".

    The (name, converter) order below MIRRORS the original launch file
    lines 124-151 for code-review traceability — change with care.
    """
    args_with_type = (
        ('ip_address', str),
        ('range',      float),
        ('gain',       int),
        ('gamma',      int),
        ('ping_rate',  int),
        ('freq_mode',  int),
        ('data_size',  str),
    )
    overrides = {}
    for name, conv in args_with_type:
        v = LaunchConfiguration(name).perform(context)
        if v:
            overrides[name] = conv(v)
    return overrides


def _collect_fan_imager_overrides(context, model: str) -> dict:
    """Build the fan_imager param dict using launch args + the resolved model
    name. Always includes input/output topic, colormap, and qos_reliability;
    optionally includes freq_mode if the launch-arg version is non-empty.
    """
    apply_colormap = LaunchConfiguration('apply_colormap').perform(context)
    colormap = LaunchConfiguration('colormap').perform(context)
    qos_reliability = LaunchConfiguration('qos_reliability').perform(context)
    freq_mode = LaunchConfiguration('freq_mode').perform(context)

    ns = topic_namespace(model)
    overrides = {
        'input_topic': f'{ns}/sonar',
        'output_topic': f'{ns}/fan_image',
        'apply_colormap': apply_colormap.lower() == 'true' if apply_colormap else True,
        'colormap': colormap if colormap else 'turbo',
        'qos_reliability': qos_reliability if qos_reliability else 'reliable',
    }
    if freq_mode:
        overrides['freq_mode'] = int(freq_mode)
    return overrides


def launch_setup(context, *args, **kwargs):
    model = LaunchConfiguration('sonar_model').perform(context)
    with_fan = LaunchConfiguration('with_fan').perform(context).lower() == 'true'

    driver_overrides = _collect_driver_overrides(context)
    fan_imager_overrides = _collect_fan_imager_overrides(context, model) if with_fan else {}

    container = make_sonar_container(
        model=model,
        with_fan=with_fan,
        driver_overrides=driver_overrides,
        fan_imager_overrides=fan_imager_overrides,
    )
    return [container]


def generate_launch_description():
    return LaunchDescription([
        # Model selection
        DeclareLaunchArgument('sonar_model', default_value='m750d',
                              description='Sonar model: m750d, m1200d, m3000d'),
        DeclareLaunchArgument('with_fan', default_value='false',
                              description='Enable fan image converter: true/false'),

        # Driver parameter overrides (empty = use config YAML)
        DeclareLaunchArgument('ip_address', default_value='',
                              description='Sonar IP address (empty = use config)'),
        DeclareLaunchArgument('range', default_value='',
                              description='Sonar range in meters (empty = use config)'),
        DeclareLaunchArgument('gain', default_value='',
                              description='Gain percentage (empty = use config)'),
        DeclareLaunchArgument('gamma', default_value='',
                              description='Gamma correction (empty = use config)'),
        DeclareLaunchArgument('ping_rate', default_value='',
                              description='Ping rate (empty = use config)'),
        DeclareLaunchArgument('freq_mode', default_value='',
                              description='Frequency mode: 1=low, 2=high (empty = use config)'),
        DeclareLaunchArgument('data_size', default_value='',
                              description='Data size: 8bit, 16bit, 32bit (empty = use config)'),

        # Fan imager parameters
        DeclareLaunchArgument('apply_colormap', default_value='true',
                              description='Apply colormap to fan image: true/false'),
        DeclareLaunchArgument('colormap', default_value='turbo',
                              description='Colormap: jet, hot, viridis, turbo, plasma, magma, '
                                          'inferno, ocean, rainbow, etc.'),
        DeclareLaunchArgument('qos_reliability', default_value='best_effort',
                              description='QoS reliability for subscriber: reliable or best_effort'),

        OpaqueFunction(function=launch_setup),
    ])
