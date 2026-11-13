# Home Theatre Launcher — implementation plan

> **Status:** implemented. Change from this plan: there are no app-specific
> adapters. Every app uses the same generic options (`systemUnits`,
> `stopCommand`, `ready.*`, `exitWhen.*`, hooks), and Moonlight / Waydroid
> are presets in `nix/apps.nix`. Icons are Material Symbols Sharp (not hand-drawn,
> see §6.6). The README is the up-to-date reference.

Source of truth: `REQUIREMENTS.md` (behaviour, copied from the home-theatre repo), `docs/design/*.png` (look; rendered
from `~/Downloads/Home Theatre Launcher.pdf`), flatwork-ui `DESIGN.md` @ `c217473`.
**One deliberate deviation from the design:** controller button prompts use the
real Xbox / PS5 glyphs from Kenney *Input Prompts* (CC0, vendored in
`launcher/assets/glyphs/`) instead of the flat line glyphs on design page 10.
Kenney has no PlayStation “PS” button, so PS Home uses the in-style home-in-circle icon.

This plan is written so the whole thing can be implemented in one pass, in the
order of §10. Everything marked **verify** must be confirmed on the real VM.

---

## 1. Decisions

| Topic | Choice | Why |
|---|---|---|
| UI + controller | One C++20 / Qt 6.11 / QML binary `home-theatre-launcher` | Lean on 1 vCPU / 4 GB, native QtDBus for BlueZ + systemd, no Python runtime |
| Process supervision | **systemd user manager** via D-Bus `StartTransientUnit` (`htl-app-<id>.service`) | Whole-cgroup kill, stop timeouts, SIGKILL escalation, exit status — for free; argv executed directly |
| “Small supervisor” | systemd (system unit for the session, user unit for the launcher, transient units per app) + the launcher’s `LifecycleController` | No custom daemon to crash; launcher crash is recovered by adoption (§4.5) |
| Compositor | **labwc** 0.9.7, one instance for the whole session | Requirement; supports layer-shell + wlr-foreign-toplevel |
| Session start | System unit `home-theatre-session.service` (pattern of nixpkgs `services.cage`): `User=theatre`, `PAMName=home-theatre`, `TTYPath=/dev/tty1`, `Restart=always` | Real logind seat session, user systemd manager, deterministic restart |
| Launcher above apps | **LayerShellQt** overlay layer, exclusive keyboard while visible; unmapped while an app runs | Lets the transition view sit above the app during teardown |
| Readiness / window tracking | `zwlr_foreign_toplevel_manager_v1` via raw libwayland-client on its own `wl_display` connection, fd in a `QSocketNotifier` | Event driven (no polling); also lets us `activate` the app window |
| Controllers | **evdev** (libevdev), read-only, not grabbed; hot-plug via `QFileSystemWatcher` on `/dev/input` | Qt 6 has no gamepad module; Home must work while an app has focus |
| Keyboard / remote navigation in launcher | Normal Wayland key events into QML | Home-hold for these also detected via evdev (same code path) |
| Bluetooth | BlueZ D-Bus (`ObjectManager`, `Adapter1`, `Device1`, `AgentManager1`) + own `Agent1` adaptor | Requirement |
| Audio | PipeWire + WirePlumber (user services, bluez5 enabled) | Requirement; BT audio “just works” once connected |
| Internet | `/sys/class/net/<if>/carrier` + default-route check + bounded DNS + HTTPS probe (`QNetworkAccessManager`), timer-driven | Real reachability over Ethernet, never blocks UI |
| Config | Nix module → versioned JSON in the store → `--config <path>` | Read-only, no launcher database |
| Icons | Own stroke SVGs with `currentColor`, recoloured by a `QQuickImageProvider` (`image://icon/<name>?c=<hex>`) | Square/flat style, any token colour, sharp at any scale |

Not used: QtQuick.Controls styles (all controls are plain `Item`s so geometry is exact),
Electron, Python, busy loops. Timers only: clock (minute-aligned), internet check
(30 s, 5 s while offline), scan/pairing countdowns (1 s, only while visible),
indeterminate bar animation (only while visible).

---

## 2. Repository layout

