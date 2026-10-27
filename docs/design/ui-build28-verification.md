# UI refinement — Build 28

Reviewed 27/10/2026. The existing product is a desktop utility; the pass preserves
its content, purple identity, dark player and sound-control behavior.

## Changes

- Smaller header and sidebar; neutral surfaces instead of repeated lavender fills.
- Compact device/battery row with clipped long names. Track labels also use ellipsis.
- Bounded paired-device list; connection actions follow the list rather than every item.
- Shared tokens in `src/UiTheme.h` and durable rules in root `DESIGN.md`.
- Selected listening mode has an explicit check. Custom toggle gets a keyboard focus ring.
- Sound/device controls remain disabled while the required properties are unreported.
- EQ curve has real ±10/0 dB ticks; frequency labels and sliders are centered per column.
- Apply uses primary emphasis. Live preview, rollback, battery decoding and menubar
  preferences retain their existing behavior.
- macOS Reduce Motion disables control interpolation. No animation framework added.

## macOS discovery and permission handling

Final installed-app inspection exposed an IOBluetooth initialization wait inside
macOS 27.0.1 while Bluetooth permission was unavailable. The app now checks the
public CoreBluetooth authorization state before connecting and gives an actionable
message when access is unavailable. An undetermined permission request is started
asynchronously; the UI does not wait inside IOBluetooth for that decision.

Paired-device metadata is read through the built-in `/usr/sbin/system_profiler`
JSON output in a worker, with a five-second tool timeout and an eight-second process
watchdog. Only names and addresses are consumed; addresses are validated, normalized
and deduplicated. Battery values still come exclusively from the existing Sony
capability-filtered protocol decoder. This does not grant permission or change
system privacy preferences. No device listing is stored in the repository.

The main event loop keeps serving callbacks while pending work finishes on exit;
new automatic discovery/connection jobs are stopped once exit is requested.
The installed app displayed the paired list and permission guidance, then exited
normally with Cmd-Q. A fresh real-headphone connection was not verified because
Bluetooth access was unavailable; retry after enabling the existing app in System
Settings → Privacy & Security → Bluetooth.

## Reference selection

| Source supplied by the user | Use in this pass |
|---|---|
| [UI/UX Pro Max](https://github.com/nextlevelbuilder/ui-ux-pro-max-skill) | Hierarchy, spacing, contrast, visible keyboard focus. Local search for focus states applied; suggested landing-page compositions were unsuitable for this utility and were not persisted as a product match. |
| [Impeccable](https://github.com/pbakaus/impeccable) | Operate-mode restraint, preserve incumbent identity, inspect rendered layouts and avoid misleading states. Context launcher unavailable; project context read directly. |
| [Google DESIGN.md](https://github.com/google-labs-code/design.md) | Named tokens plus rationale in a durable Markdown design specification. |
| [Awesome DESIGN.md](https://github.com/VoltAgent/awesome-design-md) | Consistency through a project-owned design description; no brand template copied. |
| [Taste Skill](https://github.com/Leonxlnx/taste-skill) | App-screen guidance: hierarchy, neutral chrome, honest missing values. Consulted as reference rather than running its web inspector. |
| [Skills for Designers and Engineers](https://github.com/emilkowalski/skills) | Restrained motion and attention to interaction details. |
| [Hallmark](https://github.com/nutlope/hallmark) | Cross-check for generic decoration and visual consistency. |
| [Open Design](https://github.com/nexu-io/open-design) | Reference workflow; no extra local design service needed. |
| [React Bits](https://github.com/DavidHDev/react-bits), [GSAP Skills](https://github.com/greensock/gsap-skills), [Scroll World](https://github.com/oso95/scroll-world), [Canvas UI](https://github.com/DavidHDev/canvas-ui) | Web-specific references. No React, GSAP, 3D, canvas runtime or landing-page effects added to the C++ utility. |

The reference skills were read locally for this task, not installed into the
user's global skill directory. No external skill source or downloaded binary is
bundled with the product.

## Validation

- Release app compiled; all six existing CTest suites passed, including battery,
  transport, menu preferences and EQ preview/recovery.
- Native preview renders inspected: overview and sound at 1120 × 860; EQ at
  700 × 540 with tabs and a scrolling editor; Settings at 700 × 540 with 14/18-pixel body text.
- Visible keyboard focus observed on the player/volume controls. Native automated
  mouse clicks did not reliably activate ImGui controls in this session, so they
  are not evidence of completed click-through verification. The underlying action
  handlers and existing state-machine regression suites were retained.
- No deliberate changes sent to real headphones during the UI preview.
- Light theme only; no claim of a tested dark theme or full VoiceOver coverage.

Normal text contrast computed from the actual float tokens using sRGB relative
luminance (before standard disabled alpha):

| Pair | Ratio |
|---|---|
| Ink / white surface | 15.27:1 |
| Muted / white surface | 6.45:1 |
| Muted / subtle surface | 5.69:1 |
| Accent / accent tint | 5.83:1 |
| White / primary accent | 7.32:1 |
| Player secondary text / player | 8.49:1 |
| Player primary text / player | 13.66:1 |

These checks cover the named normal-state text pairs, not an accessibility
certification of every control state or the native accessibility tree.

## Reproduce a safe layout preview

Use a separate preview bundle identifier so the installed app remains independent.
Point configuration at an existing temporary TOML file. Preview values are illustrative.

```sh
SONY_UI_PREVIEW=1 SONY_UI_PREVIEW_PAGE=2 \
SONY_UI_PREVIEW_WIDTH=700 SONY_UI_PREVIEW_HEIGHT=540 \
SONYHEADPHONESCLIENT_CONFIG_PATH=/path/to/temporary.toml \
/path/to/Preview.app/Contents/MacOS/SonyHeadphonesClient
```

Pages are 0=overview, 1=sound, 2=EQ, 3=device. `SONY_UI_PREVIEW_SETTINGS=1`
opens Settings. To test a larger font, use `typographyVersion=1` and
`imguiFontSize=18` in the temporary TOML. These options are ignored outside
preview mode; they do not change stored preferences or connect to headphones.
