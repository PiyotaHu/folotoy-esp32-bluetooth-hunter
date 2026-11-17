# AI Passport Community Publishing Metadata (use only after author confirmation)

This file collects the fields required to publish to `https://ai-passport.folotoy.cn`. Per the official workflow, every field must be shown to the author for confirmation before anything is uploaded.

[简体中文](community-metadata.zh_CN.md)

## Basic information

| Field | Proposed value |
| --- | --- |
| Chinese title | To be filled in (see the Chinese peer document) |
| English title | BLE Signal Finder |
| Version | 1.0.0 |
| Category | Utility / Finder |
| Tags | Bluetooth, BLE, RSSI, finder, filtering, open source |
| Author / maintainer | To be filled in |

## Summary

**English summary (short):**

> Find earbuds, phones, and BLE tags by signal strength without pairing or connecting. Median + EMA filtering, audio and text cues, and faster beeps as you get closer.

The Chinese summary is in the linked Chinese peer document.

## Detailed description

See [README.en.md](../../README.en.md) and [README.md](../../README.md).

## Publishing assets

| Asset | Path |
| --- | --- |
| Merged firmware | `build/FoloToy-AI-Passport-full.bin` |
| Firmware SHA-256 | `b9589914dbf130234dc61cf6fffcdc626b0efd318d7d16b57c91247218a1e78a` |
| Cover image | `assets/cover.png` (PNG, 1200×675, 167 KiB) |
| Source repository | **To be filled in**: a publicly reachable HTTPS Git URL is required |
| Release notes | [RELEASE_NOTES.md](RELEASE_NOTES.md) |

## Pre-publish checklist

- [ ] Author confirms the Chinese and English titles and summaries.
- [ ] Author confirms the cover image.
- [ ] Author provides and confirms a public HTTPS Git repository URL.
- [ ] Author confirms the firmware hash and version.
- [ ] Author signs in on the official site and personally approves the displayed code.
- [ ] Author explicitly authorizes the upload to the community site.
- [ ] After publishing, decide whether to archive the app upstream in `docs/reference/` per the official process.

## Scope

This repository only prepares publishing materials; it does not perform the upload. Publishing is an external change and must be authorized by the author. The assistant never asks for, receives, or stores credentials.