```
flake.nix                 packages.home-theatre-launcher, nixosModules, devShells, checks, apps.dev/screenshots
configuration.nix         host config; imports + enables the module (with VM-specific values)
nix/
  package.nix             CMake build (done — scaffold builds)
  module.nix              services.home-theatre-launcher (options §5, system wiring §7)
  options/{apps,appearance,behavior}.nix   option declarations split for readability
  config-json.nix         options → versioned JSON + assertions
  labwc.nix               generated labwc rc.xml / menu.xml / autostart / environment
  example.nix             documented Moonlight + Waydroid example (from design p.9)
  tests/
    lifecycle.nix         runNixOSTest: launch/ready/exit/crash/double/timeout/stuck/home/adopt
    network.nix           runNixOSTest: online / link down / internet down with link up
    bluetooth.nix         runNixOSTest: no adapter; virtual adapter via hci_vhci (stretch: pair two vhci)
    test-app/             tiny Qt window app with modes: normal|crash-after|ignore-term|no-window|exit-after
launcher/
  CMakeLists.txt
  src/
    main.cpp                    args: --config, --windowed, --demo, --screenshot <screen> <png>
    Config.{h,cpp}              parse + validate JSON v1, expose Theme/Apps/Behavior to QML
    Theme.{h,cpp}               Q_GADGET/QObject of colour, font, size tokens; px() scaling
    AppModel.{h,cpp}            QAbstractListModel of apps (id,label,description,icon,state flags)
    lifecycle/
      LifecycleController.{h,cpp}  state machine §4
      UnitBackend.{h,cpp}          interface + SystemdUnitBackend (user + system bus)
      ReadinessWatcher.{h,cpp}     window / dbus-name / process rules
      AppAdapter.h                 interface
      GenericAdapter.cpp
      WaydroidAdapter.{h,cpp}
    wayland/
      ToplevelWatcher.{h,cpp}      wlr-foreign-toplevel client (wayland-scanner generated code)
    input/
      InputRouter.{h,cpp}          evdev devices, nav events, Home hold, last-used device kind
      EvdevDevice.{h,cpp}
    bluetooth/
      BluetoothManager.{h,cpp}     ObjectManager cache, adapters, scan, actions
      AdapterModel / DeviceModel   list models (paired / nearby filtered proxies)
      BluezAgent.{h,cpp}           org.bluez.Agent1 adaptor with delayed replies
      DemoBluetooth.cpp            fake data for --demo/screenshots
    status/
      NetworkStatus.{h,cpp}
      Clock.{h,cpp}
    ui/
      IconProvider.{h,cpp}
      WindowController.{h,cpp}     layer-shell setup, show/hide, keyboard interactivity
    ControlService.{h,cpp}      session D-Bus nl.jurrebuunk.HomeTheatre1 (tests/automation)
  qml/
    Main.qml                    root; switches views by Lifecycle.state & overlays
    views/HomeView.qml, TransitionView.qml, ErrorView.qml, BluetoothView.qml,
          PairingDialog.qml, ConfirmDialog.qml, NoAdapterView.qml, FatalConfigView.qml
    components/StatusRow.qml, AppTile.qml, FlatButton.qml, FocusFrame.qml, Icon.qml,
          Glyph.qml, HintBar.qml, Toggle.qml, CheckBox.qml, IndeterminateBar.qml,
          CountdownBar.qml, DeviceRow.qml, AdapterSelector.qml, SectionLabel.qml, Separator.qml,
          DigitEntry.qml
  assets/icons/*.svg            own icons (list §6.6)
  assets/apps/stream.svg, tv.svg   default app icons from design
  assets/glyphs/                Kenney CC0 (done)
  tests/                        QtTest: tst_config, tst_lifecycle (fake backend), tst_input_mapping
docs/design/                    reference renders of the PDF (done)
```

---

## 3. Process & session architecture

```
systemd (system)
 └─ home-theatre-session.service  User=theatre, PAM session on tty1, Restart=always
     └─ labwc -C /etc/home-theatre/labwc   (generated; no menus, no keybinds, no decorations)
         └─ autostart: dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP
                       systemctl --user start home-theatre.target
systemd --user (theatre)
 ├─ pipewire, wireplumber, pipewire-pulse
 ├─ home-theatre-launcher.service   PartOf/BindsTo home-theatre.target, Restart=always, RestartSec=1,
 │                                  StartLimitIntervalSec=0, MemoryMax=512M, env QT_QPA_PLATFORM=wayland
 └─ htl-app-<id>.service            transient, one at a time (created by the launcher)
     htl-hook-<id>-<pre|post>.service  transient oneshot hooks
systemd (system)
 └─ waydroid-container.service      NOT wanted at boot; started/stopped by the launcher via polkit
```

