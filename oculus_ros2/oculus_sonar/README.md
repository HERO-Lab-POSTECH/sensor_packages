# oculus_sonar

ROS2 Humble driver for Blue Print Subsea Oculus multibeam imaging sonars (M750d, M1200d, M3000d), with optional Cartesian fan-image converter.

## Components

- `oculus_sonar::OculusDriver` — Connects to sonar over UDP, publishes ping data and metadata.
- `oculus_sonar::OculusFanImager` — Subscribes to polar `SonarImage` and publishes Cartesian `Image` (optional colormap).

Both are `rclcpp_components` (composition-friendly) and also installed as standalone executables (`oculus_driver_node`, `oculus_fan_imager_node`).

## Quickstart

```bash
# Driver only (M750d / M1200d / M3000d):
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d

# Driver + fan imager in same composition container (intra-process zero-copy):
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d with_fan:=true colormap:=viridis
```

## Topics Published

| Topic | Type | Description |
|-------|------|-------------|
| `/sensor/sonar/oculus/{model}/sonar` | `marine_acoustic_msgs/SonarImage` | Polar sonar image |
| `/sensor/sonar/oculus/{model}/image` | `sensor_msgs/Image` | Polar grayscale (legacy view) |
| `/sensor/sonar/oculus/{model}/metadata` | `oculus_sonar_msgs/OculusMetadata` | Per-ping metadata |
| `/sensor/sonar/oculus/{model}/raw_data` | `apl_msgs/RawData` | Raw bytes (debug/replay) |
| `/sensor/sonar/oculus/{model}/fan_image` | `sensor_msgs/Image` | Cartesian fan image (when `with_fan:=true`) |

## Launch Arguments

| Arg | Default | Description |
|-----|---------|-------------|
| `sonar_model` | `m750d` | One of `m750d`, `m1200d`, `m3000d` |
| `with_fan` | `false` | Add `OculusFanImager` to the same composition container |
| `ip_address` | from yaml | Sonar IP (or `auto` for auto-detect) |
| `range` | from yaml | Range in meters |
| `gain` | from yaml | Gain percent (1-100) |
| `gamma` | from yaml | Gamma (0-255) |
| `ping_rate` | from yaml | 0=Normal, 1=High, 2=Highest, 3=Low, 4=Lowest |
| `freq_mode` | from yaml | 1=low, 2=high |
| `data_size` | from yaml | `8bit` / `16bit` / `32bit` |
| `apply_colormap` | `true` | Fan imager: apply colormap |
| `colormap` | `turbo` | Fan imager colormap (jet, viridis, plasma, …) |
| `qos_reliability` | `best_effort` | Fan imager subscriber QoS (`reliable` / `best_effort`) |

Empty string for any sonar parameter means "use the value from `config/{sonar_model}_params.yaml`".

## Configuration Files

`config/{m750d,m1200d,m3000d}_params.yaml` — per-model defaults. Override at launch time via the arguments above.

## Dynamic Parameter Changes

```bash
ros2 param set /oculus/fan_imager colormap viridis
ros2 param set /oculus/fan_imager apply_colormap false
```

## Build

```bash
cd ros2_ws && colcon build --packages-select oculus_sonar
source install/setup.bash
```

Depends on `liboculus` (vendored under `oculus_ros2/liboculus`).

## Troubleshooting

### Connection Issues

```bash
# Check sonar connectivity
ping <sonar_ip_address>

# Monitor raw communication data
ros2 topic echo /sensor/sonar/oculus/m750d/raw_data

# Verify topic publishing
ros2 topic hz /sensor/sonar/oculus/m750d/sonar
```

### Node Status

```bash
ros2 node list
ros2 node info /oculus/m750d
```

### Visualization (rviz2)

```bash
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d with_fan:=true
rviz2
# In rviz2:
# 1. Add Image display
# 2. Polar image: /sensor/sonar/oculus/m750d/image
# 3. Fan image:   /sensor/sonar/oculus/m750d/fan_image
```

## License

MIT License — see the LICENSE file for details.
