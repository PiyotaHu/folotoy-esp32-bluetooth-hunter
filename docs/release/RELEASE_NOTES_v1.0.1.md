# BLE Signal Finder v1.0.1 Release Notes

[简体中文](RELEASE_NOTES_v1.0.1.zh_CN.md) · [v1.0.0 notes](RELEASE_NOTES.md)

This is a stability release. It answers the AI Passport community review note
("optimise the LVGL task logic, yield CPU to IDLE, or adjust the watchdog
timeout so the screen does not freeze") and fixes three issues found on real
hardware: a list that kept jumping, rows showing only MAC addresses, and a
tracking screen that often reported no signal.

## Fixes

- **LVGL / IDLE starvation.** `app_main()` previously set the queue wait to
  zero after the first event and drained the queue forever while dense BLE
  traffic kept it full, starving the IDLE task and the LVGL refresh task.
  The loop now drains at most 8 events per pass and unconditionally calls
  `vTaskDelay(1)` afterwards. `ble_scan.c` coalesces advertisements per
  address at about 10 Hz, and the event queue is 64 entries deep.
- **Jumping list and wrong target on OK.** Ordering no longer uses live RSSI.
  A frozen sort key moves only after a sustained 12 dB change, ties break on
  first-seen order, and the cursor is pinned to a device address instead of a
  row index. List eviction is conservative and never removes a locked target.
- **MAC-only rows.** A new total AD-structure decoder (`device_label.c`)
  always produces a usable report, even for truncated or unknown advertising
  data. When no Local Name is broadcast it infers an ASCII label from
  manufacturer data (including Apple proximity pairing and Find My),
  Appearance and 16-bit service UUIDs, so rows read `Apple Earbuds 4C:19`
  instead of a bare MAC. Names carried in scan responses are merged in.
- **"No signal" after locking.** The target now uses an independent signal
  filter seeded from the samples already collected on the scan screen, so it
  survives eviction from the 12-row nearby list. Freshness windows were raised
  to 4 / 5 / 6.5 s because idle phones, earbuds and tags advertise every 1-3 s.
  The tracking view distinguishes acquiring, unstable and out-of-range states.
- **Robustness.** Shortened/complete names, private-address rotation rebinding,
  shorter-label truncation and ASCII-only labels were tightened up.

## Validation

- `./tests/run_host_tests.sh` - all host tests pass, including the two new
  regression suites.
- `./tools/validate.sh --static` - repository, glyph, BSP, audio, demo,
  battery and firmware-layout checks pass.
- `Firmware checks` GitHub Actions workflow builds with ESP-IDF 5.5.3 /
  esp32c3 using `./tools/validate.sh --firmware` and uploads the merged
  image. Shell scripts were marked executable so CI can actually run them
  (earlier runs failed with `Permission denied`).
- New host tests: `tests/test_finder_stability.c`, `tests/test_device_label.c`.

## Firmware artifact

See `release/SHA256SUMS.txt` for the size and SHA-256 of the merged
`release/FoloToy-AI-Passport-full.bin` produced by CI.

## Notes

The community-facing Chinese title was changed to a more literal
"Bluetooth item finder" name (see the Chinese peer document for the exact
wording); the English title remains BLE Signal Finder. README files were
updated to match. v1.0.0 release notes are kept unchanged in their own file.
