# MCU second-hop OTA validation candidate (2026-11-28)

This branch intentionally creates a new immutable firmware identity from the
already-built `3919f7ba90e51035b4300e87dcb1967d7ec345e0` source tree.

## Purpose

The device has now been directly refreshed with the new ESP32 firmware and
MCU `0.0.0-3919f7`. This candidate is a controlled second-hop OTA experiment:
it changes only the Git-derived firmware identity, not the MCU application
logic. The resulting `screen_firmware.bin` is therefore a different image
(version/hash) while preserving the exact 3919f7 runtime behavior.

This isolates whether a running 3919f7 MCU can stage, verify, activate, reset,
and boot a subsequent MCU image. It is not a claimed fix for the original
first-upgrade hang.

## Expected identity

The build pipeline derives the displayed version from this commit:
`0.0.0-<new commit short SHA>`. The build manifest and SHA256SUMS are the
authoritative identity and integrity record.

## Test procedure

1. Keep the already-installed ESP32 firmware unchanged.
2. Confirm the current MCU reports `0.0.0-3919f7`.
3. Upload only the release's `screen_firmware.bin` through the G3 screen MCU
   update page. Do not flash the recovery image, ELF, ESP32 image, or ALL image.
4. Keep stable power connected and allow the full install/boot confirmation
   window to finish; do not power-cycle during the update.
5. Confirm the MCU reports the new commit-derived version and that sensors,
   display, and ESP32-to-MCU communication return to normal.

## Interpretation

- New version reported and normal services recover: the second-hop OTA path
  from 3919f7 works; the earlier first-hop hang is isolated to the older
  starting MCU / first-install handoff path.
- Transfer/staging completes but installation remains pending or times out:
  capture ESP32 `screen_image`, `screen_recovery`, and MCU diagnostics,
  especially `stage`, `previous_stage`, `reason`, and reset/fault fields.
- MCU goes offline and does not return: do not repeat the OTA blindly; preserve
  the device state and use the recovery/J-Link path if required.

This candidate is for controlled diagnosis only. CI compilation is not a
substitute for physical-board OTA validation.
