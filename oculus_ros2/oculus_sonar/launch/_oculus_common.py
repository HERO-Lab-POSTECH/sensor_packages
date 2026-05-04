"""
Internal launch helpers for oculus_sonar.

Used by oculus.launch.py. Not part of the public API — the leading underscore
indicates module-private status. Callers pass plain Python types only (no
LaunchConfiguration objects); the launch entry-point is responsible for
resolving launch arguments via .perform(context).
"""

import os
from ament_index_python.packages import get_package_share_directory
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode

_VALID_MODELS = ('m750d', 'm1200d', 'm3000d')


def sonar_model_to_config_path(model: str) -> str:
    """Resolve the config YAML path for the given sonar model.

    Raises FileNotFoundError (with the valid model whitelist in the message)
    when the YAML is missing — typically because the user passed a typo'd
    model name like 'm700d'.
    """
    config_file = os.path.join(
        get_package_share_directory('oculus_sonar'),
        'config',
        f'{model}_params.yaml',
    )
    if not os.path.exists(config_file):
        raise FileNotFoundError(
            f"Config file not found: {config_file}. "
            f"Valid models: {', '.join(_VALID_MODELS)}"
        )
    return config_file


def topic_namespace(model: str) -> str:
    """Return the per-model topic prefix, e.g. '/sensor/sonar/oculus/m750d'."""
    return f'/sensor/sonar/oculus/{model}'


def make_sonar_container(
    model: str,
    with_fan: bool,
    driver_overrides: dict,
    fan_imager_overrides: dict,
    *,
    container_name: str = None,
) -> ComposableNodeContainer:
    """Build the ComposableNodeContainer holding the OculusDriver and
    (optionally) the OculusFanImager.

    Args:
        model: One of m750d/m1200d/m3000d. Caller must validate.
        with_fan: When True, adds OculusFanImager composable node.
        driver_overrides: Driver param overrides. Empty dict means use
            config-file defaults only.
        fan_imager_overrides: Fan imager param overrides. Ignored when
            with_fan is False; required keys when True: 'input_topic',
            'output_topic', 'apply_colormap', 'colormap', 'qos_reliability'.
            Optional: 'freq_mode'.
        container_name: Override the auto-generated container node name.
            Defaults to f'oculus_{model}_container'.
    """
    config_file = sonar_model_to_config_path(model)

    driver_params = [config_file]
    if driver_overrides:
        driver_params.append(driver_overrides)

    composable_nodes = [
        ComposableNode(
            package='oculus_sonar',
            plugin='oculus_sonar::OculusDriver',
            name=model,
            namespace='oculus',
            parameters=driver_params,
        ),
    ]

    if with_fan:
        composable_nodes.append(
            ComposableNode(
                package='oculus_sonar',
                plugin='oculus_sonar::OculusFanImager',
                name='fan_imager',
                namespace='oculus',
                parameters=[config_file, fan_imager_overrides],
            )
        )

    return ComposableNodeContainer(
        name=container_name or f'oculus_{model}_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        composable_node_descriptions=composable_nodes,
        output='screen',
    )
