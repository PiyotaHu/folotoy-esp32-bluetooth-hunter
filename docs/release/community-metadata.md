# AI Passport Community Publishing Metadata (v1.0.1)

[简体中文](community-metadata.zh_CN.md)

This file collects the fields and assets used for
`https://ai-passport.folotoy.cn`. Releases use the official one-time grant
flow: the creator supplies a code matching the operation, the assistant
exchanges it with `authorize --code-stdin`, and `whoami` decides whether the
grant allows creating a new play or updating an existing one. Credentials live
outside the repository and are never committed. Category and tags are assigned
by the server-side AI, not chosen by the creator.

## Basic information

| Field | Current value |
| --- | --- |
| Chinese title | Bluetooth item finder (exact wording in the Chinese peer document) |
| English title | BLE Signal Finder |
| Version | 1.0.1 |
| Project ID | 2361 (`slug: community-c22a387c`) |
| Source repository | https://github.com/PiyotaHu/folotoy-esp32-bluetooth-hunter |
| Category / tags | Assigned by the server-side AI; do not set manually |
| Author / maintainer | Lao Hu (PiyotaHu) |

## Summary

**English summary (short):**

> Find earbuds, phones, and BLE tags by signal strength without pairing or connecting. A stable nearby list, readable device labels, audio and text cues, and faster beeps as you get closer.

The Chinese summary is in the linked Chinese peer document.

## Detailed description

See [README.en.md](../../README.en.md), [README.md](../../README.md) and the
[v1.0.1 release notes](RELEASE_NOTES_v1.0.1.md).

## Publishing assets

| Asset | Path |
| --- | --- |
| Merged firmware | `release/FoloToy-AI-Passport-full.bin` |
| Firmware checksum | `release/SHA256SUMS.txt` |
| Portrait 3:4 cover | `release/cover_3x4.jpg` |
| Chinese instructions | `--instructions-zh-file` (UTF-8 text) |
| English instructions | `--instructions-en-file` (UTF-8 text) |
| Chinese changelog | `--changelog-zh-file` (UTF-8 text) |
| English changelog | `--changelog-en-file` (UTF-8 text) |

## Pre-publish checklist

- [x] Description, instructions and changelog submitted as separate fields; no changelog in the description.
- [x] Instructions limited to 3-5 steps covering setup, buttons, core operation and exit/resume.
- [x] Portrait cover and extra images visually inspected, with no generating/loading/error placeholders.
- [x] Public HTTPS source URL points at this release's own repository.
- [x] `publisher.py validate` checks firmware and cover format, size and dimensions.
- [x] `projects` queried after submission; no second play is created to retry.
- [ ] Creator obtains an `update_once` one-time code matching project 2361.
- [ ] Resubmit with `submit --project-id 2361 --auto` and report the real review status.

## Scope

`publisher.py validate` checks format, size and dimensions only; it does not
verify image content. `submissionResult.status` is a cached receipt value; the
real review state is `projects[].status`. `pending` means awaiting review, not
publicly published.

The v1.0.0 notes remain in [RELEASE_NOTES.md](RELEASE_NOTES.md), unchanged.
