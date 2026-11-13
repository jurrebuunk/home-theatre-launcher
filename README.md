# Home Theatre Launcher

A minimal, controller-friendly TV launcher for NixOS. It is configured
entirely through Nix: one persistent **labwc** compositor, a fullscreen
**Qt 6 / QML** interface, and **systemd** supervising every app. You can
launch any application you configure; Moonlight and Waydroid are only
ready-made examples. The launcher also includes a full Bluetooth menu, a
Network screen (Ethernet details and Wi-Fi with an on-screen keyboard), an
idle screen with a large clock, an internet indicator and a clock.

![Home](docs/screenshots/home.png)

The design is in `docs/design/` (rendered from the original PDF) and
follows flatwork-ui: flat, dark and square. The icons come from established
packs rather than the design's line drawings:

- **Interface and app icons:** Google [Material Symbols](https://fonts.google.com/icons),
  *Sharp* style at weight 300, with square corners and thin strokes to match
  the design.
- **Controller button prompts:** the real Xbox / PS5 glyphs from Kenney
  *Input Prompts*.

## Screenshots

Rendered by the app itself (`--screenshot`, demo data) at 1920×1080.

| | |
|---|---|
| ![Home](docs/screenshots/home.png) | ![Starting](docs/screenshots/starting.png) |
| **Home**: app tiles with Bluetooth, internet and clock | **Starting…** shown above the app until its window is ready |
| ![Bluetooth](docs/screenshots/bluetooth.png) | ![Bluetooth, PlayStation prompts](docs/screenshots/bluetooth-playstation.png) |
| **Bluetooth** menu with Xbox button prompts | Prompts follow the last-used controller (PS5) |
| ![Pairing](docs/screenshots/pairing.png) | ![No adapter](docs/screenshots/bt-no-adapter.png) |
| **Pairing**: confirm the passkey; trust is opt-in | No Bluetooth adapter |
| ![Start failed](docs/screenshots/error-start.png) | ![Still closing](docs/screenshots/error-stop.png) |
| Readable start error with Retry / Dismiss | Cleanup incomplete: Stop again forces it and launches stay blocked |
| ![Wi-Fi](docs/screenshots/network-wifi.png) | ![Wi-Fi password](docs/screenshots/network-password.png) |
| **Network → Wi-Fi**: power, timed scan, networks with signal / lock / connected | Password entry with the on-screen keyboard (A type, X delete, Menu connect) |
| ![Ethernet](docs/screenshots/network-ethernet.png) | ![Idle](docs/screenshots/idle-image.png) |
| **Network → Ethernet**: speed, reachability, Automatic / Manual IPv4 | **Idle screen** over an image (`builtin:mountains`) |

## Use it from a NixOS flake

```nix
# flake.nix of the machine
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    home-theatre-launcher.url = "github:jurrebuunk/home-theatre-launcher";
    home-theatre-launcher.inputs.nixpkgs.follows = "nixpkgs";
  };
  outputs = { nixpkgs, home-theatre-launcher, ... }: {
    nixosConfigurations.tv = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      specialArgs = { inherit home-theatre-launcher; };
      modules = [ home-theatre-launcher.nixosModules.default ./configuration.nix ];
    };
  };
}
```

```nix
# configuration.nix
{ pkgs, home-theatre-launcher, ... }:
{
  services.home-theatre-launcher = {
    enable = true;
    user = "theatre";
    apps = [
      (home-theatre-launcher.lib.apps.moonlight { inherit pkgs; })
      (home-theatre-launcher.lib.apps.waydroid { inherit pkgs; })
      # any other application:
      {
        id = "kodi";
        label = "Kodi";
        icon = "material:live_tv";       # any Material Symbol, builtin:<name>, or a path
        command = [ "${pkgs.kodi-wayland}/bin/kodi" ];
        ready.window.appId = "kodi";
      }
    ];
    appearance = {
      uiScale = 1.25;                     # bigger for a far couch
      tile = { width = 480; height = 360; };
    };
    behavior.internet = {
      interface = "ens18";
      url = "https://example.net/generate_204";
      expectStatus = 204;
    };
    behavior.idle = {
      timeout = 600;                      # seconds without input
      background = {
        type = "slideshow";               # color | image | slideshow | video
        directory = "/srv/photos";        # re-read every time the idle screen starts
        interval = 30;
        fadeMs = 1500;
      };
    };
    behavior.network.networkmanager = true; # editable Wi-Fi / Ethernet (see below)
  };
  virtualisation.waydroid.enable = true;  # only needed for the waydroid preset
}
```

To add apps or change the layout or style, edit the Nix config and rebuild.
You never need to change the application source. The module turns the
options into a versioned, read-only JSON file
(`/etc/home-theatre-launcher/config.json`). The app checks that file again
at build time (`--check-config`), so an invalid setup fails the build
instead of failing on the TV.

### What the module sets up

- The `user` account (no password; groups `input`, `video`, `render`, `audio`).
- greetd autologin into a single persistent labwc, which restarts if it
  crashes. labwc runs with no keybindings, no menus and no window
  decorations. Set `session.enable = false` to use your own session.
- The `home-theatre-launcher` systemd user service (`Restart=always`).
- A polkit rule that lets the user start and stop **only** the system units
  that apps declare in `systemUnits`. No sudo.
- BlueZ, PipeWire with WirePlumber (Bluetooth audio), fonts, and graphics.

### Idle screen

After `behavior.idle.timeout` seconds without input on the launcher (never
while an app runs), a large clock and date appear over the background. Any
button, key or mouse movement wakes it; that first press is swallowed so it
never also launches something.

| `behavior.idle.background.type` | uses |
|---|---|
| `color` (default) | `color` (default: the page background) |
| `image` | `image`: a path, or `builtin:mountains` (the landscape from the design) |
| `slideshow` | `images` and/or `directory`, `interval`, `fadeMs` (cross-fade), `shuffle` |
| `video` | `video` (path or URL, looped), `muted` |

`fill` (`cover` / `contain`) and `dim` (0–1, darkens pictures behind the
clock) apply to pictures and video. Set `behavior.idle.enable = false` to turn
it off. The date line uses `behavior.clock.dateFormat` and `locale`.

### Network screen

Select the network icon in the status row. Without NetworkManager the screen
is read-only: every Ethernet and Wi-Fi interface with its address, gateway,
DNS and link speed, read from the kernel. With
`behavior.network.networkmanager = true` the module enables NetworkManager
and a polkit rule for the launcher user, and the screen can:

- turn Wi-Fi on/off, scan, connect (password via the on-screen keyboard or a
  physical keyboard), join hidden networks, disconnect and forget;
- switch Ethernet between Automatic (DHCP) and Manual IPv4 (address/prefix,
  gateway, DNS).

A wrong password is reported as such and never saved. Switching an existing
machine to NetworkManager can give it a different DHCP lease, so enable it
when you can reach the machine another way.

### App options

Each entry in `apps` accepts these fields:

| option | meaning |
|---|---|
| `id`, `label`, `description`, `icon` | identity; `icon` is `material:<name>` (any [Material Symbol](https://fonts.google.com/icons)), `builtin:stream` / `tv` / `android` / `gamepad` / `desktop` / `cast` / `music` / `movie` / `web`, or a path to an SVG/PNG |
| `command`, `environment`, `workingDirectory` | argv executed directly (no shell) in a transient unit `htl-app-<id>.service` |
| `ready.window.appId` / `appIdRegex` / `titleRegex` / `any` | ready when a matching Wayland window is mapped |
| `ready.dbusName` | ready when this session-bus name appears |
| `exitWhen.unitExit` / `windowClosed` / `dbusNameLost` | what counts as the app having ended |
| `systemUnits` | system units started before the app and stopped (and verified stopped) after it |
| `stopCommand` | command run before stopping the app unit (e.g. `waydroid session stop`) |
| `timeouts.start` / `timeouts.stop` | bounded start and stop; stopping escalates to SIGKILL for the whole cgroup |
| `hooks.preStart` / `hooks.postStop` / `hooks.timeout` | optional argv hooks |

`appearance` exposes every colour, font and size token from the design. All
sizes are in px at 1920×1080 and are multiplied by `uiScale`. `behavior`
covers input mappings, Bluetooth, the internet check, the clock and the
compositor. See `nix/module.nix` for every option and its default.

## How it works

- **Lifecycle:** `launcher → launching → running → closing → launcher`, plus
  error states. Only one app runs at a time, and repeated activation is
  ignored. "Ready" means the configured window or bus name has appeared, not
  just that the process started. Exiting, crashing, or holding **Home**
  returns to the launcher.
- **Stopping:** the launcher runs the stop command, stops the app unit
  (systemd kills the whole cgroup), stops the app's system units, and then
  checks that all of them are gone. If something is still running after the
  stop budget, it shows *"… is still closing"* with **Stop again** (SIGKILL)
  and blocks further launches until cleanup succeeds.
- **Launcher restarts:** if the launcher process restarts, it adopts a
  still-running app. When nothing is running, it stops any app system units
  that were left active, so no Android container stays behind the home
  screen.
- **On top of apps:** the launcher is a layer-shell overlay, so
  *Starting…* and *Closing…* stay above the app. It unmaps while an app runs.
- **Input:** the launcher reads controllers through evdev (read-only, never
  grabbed). Holding Home works inside apps, controllers can navigate the
  launcher, and the button prompts follow the last-used device (Xbox,
  PlayStation, keyboard or remote).
- **Bluetooth:** uses BlueZ over D-Bus with its own pairing agent. It
  supports adapter selection, power, scanning with a timeout, pair (confirm
  passkey, display code, or enter PIN), connect/disconnect, trust/untrust and
  forget. Trust is only granted through the explicit checkbox.
- **Internet:** the indicator checks Ethernet carrier, then the default
  route, then DNS, then an HTTPS request with an expected status. The checks
  are timer driven and never block the UI or app launches.
- **Control API:** `nl.jurrebuunk.HomeTheatre1` on the session bus provides
  `Launch(s)`, `Home()`, `OpenBluetooth()`, `OpenNetwork()`, `EnterIdle()`,
  `Wake()`, the network calls (`NetworkInterfaces`, `WifiNetworks`,
  `WifiScan`, `WifiConnect(ss)`, `WifiConnectStatus`, `SetIpv4(sssss)`) and
  the `State` / `Idle` properties. The VM tests use it, and it can also back
  a compositor keybinding.

## Development

```sh
nix develop                      # toolchain
cmake -S launcher -B launcher/build -G Ninja -DWLR_PROTOCOLS_DIR=$(nix eval --raw nixpkgs#wlr-protocols)/share/wlr-protocols
ninja -C launcher/build && (cd launcher/build && ctest)

nix run .#demo                   # windowed demo: fake apps, Bluetooth and network
./launcher/build/home-theatre-launcher --screenshot bluetooth:/tmp/bt.png [--glyphs playstation]
nix flake check                  # package + unit tests + both NixOS VM tests
```

Screenshot views: `home`, `starting`, `closing`, `error-start`, `error-stop`,
`bluetooth`, `pairing`, `bt-no-adapter`, `network-wifi`, `network-password`,
`network-ethernet`, `idle`, `idle-image`. The README images in
`docs/screenshots/` are made this way (fonts via `FONTCONFIG_FILE` with STIX Two
and IBM Plex Mono).

The `vm-network` check runs the Network screen against a real NetworkManager
and a simulated Wi-Fi access point (`mac80211_hwsim` + hostapd): scanning, a
rejected wrong password, connecting, manual/automatic IPv4, and the idle
timer.

## Verify on the real machine

These can only be confirmed with real hardware:

- Moonlight's Wayland app_id (`com.moonlight_stream.Moonlight`).
- How Waydroid's full UI behaves with the preset (session bus name
  `id.waydro.Session`). Check that `waydroid status` shows the container
  stopped after returning home.
- Controller Guide-button codes over Bluetooth. `BTN_MODE` and
  `KEY_HOMEPAGE` are both mapped by default; Xbox controllers may need
  `hardware.xpadneo`.

## Third-party assets

- Material Symbols (Google), Apache-2.0: `launcher/assets/icons`, `launcher/assets/apps`
  (`launcher/assets/LICENSE-material-symbols.txt`). To refresh or remap them, run
  `scripts/vendor-material-icons.sh [version]`.
- Controller glyphs: Kenney *Input Prompts*, CC0 (`launcher/assets/glyphs/LICENSE-kenney.txt`).
