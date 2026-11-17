# AI Passport 社区发布元数据（待用户确认后使用）

[English](community-metadata.md)

本文件汇总发布到 `https://ai-passport.folotoy.cn` 所需的字段。按官方流程，所有字段必须先展示给作者确认，未经确认不得上传。

## 基本信息

| 字段 | 建议值 |
| --- | --- |
| 中文标题 | 蓝牙信号猎手 |
| English title | BLE Signal Finder |
| 版本 | 1.0.0 |
| 分类 | 实用工具 / 寻物 |
| 标签 | 蓝牙, BLE, RSSI, 寻物, 滤波, 开源 |
| 作者 / 维护者 | 待填写 |

## 简介

**中文简介（短）：**

> 无需配对，跟着蓝牙信号强弱找到耳机、手机和 BLE 标签。中值加 EMA 滤波，声音与文字双重提示，越近提示音越密。

**English summary (short):**

> Find earbuds, phones, and BLE tags by signal strength without pairing or connecting. Median + EMA filtering, audio and text cues, and faster beeps as you get closer.

## 详细介绍

见 [README.zh_CN.md](../../README.zh_CN.md) 与 [README.en.md](../../README.en.md)。

## 发布资产

| 资产 | 路径 |
| --- | --- |
| 合并固件 | `build/FoloToy-AI-Passport-full.bin` |
| 固件 SHA-256 | `b9589914dbf130234dc61cf6fffcdc626b0efd318d7d16b57c91247218a1e78a` |
| 封面 | `assets/cover.png`（PNG，1200×675，167 KiB） |
| 源码仓库 | **待填写**：需公开可达的 HTTPS Git 仓库地址 |
| 发布说明 | [RELEASE_NOTES.zh_CN.md](RELEASE_NOTES.zh_CN.md) |

## 发布前检查清单

- [ ] 用户确认中文 / 英文标题与简介。
- [ ] 用户确认封面图。
- [ ] 用户提供并确认公开 HTTPS Git 仓库地址。
- [ ] 用户确认固件哈希与版本。
- [ ] 用户在官网完成登录 / 授权，并亲自批准显示的代码。
- [ ] 用户明确授权上传到社区站点。
- [ ] 发布后再按官方流程决定是否归档到上游 `docs/reference/`。

## 边界说明

本仓库只准备发布材料，不执行上传。发布是外部变更，必须由作者确认后才可进行；助手不索取、接收或存储授权凭证。
