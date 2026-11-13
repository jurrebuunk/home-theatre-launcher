# Home Theatre — launcher requirements

## Architecture

A minimal TV launcher for Moonlight and Waydroid, configured entirely through
Nix. Use one persistent lightweight Wayland compositor (labwc recommended),
a fullscreen Qt/QML interface and a small supervisor. Do not restart the
compositor between apps. Run as a dedicated non-root user; use systemd for
process management, BlueZ D-Bus for Bluetooth and PipeWire for audio.

## Look and layout

Visual authority: [jurrebuunk/flatwork-ui DESIGN.md](https://github.com/jurrebuunk/flatwork-ui/blob/c217473882c9d386770a50d5cc73dcb49950dd92/DESIGN.md),
checked online at commit `c217473882c9d386770a50d5cc73dcb49950dd92`.

- Flat, dark, square: no rounded corners, shadows, gradients, glass or pills.
  Thin 1px structural borders; obvious square keyboard/controller focus.
- Defaults: background `#1b1818`, surface `#252121`, text `#c4c1c1`, muted
  `#918c8c`, border `#666262`, accent `#8aa1a8`, focus `#a5bbc2`.
- STIX Two Text for labels, italic for supporting text; IBM Plex Mono for
  clock/status values. Adapt sizes for couch viewing, not desktop density.
- Home: one simple row/grid of app buttons, initially Moonlight and Android
  TV. One quiet status row containing **Bluetooth, internet status, clock**.
  No extra widgets or nested home navigation.

## Allowed UI

- Only launch apps and open Bluetooth. Clock and internet status are read-only.
  No terminal, file browser, general settings, Wi-Fi menu or app/config editor.
- Full Bluetooth menu: adapter selection/power, scan, paired/available lists,
  pair, trust/untrust, connect/disconnect, forget, PIN/passkey/confirmation.
  Handle cancellation, errors and absent hardware; stop scanning on close.
  Trust requires explicit approval. Support controllers, remotes, keyboards
  and audio devices supported by BlueZ; audio must work through PipeWire.
- Internet indicator checks real reachability **over Ethernet**, not just link
  state: checking / online / offline. Use configurable bounded DNS/HTTPS
  checks and expected response; never block the UI or launches when offline.
- Clock uses system time with configurable format/timezone.
- Everything works without a mouse: clear focus, predictable navigation,
  configurable keyboard/controller/remote mappings and Back to dismiss menus.
  Do not assume the DisplayPort adapter supports HDMI-CEC.

## Lifecycle and loading

- States: `launcher → launching → running → closing → launcher`, plus error
  recovery. Exactly one managed main app at a time; the hidden launcher and
  compositor remain alive. Serialize requests; ignore repeated activation.
- Show a flat transition view with the app name, “Starting…” / “Closing…”
  and an indeterminate indicator. No fake percentages or artificial delays.
  Keep it above the app during teardown; restore the selected tile afterward.
- Readiness must mean a window/session is ready, not merely process creation.
- Exit, crash or a global Home action cleans up and returns to the launcher.
  Home works independently of app shortcuts; Back remains Android navigation.
- Bounded startup/stop timeouts, whole process-group/cgroup cleanup and forced
  termination when needed. Show readable errors with Dismiss/Retry, never a
  shell. Block further launches if cleanup remains incomplete. Automatically
  restart a crashed launcher/session.
- Waydroid needs an adapter: UI-command exit does not prove Android stopped.
  Track the actual session; start on demand and stop **session and container**
  before returning home. No Android workload behind Moonlight. Privileged
  control requires narrow authorization, not unrestricted sudo.

## Nix contract

Deliver a flake package and NixOS module. Proposed namespace:
`services.home-theatre-launcher` — these options do not exist yet.
Use typed options generating versioned, read-only runtime configuration.
No editable launcher database; BlueZ pairing/trust state is the exception.

Expose:

- **Apps:** ordered list; stable ID, label, icon/description, argv command,
  environment, working directory, lifecycle adapter, readiness rule,
  start/stop timeouts and optional managed start/stop hooks.
- **Appearance:** all color/font tokens, font sizes, borders/focus widths,
  UI scale, safe margins, padding, gaps, tile/icon sizes, columns, alignment,
  status-row height and transition timings. Keep flat/square defaults.
- **Behavior:** input/Home mappings, Bluetooth adapter/discovery timeout,
  Ethernet interface/reachability endpoint/expected response/check timing,
  clock format/timezone, session user and compositor integration.

Use Nix store paths for assets/executables and retain their dependencies.
Execute argv directly, not interpolated shell strings. Validate duplicate IDs,
empty commands and invalid dimensions. Document defaults and provide a
Moonlight + Waydroid example. Adding apps and changing layout/style must need
only Nix edits and a rebuild, not application source changes.

## Acceptance and boundaries

- Boots into the restricted launcher; Bluetooth is fully usable without mouse.
- Exit/crash/double-activation/timeout tests preserve exclusivity and recovery,
  including no leftover Waydroid workload.
- Nix changes to apps, fonts, colors, sizing and spacing visibly take effect.
- Test Ethernet loss, internet loss despite active link, Bluetooth pairing,
  reconnect/cancel and missing adapter.
- Target the **4 GB VM**: no Electron, full desktop or busy polling loops.

GPU passthrough, exposing a Bluetooth adapter to the VM, Android TV image
selection and DRM compatibility are separate prerequisites. Do not reboot
Proxmox or modify Home Assistant during launcher development.
