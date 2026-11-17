# 蓝牙寻物探测器 v1.0.1 发布说明

[English](RELEASE_NOTES_v1.0.1.md) · [v1.0.0 发布说明](RELEASE_NOTES.zh_CN.md)

这是一个稳定性修复版本。它回应 AI Passport 社区审核意见「优化 LVGL 任务逻辑，
适当让出 CPU 给 IDLE，或合理调整看门狗超时阈值，避免画面卡住」，同时修复实机
反馈的三个问题：列表不停上下跳、只显示 MAC 地址、点进追踪页经常显示无信号。

## 修复内容

- **LVGL / IDLE 饿死**：`app_main()` 原来收到第一个事件后就把队列等待时间置 0
  并持续排空，BLE 广播密集时主任务永远不让出 CPU，IDLE 与 LVGL 刷新任务被饿死。
  现在每轮最多处理 8 个事件、循环末尾固定 `vTaskDelay(1)`；扫描回调按设备地址
  约 10 Hz 合并广播，事件队列扩到 64。
- **列表乱跳、OK 锁错设备**：排序不再使用实时 RSSI，改用只有持续变化超过 12 dB
  才更新的冻结排序键，同分按首次发现顺序；光标绑定设备地址而非行号。列表淘汰更保守，
  且绝不淘汰已锁定目标。
- **只有 MAC 地址**：新增容错的 AD 结构解析模块 `device_label.c`，广播数据截断或
  无法识别时仍保留 RSSI；没有 Local Name 时按厂商数据（含 Apple 配对广播、Find My）、
  Appearance、16-bit 服务 UUID 推断 ASCII 设备标签，显示为 `Apple Earbuds 4C:19`
  这类可读名称；扫描响应里的名称会合并进来。
- **锁定后「无信号」**：目标改用独立信号滤波器，并在锁定时继承扫描页已采集的样本，
  不再受 12 行列表淘汰影响。新鲜度窗口放宽到 4 / 5 / 6.5 秒，适配手机、耳机、
  防丢标签 1–3 秒的广播间隔；追踪页区分「正在锁定 / 信号不稳 / 离开范围」三种状态。
- **健壮性**：短名 / 全名合并、随机私有地址轮换重绑定、标签截断、纯 ASCII 标签等
  细节一并收紧。

## 验证

- `./tests/run_host_tests.sh`：全部主机测试通过，含两个新增回归测试。
- `./tools/validate.sh --static`：仓库、字形、BSP、音频、demo、电池与固件布局检查通过。
- GitHub Actions `Firmware checks`：ESP-IDF 5.5.3 / esp32c3 执行
  `./tools/validate.sh --firmware` 并上传合并镜像。此前所有 shell 脚本在 Git 中缺少
  可执行位，CI 一直报 `Permission denied`，本次已修正。
- 新增回归测试：`tests/test_finder_stability.c`、`tests/test_device_label.c`。

## 固件产物

合并镜像为 `release/FoloToy-AI-Passport-full.bin`，大小与 SHA-256 见
`release/SHA256SUMS.txt`。

## 其他

社区中文标题改为「蓝牙寻物探测器」，英文标题仍为 BLE Signal Finder；三份 README
已同步。v1.0.0 发布说明保留在独立文件中，未删改。
