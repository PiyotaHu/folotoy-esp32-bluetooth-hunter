# AI Passport 社区发布元数据（v1.0.1）

[English](community-metadata.md)

本文件汇总发布到 `https://ai-passport.folotoy.cn` 的字段与资产。当前版本使用官网
一次性授权码（one-time grant）流程：作者提供与操作匹配的一次性授权码，助手通过
`authorize --code-stdin` 兑换后按 `whoami` 的 scope 决定新建或更新；凭据保存在仓库外，
不写入提交内容。分类与标签由服务端 AI 判定，不需要作者选择。

## 基本信息

| 字段 | 当前值 |
| --- | --- |
| 中文标题 | 蓝牙寻物探测器 |
| English title | BLE Signal Finder |
| 版本 | 1.0.1 |
| 项目 ID | 2361（`slug: community-c22a387c`） |
| 源码仓库 | https://github.com/PiyotaHu/folotoy-esp32-bluetooth-hunter |
| 分类 / 标签 | 由服务端 AI 自动判定，不手工指定 |
| 作者 / 维护者 | 老胡（PiyotaHu） |

## 简介

**中文简介（短）：**

> 无需配对，跟着蓝牙信号强弱找到耳机、手机和 BLE 标签。列表稳定、设备名可读，声音与文字双重提示，越近提示音越密。

**English summary (short):**

> Find earbuds, phones, and BLE tags by signal strength without pairing or connecting. A stable nearby list, readable device labels, audio and text cues, and faster beeps as you get closer.

## 详细介绍

见 [README.zh_CN.md](../../README.zh_CN.md)、[README.en.md](../../README.en.md) 与
[v1.0.1 发布说明](RELEASE_NOTES_v1.0.1.zh_CN.md)。

## 发布资产

| 资产 | 路径 |
| --- | --- |
| 合并固件 | `release/FoloToy-AI-Passport-full.bin` |
| 固件校验 | `release/SHA256SUMS.txt` |
| 竖版 3:4 封面 | `release/cover_3x4.jpg` |
| 中文使用方法 | `--instructions-zh-file`（UTF-8 文本文件） |
| 英文使用方法 | `--instructions-en-file`（UTF-8 文本文件） |
| 中文更新日志 | `--changelog-zh-file`（UTF-8 文本文件） |
| 英文更新日志 | `--changelog-en-file`（UTF-8 文本文件） |

## 发布前检查清单

- [x] 简介、使用方法、更新日志分字段提交，更新日志不写进简介。
- [x] 使用方法 3–5 条，只写必要准备、按键、开始与核心操作、退出或续玩。
- [x] 竖版 3:4 封面与额外图片逐张人工查看，确认无生成中 / 加载失败占位内容。
- [x] 公开 HTTPS 源码仓库填写当前发布版本自己的仓库。
- [x] `publisher.py validate` 校验固件与封面格式、大小、尺寸。
- [x] 提交后查询 `projects` 复核，不为重试另建玩法。
- [ ] 作者从创作者中心获取与项目 2361 匹配的 `update_once` 一次性授权码。
- [ ] 以 `submit --project-id 2361 --auto` 重新提交，并如实报告实际审核状态。

## 边界说明

`publisher.py validate` 只校验文件格式、大小与尺寸，不能代替图片内容人工检查。
`submissionResult.status` 是提交回执的缓存值，真实审核状态以 `projects[].status`
为准；`pending` 表示待审核，不等于已公开。

v1.0.0 的发布说明保留在 [RELEASE_NOTES.zh_CN.md](RELEASE_NOTES.zh_CN.md)，未删改。