- If labwc dies the session unit exits → restarted by systemd → launcher restarted
  by the target. If only the launcher dies → user unit restart → adoption (§4.5).
- `theatre` is a normal user, no password, groups `input` (evdev), `video`, `render`.
  No sudo. Polkit rule (§7) allows exactly start/stop of `waydroid-container.service`.
- `ControlService` on the session bus: `Launch(s id)`, `Home()`, `OpenBluetooth()`,
  property `State`, signal `StateChanged(s)`. Used by NixOS tests; harmless otherwise.

---

## 4. Lifecycle

### 4.1 States

`Launcher → Launching → Running → Closing → Launcher`, plus `Error` (with `kind`):
`StartFailed` (Retry/Dismiss), `StopIncomplete` (Stop again/Dismiss), `HookFailed`.
Global flag `cleanupIncomplete`: while set, activating any tile re-shows the
`StopIncomplete` error instead of launching.

| From | Event | Action | To |
|---|---|---|---|
| Launcher | activate(app) | remember tile index; show TransitionView “Starting…”; run preStart hook; adapter.start() | Launching |
| Launching | ready | activate app window via foreign-toplevel; fade out (transitionMs); unmap launcher | Running |
| Launching | unit exited / failed | collect exit info; adapter.cleanup() | Error(StartFailed) |
| Launching | start timeout | stop (as Closing, invisible to user beyond the error) | Error(StartFailed, “timed out after 30 s”) |
| Launching | Home | cancel → Closing | Closing |
| Running | unit exited (any status) / window closed (if `exitOnWindowClose`) / Home | map launcher (overlay, exclusive kbd) with “Closing…”, adapter.stop() | Closing |
| Closing | adapter reports fully stopped | postStop hook; restore focus to remembered tile | Launcher |
| Closing | stop timeout + 5 s grace exceeded | set cleanupIncomplete | Error(StopIncomplete) |
| Error(StartFailed) | Retry | — | Launching |
| Error(StopIncomplete) | Stop again | KillUnit(all, SIGKILL) + StopUnit; adapter.forceStop() | Closing |
| Error(*) | Dismiss / Back | — | Launcher (cleanupIncomplete persists if not resolved) |

- All requests go through one queue; while not in `Launcher`, `activate` is ignored
  (double activation = no-op). Home during `Closing` is ignored.
- “Starting…/Closing…” views appear immediately; no artificial minimum duration,
  no percentages. The bar is indeterminate (sliding 1/3-width segment, §6).
- Unit exit info: `ExecMainStatus`, `ExecMainCode`, `Result`; Details shows those plus
  the last 15 journal lines (`journalctl --user -u htl-app-<id> -n 15 -o cat`, 2 s timeout).

### 4.2 Transient unit properties

`Type=exec`, `ExecStart=[(argv0, argv, false)]`, `Environment`, `WorkingDirectory`,
`KillMode=control-group`, `SendSIGKILL=yes`, `TimeoutStopUSec=stop`,
`CollectMode=inactive-or-failed`, `Restart=no`, `Description="Home Theatre: <label>"`.
Watch via `org.freedesktop.systemd1.Manager.Subscribe` + `PropertiesChanged` on the unit
object (`ActiveState`, `SubState`, `Result`). Stop with `StopUnit(name, "replace")`.

### 4.3 Readiness rules (`ready` option)

- `window = { appId = "…"; titleRegex = null; }` — ready when a matching toplevel
  is mapped (foreign-toplevel `done` after `app_id`); recommended.
- `dbusName = "…"` — ready when the name appears on the session bus.
- Both may be set (all must hold). Neither set → `process` mode: ready when unit is
  `active/running` (module emits a warning: “readiness is process-only”).
- `exitOnWindowClose` (default true when `window` set): matching toplevel closed
  while Running → Closing.

### 4.4 Adapters

