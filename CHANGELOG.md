# Changelog

All notable changes to **MynouPAD S3** are documented in this file.

## [7.2.0] - 2026-11-25
### Changed
- **Frosted Glass Web Theme**: Matched MynouPAD Studio's visual language to Home Assistant's `Frosted Glass` dark theme (`backdrop-filter: blur(10px) saturate(1.25)`, inset glass edge highlights, translucent keycaps, `#7AA2BE` primary palette, and dark frosted wallpaper with offline gradient fallback).

## [7.1.0] - 2026-11-25
### Added
- **Official Mascot Identity**: Converted the OLED boot `drawFace` pixel-art mascot into a crisp SVG emblem (`assets/mascot.svg` and firmware `/mascot.svg` endpoint).
- **Animated Web Studio Logo & Favicon**: Added `/mascot.svg` as browser favicon and animated blinking mascot header logo in MynouPAD Studio.
- **Home Assistant Avatar Integration**: Added `entity_picture` pointing to `/mascot.svg` in `sensor.mynoupad_status` attributes so Home Assistant cards display the mascot.
- **Privacy Separation**: Moved Home Assistant base URL to `SECRET_HA_URL` in `secrets.h` / `secrets.h.example` and stripped hardcoded LAN details from public source and web UI.

## [7.0.0] - 2026-11-25
### Added
- **Multi-Modifier DuckyScript Engine**: Full support for chained modifiers (`CTRL`, `ALT`, `SHIFT`, `GUI` via spaces or `+`), function keys `F1`–`F24`, navigation keys, media controls (`VOLUME_UP`, `VOLUME_DOWN`, `MUTE`), and `HA_SERVICE <domain>.<service> <entity_id>`.
- **MynouPAD Studio Web UI**: Redesigned dark industrial web console with interactive 3×3 physical key matrix, Normal/Combo mode switch, custom friendly labels (`/labels.json`), quick-insert command chips, remote key testing, and live status telemetry.
- **Bidirectional Home Assistant Integration**: Added REST API (`/api/status`, `/api/trigger`, `/api/action`, `/api/labels`) and non-blocking periodic/event-driven telemetry push to `sensor.mynoupad_status`.

## [6.0.0] - 2026-05-19
### Added
- OLED navigable macro menu via rotary encoder long-press, 5 non-blocking haptic vibration patterns, `HA_TOGGLE` command, and FFat web admin console.
