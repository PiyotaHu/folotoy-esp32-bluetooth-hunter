# BLE Signal Finder v1.0.0 Release Notes

[简体中文](RELEASE_NOTES.zh_CN.md)

## Highlights

- Complete flashable firmware for the FoloToy AI Passport.
- BLE Observer-only scanning: no connection, pairing, or bonding.
- Median filter + Q8 EMA + hysteresis + freshness timeout, with five signal levels.
- Trend arrow (closer / away / stable) and dB delta.
- Procedurally synthesized audio cues plus Chinese text hints; beeps get faster as you approach.
- Three-screen UI: Scan, Track, and Settings.
- NVS persistence for the locked target and preferences; optional name matching for rotating MACs.
- Chinese font subset, community cover image, bilingual READMEs, and publishing metadata.

## Firmware artifact

| Item | Value |
| --- | --- |
| Merged image | `build/FoloToy-AI-Passport-full.bin` |
| Size | 1,013,328 bytes |
| SHA-256 | `b9589914dbf130234dc61cf6fffcdc626b0efd318d7d16b57c91247218a1e78a` |
| Flash offset | `0x0` |
| App partition | `factory` @ `0x10000` |
| App size | 947,792 bytes |
| Debug archive | `build/firmware/b9589914dbf130234dc61cf6fffcdc626b0efd318d7d16b57c91247218a1e78a/` |

## Validation

```text
Build: PASS
Host tests: PASS
Device tests: NOT RUN
Unverified: real-world RSSI curves, antenna directionality, speaker output,
            battery readings, long-term stability; actionlint was skipped in
            offline mode, so GitHub Actions YAML was not syntax-checked.
```

- `idf.py build` passed.
- `./tools/validate.sh --static` passed with `ACTIONLINT=offline`; only actionlint is marked as not run.
- `./tools/validate.sh --firmware` passed: independent full build, merged image, layout verification, and debug archive.
- Three application host tests and the full walkthrough simulator passed.
- The third-party `developing-ai-passport/check.sh` self-check passed.
- No on-device flashing was performed; device testing requires hardware and explicit user authorization.

## Known limitations

- RSSI depends on antenna orientation, body blockage, advertising power, and environment. It is for relative approach only.
- Name matching can mislock on duplicate or empty advertised names.
- Without a connection, the app cannot read transmit power or compute an exact distance.
- Audio cues are programmatic and tool-like by design, not musical.

## Still requires user confirmation before publishing

1. A public HTTPS Git repository URL.
2. Whether to use the proposed Chinese and English titles and summaries.
3. Whether to use `assets/cover.png` as the cover image.
4. Explicit authorization to upload to `https://ai-passport.folotoy.cn`.
5. Explicit authorization for any physical flashing and on-device test.