`AppAdapter` interface: `start()`, `stop()`, `forceStop()`, `isFullyStopped()` (async),
`adoptIfRunning()`, signals `ready/exited/stopped/failed(reason)`.

- **generic**: unit only.
- **waydroid** (source-verified against waydroid 1.6.3):
  1. `StartUnit("waydroid-container.service")` on the **system** bus (polkit-allowed);
     wait `active` (bounded by start timeout).
  2. Transient unit runs `waydroid session start` (foreground, long-running; owns
     `id.waydro.Session` on the session bus; SIGTERM stops session **and** Android container).
  3. When `id.waydro.Session` appears, run the app’s `command` (default
     `waydroid show-full-ui`) as transient oneshot `htl-app-<id>-ui`.
  4. Ready = session name present **and** window `ready.window.appId` (default
     `"Waydroid"`, **verify** real app_id with `wlrctl toplevel list` on the VM).
  5. Stop: `waydroid session stop` (oneshot, bounded) → StopUnit session unit →
     `StopUnit("waydroid-container.service")` → verify: session name gone, container
     unit `inactive`, and no `lxc-start` for `waydroid` remains (via unit cgroup being empty).
     Only then `isFullyStopped = true`.
  6. On launcher start and before any non-Waydroid launch: if container unit is
     active without a Waydroid app running → stop it (no Android behind Moonlight).
  - Module sets `systemd.services.waydroid-container.wantedBy = mkForce []`.

### 4.5 Launcher (re)start / adoption

On startup, before showing Home: list units `htl-app-*`. If one is active and its id
is configured → adopt: state `Running`, launcher hidden, keep watching. If its id is
unknown or not running correctly → Closing flow. Then apply the Waydroid rule (§4.4.6).
Show Home only after this completes (TransitionView “Closing…” if work is needed).

### 4.6 Hooks

`hooks.preStart` / `hooks.postStop`: optional argv, run as transient oneshot units
with `hooks.timeout` (default 15 s). preStart failure → Error(StartFailed, “preStart hook failed”).
postStop failure → logged + shown in Details, does not block returning home.

---

## 5. Nix contract — `services.home-theatre-launcher`

Generated JSON (`/etc/home-theatre/launcher.json` → store path) has `"version": 1`;
the binary rejects unknown major versions. All sizes are px at 1920×1080 × `uiScale`
(runtime `px(n) = n * uiScale * screenHeight / 1080`). Validation as Nix `assertions`
**and** again in `Config.cpp` (invalid config → FatalConfigView using built-in defaults).

```nix
services.home-theatre-launcher = {
  enable; package;
  user = "theatre";                       # created by the module (isNormalUser, no password)
  apps = [ {                              # ordered
    id;                                   # ^[a-z0-9][a-z0-9-]*$, unique
    label; description ? null;
    icon ? null;                          # path (store) – svg/png; null → generic icon
    command;                              # nonEmpty listOf str; argv[0] absolute
    environment ? {};  workingDirectory ? null;
    adapter ? "generic";                  # enum [ generic waydroid ]
    ready = { window = { appId ? null; titleRegex ? null; }; dbusName ? null; };
    exitOnWindowClose ? null;             # null → true iff window rule set
    timeouts = { start ? 30; stop ? 10; };  # ints > 0 (s)
    hooks = { preStart ? null; postStop ? null; timeout ? 15; };
  } ];
  appearance = {
    colors = { background "#1b1818"; surface "#252121"; surfaceAlt "#302b2b";
               text "#c4c1c1"; muted "#918c8c"; faint "#777272";
               border "#666262"; borderStrong "#7b7676"; accent "#8aa1a8";
               focus "#a5bbc2"; success "#8fae9a"; warning "#c2a27d"; error "#c97070"; };
               # all ^#[0-9a-fA-F]{6}$
    fonts = {
      serif = { family = "STIX Two Text"; package = pkgs.stix-two; };
      mono  = { family = "IBM Plex Mono"; package = pkgs.ibm-plex; };
      sizes = { display = 56; title = 44; body = 28; supporting = 24;
                clock = 40; value = 20; code = 96; };      # code = passkey
    };
    uiScale = 1.0;  safeMargin = 96;  safeMarginVertical = 56;  statusRowHeight = 88;
    tile = { width = 480; height = 360; padding = 40; };
    iconSize = 120;  statusIconSize = 32;  gap = 48;  columns = 4;
    alignment = "center";                 # enum [ left center ]
    borderWidth = 1;  focusWidth = 4;  focusOffset = 6;   # radius is fixed 0, not an option
    buttonHeight = 80;  rowHeight = 80;
    transitionMs = 150;
    glyphStyle = "color";                 # enum [ color mono ] – Kenney colour vs white glyphs
  };
  behavior = {
    input = {
      confirmButton = "south";            # enum [ south east ]  (back = the other one)
      homeHoldMs = 1000;
      repeatDelayMs = 400; repeatIntervalMs = 120; stickDeadzone = 0.5;
      gamepadHomeCodes = [ "BTN_MODE" "KEY_HOMEPAGE" ];
      keyboardHomeCodes = [ "KEY_HOMEPAGE" "KEY_HOME" ];   # evdev names, hold detection
      keys = { select = [ "Return" "Enter" "Space" "Select" ];      # Qt key names (in-launcher)
               back   = [ "Escape" "Back" "Backspace" ]; };
      defaultGlyphs = "xbox";             # enum [ xbox playstation keyboard remote ]
    };
    bluetooth = { enable = true; adapter = null; discoveryTimeout = 30;
                  pairingTimeout = 60; agentCapability = "KeyboardDisplay"; };
    internet = { interface = "ens18"; url = "https://www.google.com/generate_204";
                 expectStatus = 204; dnsHost = null;   # null → host of url
                 timeout = 5; interval = 30; offlineInterval = 5; };
    clock = { format = "HH:mm"; timeZone = null; };   # null → system tz
    compositor = { renderer = "auto"; extraEnvironment = {}; };   # auto|gles2|pixman
    session = { tty = 1; };
  };
};
```

