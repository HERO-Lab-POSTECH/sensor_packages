#!/usr/bin/env python3
"""
Unified ROS2 launch entry point for all Oculus sonar models.

Arguments:
  sonar_model    : Sonar model: m750d, m1200d, m3000d          (default: 'm750d')
  with_fan       : Launch fan_imager visualization node        (default: false)
  ip_address     : Oculus sonar IP (empty = config default)    (default: '')
  range          : Range override in meters                    (default: '')
  gain           : Gain override (0-100)                       (default: '')
  gamma          : Gamma override (0-255)                      (default: '')
  ping_rate      : Ping rate override (Hz, 0-40)               (default: '')
  freq_mode      : Frequency mode override                     (default: '')
  data_size      : Data size override                          (default: '')
  apply_colormap : Apply colormap to fan image                 (default: true)
  colormap       : Colormap name (jet, hot, viridis, turbo...) (default: 'turbo')
  qos_reliability: Fan imager subscriber QoS reliability       (default: 'best_effort')
  use_sim_time   : Honor /clock during bag replay              (default: false)

Empty string ('') for an override means "fall back to YAML config default".
Container assembly is delegated to `_oculus_common.py`.

Examples:
  ros2 launch oculus_sonar oculus.launch.py
  ros2 launch oculus_sonar oculus.launch.py sonar_model:=m1200d with_fan:=true
  ros2 launch oculus_sonar oculus.launch.py range:=20 gain:=50
"""

import os
import sys

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration

# Make sibling _oculus_common importable when this file is executed by ros2 launch
# (ros2 launch sets __file__ to the install/share/.../launch/ path).
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))
from _oculus_common import make_sonar_container, topic_namespace  # type: ignore[import-not-found]  # noqa: E402


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


def launch_setup(context, *_args, **_kwargs):
    model = LaunchConfiguration('sonar_model').perform(context)
    with_fan = LaunchConfiguration('with_fan').perform(context).lower() == 'true'
    use_sim_time = LaunchConfiguration('use_sim_time').perform(context).lower() == 'true'

    driver_overrides = _collect_driver_overrides(context)
    fan_imager_overrides = _collect_fan_imager_overrides(context, model) if with_fan else {}

    container = make_sonar_container(
        model=model,
        with_fan=with_fan,
        driver_overrides=driver_overrides,
        fan_imager_overrides=fan_imager_overrides,
        use_sim_time=use_sim_time,
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

        # Clock source (true = honor /clock during bag replay)
        DeclareLaunchArgument('use_sim_time', default_value='false',
                              description='Honor /clock during bag replay.'),

        OpaqueFunction(function=launch_setup),
    ])
