# Khôi phục sync ESP32 cho Livox Horizon — ROS 2 Jazzy

Driver có lại bridge UTC dùng cùng tiến trình Livox SDK với driver LiDAR.
Giao thức tương thích node `horizon_flir_sync/sync_camera` đã dùng với bộ
Horizon + ESP32 + FLIR. Package camera/firmware ESP32 thuộc `horizon_flir_sync`
và cần có sẵn trong workspace hoặc môi trường đã source.

## Cài vào workspace đang dùng

Giải nén ZIP vào thư mục tạm, rồi áp patch vào repo hiện có. Với repo laptop
ở `~/my_ws/data_ws/src/livox_ros2_driver`:

```bash
unzip -o ~/Downloads/livox_ros2_driver_repo_sync.zip -d /tmp/horizon_driver_sync
cd ~/my_ws/data_ws/src/livox_ros2_driver
patch --dry-run -p1 < /tmp/horizon_driver_sync/livox_ros2_driver_repo/restore_horizon_sync.patch
```

Nếu dry-run không báo lỗi, áp patch một lần:

```bash
patch -p1 < /tmp/horizon_driver_sync/livox_ros2_driver_repo/restore_horizon_sync.patch
```

Với desktop, đổi đường dẫn repo thành `~/my_ws/fastlio_ws/src/livox_ros2_driver_repo`.
Patch được tạo theo đúng ZIP bạn gửi. Nếu file nguồn đã thay đổi khác bản ZIP,
dừng ở kết quả dry-run để đối chiếu, không ép áp patch hay ghi đè `.rej`.

Gói cũng chứa toàn bộ source đã sửa và không mang thư mục `.git`. Có thể chép
nội dung vào repo tương ứng, nhưng không giải nén thêm repo trùng package vào
`src` cạnh repo đang dùng.

Repo giữ nguyên message LiDAR `livox_ros_driver2/msg/CustomMsg` như bản bạn gửi.
Package message `livox_ros_driver2` cần có trong workspace hoặc underlay đã source.
Thư mục `livox_interfaces` trong repo gốc được giữ nguyên, không đổi loại message.

```bash
cd ~/my_ws/data_ws
source /opt/ros/jazzy/setup.bash
colcon build --symlink-install --packages-up-to livox_ros2_driver
source install/setup.bash
```

Nếu dùng workspace desktop, thay `~/my_ws/data_ws` bằng `~/my_ws/fastlio_ws`.
Môi trường chạy node camera phải có PySpin và thư viện Spinnaker như trước.

## Chạy đồng bộ cả LiDAR và camera

Với package `horizon_flir_sync` đã cài, dùng launch chung như trước:

```bash
ros2 launch horizon_flir_sync hardware_sync.launch.py \
  camera_serial:=25465415 \
  serial_port:=/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0
```

Launch chung phải truyền `esp_sync_enabled: true` vào node `livox_ros2_driver`.
Launch chung cũ đã có tham số này. Chỉ chạy một tiến trình driver LiDAR tại một thời điểm.

## Chạy riêng driver để kiểm tra bridge

```bash
ros2 launch livox_ros2_driver livox_horizon_jazzy.launch.py \
  esp_sync_enabled:=true
```

Mặc định tham số là `false` để chạy LiDAR độc lập như cũ. Khi đặt `true`,
`/sync/livox_ready` có thể báo sẵn sàng sau khi nhận gói từ Horizon, nhưng
LiDAR/IMU chỉ được xuất sau khi node sync gửi RUN, UTC hợp lệ và qua thời gian
khởi động đồng bộ. Chạy riêng driver chưa tạo xung ESP32 hay kích camera.

## Các topic được khôi phục

| Topic | Kiểu | Vai trò |
| --- | --- | --- |
| `/sync/livox_ready` | `std_msgs/msg/Bool` | Driver đã nhận gói Horizon gần đây, chỉ một thiết bị |
| `/sync/livox_status` | `std_msgs/msg/String` | `timestamp_type`, số gói accepted/rejected, multiple_devices |
| `/sync/utc_ack` | `std_msgs/msg/UInt64` | Epoch ns mà SDK đã xác nhận lệnh UTC |
| `/sync/utc` | `std_msgs/msg/UInt64MultiArray` | Nhận `[epoch_ns, sent_monotonic_ns, pps_monotonic_ns]` từ node sync |
| `/sync/run` | `std_msgs/msg/Bool` | Nhận trạng thái phiên chạy, QoS transient-local |

`/sync/status` và `/sync/frame_metadata` được publish bởi node camera/sync,
không phải driver LiDAR.

```bash
ros2 param get /livox_horizon esp_sync_enabled
ros2 topic echo /sync/livox_ready --once
ros2 topic echo /sync/livox_status --once
ros2 topic echo /sync/utc_ack --once
```

Sau khi launch chung đã RUNNING và qua khởi động, kỳ vọng `timestamp_type=3`,
`accepted` tăng, `multiple_devices=0`, `utc_ack` là epoch ns hiện tại.

File `config/livox_horizon_jazzy.json` giữ `timesync_config.enable_timesync=false`:
bridge ESP nhận UTC qua ROS, không dùng bộ timesync UART/GPS cũ của SDK song song.

## Phạm vi thay đổi và kiểm tra

- Khôi phục `esp_sync.h`, publisher/subscriber/timer và cổng kiểm tra gói LiDAR/IMU.
- Đặt con trỏ callback trước khi SDK bắt đầu luồng nhận; khởi tạo trạng thái timesync.
- Thêm launch argument `esp_sync_enabled` với kiểu bool rõ ràng.
- Giữ các thay đổi sẵn có của bản gửi: UTC qua `timegm`, CustomMsg và dừng worker.

Đã kiểm tra logic UTC/gating bằng chương trình C++ dùng header SDK với SDK I/O mô phỏng,
cả hai cách viết trường `microsecond`/`mircrosecond`. Chưa build toàn bộ ROS 2 hay
kiểm thử cảm biến tại môi trường tạo gói vì không có ROS 2/thiết bị tại đó.