Assertions: unique ids; `command` non-empty with absolute argv0; all ints positive
(columns ≥ 1, timeouts ≥ 1, sizes ≥ 0, `uiScale` in (0, 4]); colour regex; at most
one `waydroid` app; waydroid app ⇒ `virtualisation.waydroid.enable`. Paths (icons,
commands, fonts) are store paths → dependencies retained automatically.

---

## 6. UI specification (all values in design px before scaling)

### 6.1 Global
- Window: fullscreen layer surface, `background` fill, no radius anywhere.
- Focus treatment (tiles, buttons, list rows’ buttons): element fill `accent`, its
  content (icon + text) in `background` colour; separate outline rectangle
  `focusWidth` (4) in `focus` colour drawn `focusOffset` (6) **outside** the element,
  so focus never changes layout. Unfocused: fill `surface` (tiles) or transparent
  (buttons), 1 px `border`, icon `accent`, text `text`.
- Disabled: text/icon `faint`, border `border`, not focusable.
- Fonts: serif regular for labels, serif italic for supporting text (descriptions,
  “Starting…”, section labels “Paired”/“Nearby”, “Details”), mono for clock, counts,
  countdowns, adapter name, passkey, technical details.
- Navigation: QML `KeyNavigation`/explicit focus chains; evdev controller events are
  injected as `Up/Down/Left/Right/Return/Escape` key events into the window (only
  while the launcher is mapped). Back on Home does nothing; Back elsewhere closes.

### 6.2 Home (p.1)
- StatusRow: top inset `safeMarginVertical`, height `statusRowHeight`, sides `safeMargin`;
  1 px `border` line under it. Left: Bluetooth icon (`statusIconSize`, `muted`) + mono
  `value` count of connected devices (hidden when 0; icon `faint` when powered off).
  The Bluetooth item is focusable (Up from tiles) and opens BluetoothView.
  Right: internet icon, 1 px vertical separator, clock (mono `clock` size, `text`).
  Internet: online → ethernet icon `muted`; checking → ethernet `faint`;
  no link → ethernet-off `error`; link but no internet → ethernet `warning`. Read-only.
- Tile grid below: `columns`, `gap`, tiles `tile.width × tile.height`, centered
  vertically in remaining space, horizontally per `alignment`. Tile: icon `iconSize`
  centred above label (`title` size), `tile.padding`. The description is not shown
  (the design shows only the name); it is kept in the config for Details/logs.
- Initial focus: first tile; restored to last-launched tile on return.

