---
name: Sound Connect Desktop
version: "1"
colors:
  canvas: "#F6F6F9"
  surface: "#FFFFFF"
  subtle: "#F1F0F5"
  ink: "#262430"
  muted: "#615C6E"
  accent: "#5E479C"
  accent-hover: "#4D3885"
  accent-tint: "#E8E3F5"
  border: "#D9D6E0"
  success: "#29664F"
  player: "#292633"
  on-player: "#F7F5FC"
  player-muted: "#C7C2D4"
typography:
  title:
    fontFamily: SF Pro
    fontSize: 24px
  brand:
    fontFamily: SF Pro
    fontSize: 22px
  section:
    fontFamily: SF Pro
    fontSize: 18px
  body:
    fontFamily: SF Pro
    fontSize: 14px
rounded:
  panel: 12px
  control: 8px
spacing:
  small: 8px
  related: 12px
  panel: 20px
  section: 24px
---

## Product and surface

Mode: Operate. This is a macOS headphone control utility built with C++/ImGui,
not a marketing site. Users need to find their headset, read confirmed state and
adjust sound without ambiguity. Vietnamese labels and original EQ terminology
remain. Preserve the established purple identity and dark player.

## Hierarchy and layout

A compact brand header contains Settings/Done. At widths of 820 logical pixels
and above, use a 188-pixel navigation column; narrower windows use four tabs.
Content scrolls independently. Minimum supported window is 700 × 540. Paired
devices use a bounded scrolling list so connection actions remain below the list,
not at the end of every discovered device.

Show the connected device and battery first. A single battery stays one value;
dual batteries keep L/R labels. Never infer topology from a percentage. The
backend's Sony capability filtering is authoritative. Missing data stays `--`.
Use ellipsis and a full-text tooltip for long device/track labels.

Use flat white panels, thin neutral borders and one spacing rhythm. Purple
indicates interaction or selection; connection status has a separate green role.
Selection also has a check or visible position indicator. Do not add promotional
captions, decorative metrics, background effects or arbitrary entrance animation.

## Controls

Native SF Pro is loaded from macOS; the body size remains user configurable.
All values are logical units, rendered at Retina scale by the existing backend.
Custom icons share a consistent authored line style. Control labels remain visible.
Preserve keyboard navigation and focus outlines, including toggles.

Unknown/unsupported sound, Speak-to-Chat, volume and device settings are disabled
until their reported state is available. Preserve the existing bounds, ACK and
readback rules. UI edits must not add Bluetooth probes or writes.

EQ displays the actual five bands and Clear Bass. Keep Preset, Flat, Apply,
Revert and the brief explanation of live preview. Apply has the primary fill;
secondary actions are neutral. Chart ticks represent -10, 0 and +10 dB. Do not
replace confirmed readback with an illustrative curve outside preview mode.

## Motion and accessibility

Hover/selection transitions use the existing short exponential interpolation.
Respect macOS Reduce Motion by snapping to the target state; query the preference
at most once per second. Do not animate numerical state or introduce scroll effects.

Contrast checks for the normal semantic text pairs are documented in
[UI verification](docs/design/ui-build28-verification.md). Disabled controls use
the standard disabled alpha. This document does not claim full VoiceOver support:
ImGui's current native accessibility tree does not expose each drawn widget.

## Implementation and references

`src/UiTheme.h` owns the shared palette and global metrics; `src/App.cpp` owns
layout and UI widgets. Retain the C++/ImGui/Metal stack. Reference material and
its applicability are recorded in [the design notes](docs/design/ui-build28-verification.md).
Do not install web animation packages into this native app for visual similarity.
