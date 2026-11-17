# 蓝牙信号猎手 v1.0.0 发布说明

[English](RELEASE_NOTES.md)

## 本版本包含什么

- 面向 FoloToy AI Passport 的完整可烧录固件。
- BLE Observer 被动扫描，不连接、不配对、不绑定。
- 中值滤波 + Q8 EMA + 滞回 + 超时判定，五档信号强度。
- 趋势箭头（更近 / 更远 / 稳定）与 dB 变化量。
- 程序化提示音与中文文字提示；越靠近提示音越密。
- 扫描列表、追踪页、设置页三屏交互。
- 目标与偏好 NVS 持久化；随机 MAC 名称匹配可选项。
- 中文字体子集、社区封面、双语 README 与发布元数据。

## 固件产物

| 项目 | 值 |
| --- | --- |
| 合并镜像 | `build/FoloToy-AI-Passport-full.bin` |
| 大小 | 1,013,328 字节 |
| SHA-256 | `b9589914dbf130234dc61cf6fffcdc626b0efd318d7d16b57c91247218a1e78a` |
| 烧录偏移 | `0x0` |
| 应用分区 | `factory` @ `0x10000` |
| 应用大小 | 947,792 字节 |
| 调试归档 | `build/firmware/b9589914dbf130234dc61cf6fffcdc626b0efd318d7d16b57c91247218a1e78a/` |

## 验证结果

```text
Build: PASS
Host tests: PASS
Device tests: NOT RUN
Unverified: 真实 RSSI 曲线、天线方向性、扬声器听感、电量读数、长期稳定性；
            actionlint 在离线模式下跳过，GitHub Actions YAML 未做语法检查。
```

- `idf.py build` 成功。
- `./tools/validate.sh --static` 在 `ACTIONLINT=offline` 下全部通过，仅 actionlint 标记为未运行。
- `./tools/validate.sh --firmware` 全量构建、合并镜像、布局校验、归档全部通过。
- 3 个应用主机测试和完整走位模拟器通过。
- 第三方 `developing-ai-passport/check.sh` 自检通过。
- 未进行真机烧录；任何设备测试都需要用户连接硬件并明确授权。

## 已知限制

- RSSI 受天线方向、人体遮挡、广播功率与环境影响，只能用于相对接近判断。
- 随机 MAC 设备开启名称匹配后，可能因重名或空名称出现误判。
- 没有连接设备，因此无法读取发射功率或精确距离。
- 提示音为程序化合成，音色偏工具化，不追求音乐性。

## 发布前仍需用户确认

1. 公开 HTTPS Git 仓库地址（社区发布必填）。
2. 中文 / 英文标题与简介是否采用元数据文件中的建议文案。
3. 封面图是否采用 `assets/cover.png`。
4. 是否授权上传到 `https://ai-passport.folotoy.cn`。
5. 是否授权后续真机烧录与实测。