### 6.3 TransitionView (p.2–3)
Centered column: app icon (~140, `accent`), label (`display`, `text`), “Starting…” /
“Closing…” (`supporting` italic, `muted`), 32 px below an indeterminate bar 500 × 3:
track `surface`, segment 1/3 width `accent`, slides L→R in 1.2 s loop, only while visible.
Fade in/out `transitionMs`.

### 6.4 ErrorView (p.4–5)
Icon 140: StartFailed → square with “!” (`error`); StopIncomplete → triangle “!” (`warning`).
Title `display`: “<Label> didn’t start” / “<Label> is still closing”.
Buttons (height `buttonHeight`, padding 40, icon 28 + 16 gap + `body` text):
Retry (refresh icon) / Stop again (power icon), Dismiss (x icon). First button focused.
“› Details” (`supporting` italic `muted`, focusable) toggles a mono `value` panel
(surface, 1 px border, max 12 lines) with reason, exit code/signal, journal tail.

### 6.5 Bluetooth (p.6–8)
- Header: bluetooth icon (`accent`) + “Bluetooth” (`display`), 1 px line under.
- Left column 400 wide: AdapterSelector (“‹ hci0 ›” mono, Left/Right cycles when >1
  adapter, chevrons `faint` when only one), power row (power icon + square Toggle:
  32×22 frame, filled square knob, `accent` when on), scan row (search icon + mono
  countdown “0:23” right-aligned, 3 px progress line at its bottom edge shrinking
  `accent` while scanning; activating toggles scan). Each 400 × 88, `surface`, 1 px border,
  16 gap.
- Right column from x=576: SectionLabel “Paired” (supporting italic muted) + line;
  rows (`rowHeight`, 1 px `border` separators): device icon 36 (`muted`), name (`body`),
  right side link icon (`accent` if connected else `faint`) and shield-check (`accent`
  if trusted else `faint`). Focused/expanded row: `surfaceAlt` background and an action
  row of compact buttons (height 64): Connect|Disconnect, Trust|Untrust, Forget.
  “Nearby” section: unpaired discovered devices (with RSSI ordering); activating → pairing.
  Empty states (italic muted): “No paired devices”, “Search to find devices”.
- Footer: line, then HintBar: [glyph] Select · [glyph] Back · [glyph] Home, glyphs
  per last-used device (Kenney for Xbox/PS; mono key boxes “Enter/Esc/Home” for
  keyboard, “OK/Back/Home” for remote).
- Pairing dialog (p.7): full-screen overlay on `background`. Device icon (~100 `accent`)
  + small bluetooth icon `faint`; device name `display`; passkey box 640 × 184 `surface`
  with border, mono `code` size, formatted “482 913”; CheckBox (36 square) + shield +
  “Trust”, unchecked by default (trust only by explicit tick); buttons Pair / Cancel;
  countdown mono `value` `faint` (agent request timeout). Variants:
  RequestConfirmation (shows passkey, Pair confirms), DisplayPasskey/DisplayPinCode
  (shows code, “Type this on the device”, only Cancel), RequestPasskey/RequestPinCode
  (DigitEntry: 6 digit cells, Up/Down change digit, Left/Right move, number keys type),
  RequestAuthorization / AuthorizeService (“Allow <name> to connect?” Allow/Deny),
  Just-works (no agent call): “Pairing…” + IndeterminateBar + Cancel (`CancelPairing`).
- ConfirmDialog for Forget (“Forget <name>?” Forget/Cancel).
- NoAdapterView (p.8): bluetooth-off icon `error`, “No Bluetooth adapter”, Retry / Close.
  Also used for “Bluetooth service unavailable” (BlueZ not on the bus).
- Errors from BlueZ calls (e.g. `org.bluez.Error.AuthenticationFailed`) shown as a one-line
  italic `error` message under the affected row / in the dialog; mapping table in code.
- Closing the view (Back) stops discovery and cancels any outstanding agent request.

### 6.6 Own icons (`assets/icons/*.svg`, 24×24 viewBox, stroke 1.5, `currentColor`, square caps/joins)
bluetooth, bluetooth-off, ethernet, ethernet-off, monitor-play, tv, power, search,
chevron-left, chevron-right, link, shield-check, trash, check, x, refresh,
warning-triangle, error-square, gamepad, remote, headphones, keyboard, speaker,
mouse, phone, device (generic), home-circle, arrows (keyboard move hint).
BlueZ `Icon` → icon: `input-gaming`→gamepad, `input-keyboard`→keyboard,
`input-mouse`/`input-tablet`→mouse, `audio-headset`/`audio-headphones`→headphones,
`audio-card`/`audio-speakers`→speaker, `phone`→phone, else device; name contains
“remote” → remote.

### 6.7 Glyph selection (`InputRouter.lastDeviceKind`)
Vendor `0x045e` → xbox, `0x054c` → playstation, other gamepads → `defaultGlyphs`
(xbox); devices with letter keys → keyboard; devices with `KEY_HOMEPAGE`/`KEY_BACK`/
`KEY_OK` but no letter keys → remote. Xbox: A/B/Guide (`xbox_button_color_a`, `…_b`,
`xbox_guide`); PlayStation: ✕/○ (`playstation_button_color_cross`, `…_circle`) +
home-circle. With `confirmButton = "east"` swap the select/back glyphs.

---

## 7. NixOS system wiring (module `config` section)

- `users.users.<user>`: isNormalUser, `extraGroups = [ "input" "video" "render" ]`,
  `hashedPassword = "!"`; `users.groups` as needed.
- `security.pam.services.home-theatre.startSession = true;`
- `systemd.services.home-theatre-session` (modelled on nixpkgs `services/wayland/cage.nix`):
  `After = systemd-user-sessions.service plymouth-quit.service systemd-logind.service`,
  `Conflicts/After getty@tty1`, `wantedBy = graphical.target`, `Restart=always`,
  `User`, `PAMName`, `TTYPath=/dev/tty1`, `TTYReset/VHangup/VTDisallocate`,
  `StandardInput=tty-fail`, `UtmpIdentifier=tty1`, env `XDG_SESSION_TYPE=wayland`;
  `ExecStart = labwc -C <generated dir>`. `systemd.defaultUnit = graphical.target`.
  Disable `getty@tty1`.
- labwc config: `rc.xml` with no server-side decorations,
  window rule `identifier="*"` → serverDecoration no + Maximize, `<keyboard>` section
  **without** `<default/>` (no built-in keybinds), empty `menu.xml`, `autostart` per §3,
  `environment` with `XDG_CURRENT_DESKTOP=labwc`, `QT_QPA_PLATFORM=wayland`,
  `WLR_RENDERER` from `compositor.renderer` (omitted for auto), `XKB_DEFAULT_LAYOUT=us`.
- `systemd.user.services.home-theatre-launcher` (only for `user` via
  `ConditionUser`), `systemd.user.targets.home-theatre` (BindsTo graphical-session.target).
- `environment.etc."home-theatre/launcher.json".source = <generated json>`.
- `fonts.packages = [ serif.package mono.package ]`, `fonts.fontconfig.enable = true`.
- `hardware.bluetooth = { enable = true; powerOnBoot = true; settings.General =
  { Privacy = "device"; JustWorksRepairing = "always"; FastConnectable = true;
  Experimental = true; }; }` (mkDefault each).
- `services.pipewire = { enable; alsa.enable; pulse.enable; wireplumber.enable; }`,
  `security.rtkit.enable`; WirePlumber bluez5 roles default (a2dp_sink, hfp/hsp).
- If a waydroid app exists: `virtualisation.waydroid.enable = true`,
  `systemd.services.waydroid-container.wantedBy = mkForce []`, and polkit rule:
  ```js
  polkit.addRule(function (action, subject) {
    if (action.id == "org.freedesktop.systemd1.manage-units" &&
        subject.user == "theatre" &&
        action.lookup("unit") == "waydroid-container.service" &&
        ["start", "stop"].indexOf(action.lookup("verb")) >= 0)
      return polkit.Result.YES;
  });
  ```
- `hardware.graphics.enable = true`; `hardware.xpadneo.enable` optional (mkDefault false;
  note in docs for Xbox BT quirks).
- No terminal, file manager, or other desktop packages installed for the user.

`configuration.nix` for this VM: enable the module with the example apps,
`behavior.internet.interface = "ens18"` (actual NIC), keep SSH/guest agent as is.

---

## 8. Testing & acceptance

**Unit (QtTest, run in `checkPhase`)**
- `tst_config`: defaults, every validation error, version mismatch.
- `tst_lifecycle` with `FakeUnitBackend` + fake readiness: happy path; double activation
  ignored; start timeout → StartFailed + unit stopped; crash while Running → Launcher;
  Home during Launching; stop timeout → StopIncomplete blocks launches; Stop again
  resolves; adoption at startup; waydroid adapter ordering & full-stop verification.
- `tst_input_mapping`: device classification, Home hold threshold, axis → d-pad
  with deadzone/repeat.

**NixOS VM tests (`nix flake check`)** — labwc headless (`WLR_BACKENDS=headless`,
`WLR_RENDERER=pixman`, `QT_QUICK_BACKEND=software`), driven via `ControlService`:
- `lifecycle`: test-app modes normal / crash-after / ignore-term / no-window /
  exit-after; asserts state sequence, `systemctl --user list-units 'htl-app-*'` empty
  after each, launcher restart while Running → adopted; kill launcher → restarted.
- `network`: second node serving 204; `ip link set eth1 down` → offline(no link);
  block port 443 on server → offline despite carrier; restore → online; launches still
  work while offline.
- `bluetooth`: no adapter → NoAdapterView state via `ControlService`; with `hci_vhci`
  virtual controller → adapter listed, power toggle, scan start/stop on close.
- Screenshot check: `--demo --screenshot <view>` for home, starting, closing,
  error-start, error-stop, bluetooth, pairing, bt-no-adapter → compared by eye to
  `docs/design/` (`nix run .#screenshots` writes `./screenshots/`).

**Manual on the VM (after display prerequisite)** — REQUIREMENTS “Acceptance”: boot
into launcher; Moonlight start/exit/Home; Waydroid start/Home → `waydroid status` STOPPED,
container unit inactive; pair DualSense, Xbox, a keyboard and a BT speaker (audio via
`wpctl status`); reconnect, cancel pairing, unplug adapter; pull Ethernet; block WAN at
router; change a colour/tile size/app in Nix → `./deploy.sh` → visible change;
`free -m` with Moonlight running.

---

## 9. Prerequisites / risks (outside the launcher)

1. **The VM has no `/dev/dri` today** → labwc cannot start on real DRM. Either finish
   GPU passthrough (README maintenance window) or temporarily set the Proxmox display
   to `virtio-gpu` (needs a full VM 101 stop/start — your call, never Proxmox reboot).
   Until then development uses the laptop (`nix run .#dev`, windowed, `--demo`) and
   the NixOS VM tests.
2. Bluetooth adapter passthrough to VM 101 (USB passthrough) — separate.
3. Waydroid image (`waydroid init` with an Android TV image) — separate; module only
   enables Waydroid. **Verify** the full-UI window `app_id` and binder support in the
   6.18 kernel.
4. Xbox controllers over BT may need `xpadneo`; Guide may arrive as `KEY_HOMEPAGE`
   — both are in the default home codes.
5. Moonlight-qt app_id `com.moonlight_stream.Moonlight` — **verify**.
6. 1 vCPU: consider 2 vCPUs (README note) for Waydroid; launcher itself idles at ~0 %.

---

## 10. Implementation order (one pass)

1. Module options + JSON generation + assertions + `example.nix`; `configuration.nix` wiring. `nix eval` passes.
2. `Config`/`Theme` + IconProvider + icons + base components (FocusFrame, FlatButton, Icon, Glyph, Separator).
3. HomeView + StatusRow + Clock + NetworkStatus; `--demo`/`--windowed`/`--screenshot`. Compare to p.1.
4. UnitBackend (systemd D-Bus), ToplevelWatcher, ReadinessWatcher, LifecycleController,
   GenericAdapter, TransitionView, ErrorView, WindowController (layer shell), ControlService. Unit tests.
5. InputRouter (evdev nav, Home hold, device kind) + HintBar glyphs.
6. WaydroidAdapter + polkit + adoption logic.
7. BluetoothManager + Agent + all Bluetooth views/dialogs (+ demo data). Compare to p.6–8.
8. labwc config, session/user units, PipeWire, Bluetooth system settings.
9. NixOS tests + test-app; `nix flake check` green; screenshots reviewed.
10. README section: options reference, defaults table, deploy/test instructions.
