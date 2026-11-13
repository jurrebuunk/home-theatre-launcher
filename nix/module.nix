{ self }:
{ config, lib, pkgs, ... }:

let
  inherit (lib) mkOption mkEnableOption mkIf mkDefault types literalExpression;
  cfg = config.services.home-theatre-launcher;

  color = types.strMatching "^#[0-9a-fA-F]{6}$";
  pos = types.ints.positive;
  nonneg = types.ints.unsigned;
  argv = types.listOf types.str;

  opt = type: default: description: mkOption { inherit type default description; };

  appModule = { config, ... }: {
    options = {
      id = mkOption {
        type = types.strMatching "^[a-z0-9][a-z0-9-]*$";
        description = "Stable identifier (also names the systemd unit `htl-app-<id>.service`).";
      };
      label = mkOption { type = types.str; description = "Tile label."; };
      description = opt (types.nullOr types.str) null "Supporting text (shown in error details).";
      icon = mkOption {
        type = types.nullOr (types.either types.path (types.strMatching "^(/.*|builtin:[a-z]+)$"));
        default = null;
        example = literalExpression "./icons/stream.svg";
        description = ''
          SVG (recoloured through `currentColor`) or PNG icon, or a bundled icon
          `builtin:stream` / `builtin:tv`. Null uses a generic icon.
        '';
      };
      command = mkOption {
        type = types.nonEmptyListOf types.str;
        example = literalExpression ''[ "''${pkgs.moonlight-qt}/bin/moonlight" ]'';
        description = "argv executed directly (no shell). argv[0] must be an absolute path.";
      };
      environment = opt (types.attrsOf types.str) { } "Extra environment variables.";
      workingDirectory = opt (types.nullOr types.str) null "Working directory.";
      systemUnits = opt (types.listOf types.str) [ ] ''
        System units started before the app and stopped (and verified stopped)
        after it, e.g. a container service. The launcher user is allowed to
        start/stop exactly these units through polkit.
      '';
      stopCommand = opt (types.nullOr argv) null
        "Command run (bounded by timeouts.stop) before the app's unit is stopped.";
      ready = {
        window = {
          appId = opt (types.nullOr types.str) null "Ready when a window with this Wayland app_id is mapped.";
          appIdRegex = opt (types.nullOr types.str) null "Ready when a window app_id matches this regex.";
          titleRegex = opt (types.nullOr types.str) null "Ready when a window title matches this regex.";
          any = opt types.bool false "Ready when any new window appears.";
        };
        dbusName = opt (types.nullOr types.str) null "Ready when this name appears on the session bus.";
      };
      exitWhen = {
        unitExit = opt types.bool true "Return home when the app process exits.";
        windowClosed = opt (types.nullOr types.bool) null
          "Return home when the ready window closes (default: true if a window rule is set).";
        dbusNameLost = opt (types.nullOr types.bool) null
          "Return home when ready.dbusName disappears (default: true if set).";
      };
      timeouts = {
        start = opt pos 30 "Seconds until the app must be ready.";
        stop = opt pos 10 "Seconds for a graceful stop before SIGKILL.";
      };
      hooks = {
        preStart = opt (types.nullOr argv) null "Command run before starting; failure aborts the launch.";
        postStop = opt (types.nullOr argv) null "Command run after a complete stop.";
        timeout = opt pos 15 "Seconds allowed per hook.";
      };
    };
  };

  fontModule = family: package: {
    options = {
      family = opt types.str family "Font family name.";
      package = opt (types.nullOr types.package) package "Package providing the font.";
    };
  };

  a = cfg.appearance;
  b = cfg.behavior;
  dropNulls = lib.filterAttrsRecursive (_: v: v != null);

  appJson = app: dropNulls {
    inherit (app) id label description command environment workingDirectory systemUnits stopCommand
      ready exitWhen timeouts hooks;
    icon = if app.icon == null then null
           else if builtins.isPath app.icon then "${app.icon}"
           else app.icon;
  };

  configJson = pkgs.writeText "home-theatre-launcher.json" (builtins.toJSON {
    version = 1;
    apps = map appJson cfg.apps;
    appearance = {
      inherit (a) colors uiScale safeMargin safeMarginVertical statusRowHeight tile iconSize statusIconSize
        gap columns alignment borderWidth focusWidth focusOffset buttonHeight rowHeight transitionMs glyphStyle;
      fonts = {
        serif = a.fonts.serif.family;
        mono = a.fonts.mono.family;
        inherit (a.fonts) sizes;
      };
    };
    behavior = {
      input = b.input;
      bluetooth = dropNulls b.bluetooth;
      internet = dropNulls b.internet;
      clock = dropNulls b.clock;
    };
  });

  # Validated again by the application itself at build time.
  checkedConfig = pkgs.runCommand "home-theatre-launcher-config.json" { } ''
    export QT_QPA_PLATFORM=offscreen HOME=$TMPDIR XDG_RUNTIME_DIR=$TMPDIR
    ${cfg.package}/bin/home-theatre-launcher --check-config --config ${configJson}
    cp ${configJson} $out
  '';

  allSystemUnits = lib.unique (lib.concatMap (app: app.systemUnits) cfg.apps);

  rendererEnv = lib.optionalString (b.compositor.renderer != "auto") "WLR_RENDERER=${b.compositor.renderer}";

  labwcDir = "home-theatre-launcher/labwc";

  sessionScript = pkgs.writeShellScript "home-theatre-session" ''
    export XDG_SESSION_TYPE=wayland
    export XDG_CURRENT_DESKTOP=labwc
    export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"
    ${lib.concatStringsSep "\n" (lib.mapAttrsToList (k: v: "export ${k}=${lib.escapeShellArg v}") b.compositor.environment)}
    ${pkgs.labwc}/bin/labwc -C /etc/${labwcDir}
    status=$?
    # Compositor gone: stop the launcher and any app before the next session.
    ${pkgs.systemd}/bin/systemctl --user stop home-theatre-launcher.service 'htl-app-*' || true
    exit $status
  '';

  autostart = pkgs.writeShellScript "home-theatre-autostart" ''
    ${pkgs.systemd}/bin/systemctl --user import-environment WAYLAND_DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_TYPE
    ${pkgs.dbus}/bin/dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_TYPE
    ${b.compositor.extraAutostart}
    ${pkgs.systemd}/bin/systemctl --user restart home-theatre-launcher.service
  '';

  rcXml = ''
    <?xml version="1.0"?>
    <labwc_config>
      <core><gap>0</gap></core>
      <focus><followMouse>no</followMouse></focus>
      <keyboard>
        <repeatRate>25</repeatRate>
        <repeatDelay>600</repeatDelay>
        <!-- A no-op binding stops labwc from loading its default keybinds. -->
        <keybind key="W-F24"><action name="None"/></keybind>
      </keyboard>
      <mouse>
        <context name="Root">
          <mousebind button="Right" action="Press"><action name="None"/></mousebind>
        </context>
      </mouse>
      <windowRules>
        <windowRule identifier="*" serverDecoration="no">
          <action name="Maximize"/>
        </windowRule>
      </windowRules>
    </labwc_config>
  '';
in
{
  options.services.home-theatre-launcher = {
    enable = mkEnableOption "the Home Theatre TV launcher";

    package = mkOption {
      type = types.package;
      default = self.packages.${pkgs.stdenv.hostPlatform.system}.home-theatre-launcher;
      defaultText = literalExpression "home-theatre-launcher.packages.\${system}.default";
      description = "The launcher package.";
    };

    user = opt types.str "theatre" "Dedicated non-root user running the session.";
    createUser = opt types.bool true "Create the user (no password, groups input/video/render/audio).";

    apps = mkOption {
      type = types.listOf (types.submodule appModule);
      default = [ ];
      example = literalExpression ''
        [
          (home-theatre-launcher.lib.apps.moonlight { inherit pkgs; })
          (home-theatre-launcher.lib.apps.waydroid { inherit pkgs; })
          { id = "kodi"; label = "Kodi"; command = [ "''${pkgs.kodi-wayland}/bin/kodi" ];
            ready.window.appId = "kodi"; }
        ]
      '';
      description = "Ordered apps shown as tiles. Any application can be added; nothing is built in.";
    };

    appearance = {
      colors = {
        background = opt color "#1b1818" "Page background.";
        surface = opt color "#252121" "Tiles and panels.";
        surfaceAlt = opt color "#302b2b" "Expanded rows.";
        text = opt color "#c4c1c1" "Primary text.";
        muted = opt color "#918c8c" "Supporting text.";
        faint = opt color "#777272" "Disabled / inactive.";
        border = opt color "#666262" "1px structural borders.";
        borderStrong = opt color "#7b7676" "Stronger borders.";
        accent = opt color "#8aa1a8" "Primary identity colour (focus fill, icons).";
        focus = opt color "#a5bbc2" "Focus outline.";
        success = opt color "#8fae9a" "Success.";
        warning = opt color "#c2a27d" "Warnings.";
        error = opt color "#c97070" "Errors.";
      };
      fonts = {
        serif = mkOption {
          type = types.submodule (fontModule "STIX Two Text" pkgs.stix-two);
          default = { };
          description = "Label font (regular + italic).";
        };
        mono = mkOption {
          type = types.submodule (fontModule "IBM Plex Mono" pkgs.ibm-plex);
          default = { };
          description = "Clock / value font.";
        };
        sizes = {
          display = opt pos 56 "Transition, error and dialog headings.";
          title = opt pos 44 "Tile labels.";
          body = opt pos 28 "Buttons and list rows.";
          supporting = opt pos 24 "Italic supporting text.";
          clock = opt pos 40 "Clock.";
          value = opt pos 20 "Technical values.";
          code = opt pos 96 "Pairing passkey.";
        };
      };
      uiScale = mkOption {
        type = types.addCheck types.number (x: x > 0 && x <= 4) // { description = "number in (0, 4]"; };
        default = 1.0;
        description = "Multiplier for every size (sizes are px at 1920×1080).";
      };
      safeMargin = opt nonneg 96 "Horizontal safe margin.";
      safeMarginVertical = opt nonneg 56 "Vertical safe margin.";
      statusRowHeight = opt pos 88 "Status row height.";
      tile = {
        width = opt pos 480 "Tile width.";
        height = opt pos 360 "Tile height.";
        padding = opt nonneg 40 "Tile padding.";
      };
      iconSize = opt pos 120 "Tile icon size.";
      statusIconSize = opt pos 32 "Status row icon size.";
      gap = opt nonneg 48 "Gap between tiles.";
      columns = opt pos 4 "Tiles per row.";
      alignment = opt (types.enum [ "left" "center" ]) "center" "Horizontal alignment of the tile grid.";
      borderWidth = opt nonneg 1 "Structural border width (radius is always 0).";
      focusWidth = opt nonneg 4 "Focus outline width.";
      focusOffset = opt nonneg 6 "Gap between element and focus outline.";
      buttonHeight = opt pos 80 "Button height.";
      rowHeight = opt pos 80 "Bluetooth list row height.";
      transitionMs = opt nonneg 150 "Fade duration.";
      glyphStyle = opt (types.enum [ "color" "mono" ]) "color" "Controller button prompts in colour or white.";
    };

    behavior = {
      input = {
        confirmButton = opt (types.enum [ "south" "east" ]) "south" "Controller confirm button (the other one is Back).";
        homeHoldMs = opt pos 1000 "How long Home must be held (works inside apps).";
        repeatDelayMs = opt pos 400 "Controller navigation repeat delay.";
        repeatIntervalMs = opt pos 120 "Controller navigation repeat interval.";
        stickDeadzone = opt (types.addCheck types.number (x: x > 0 && x < 1)) 0.5 "Analog stick deadzone (0–1).";
        gamepadHomeCodes = opt (types.listOf types.str) [ "BTN_MODE" "KEY_HOMEPAGE" ] "evdev codes acting as Home on controllers.";
        keyboardHomeCodes = opt (types.listOf types.str) [ "KEY_HOMEPAGE" "KEY_HOME" ] "evdev codes acting as Home on keyboards/remotes.";
        keys = {
          select = opt (types.listOf types.str) [ "Return" "Enter" "Space" "Select" ] "Qt key names for Select.";
          back = opt (types.listOf types.str) [ "Escape" "Back" "Backspace" ] "Qt key names for Back.";
        };
        defaultGlyphs = opt (types.enum [ "xbox" "playstation" "keyboard" "remote" ]) "xbox" "Prompts before any input.";
      };
      bluetooth = {
        enable = opt types.bool true "Bluetooth menu and BlueZ/PipeWire setup.";
        adapter = opt (types.nullOr types.str) null "Preferred adapter, e.g. hci0.";
        discoveryTimeout = opt pos 30 "Seconds a scan runs.";
        pairingTimeout = opt pos 60 "Seconds to answer a pairing prompt.";
        agentCapability = opt (types.enum [ "DisplayOnly" "DisplayYesNo" "KeyboardOnly" "NoInputNoOutput" "KeyboardDisplay" ]) "KeyboardDisplay" "BlueZ agent capability.";
      };
      internet = {
        interface = opt types.str "eth0" "Ethernet interface whose reachability is shown.";
        url = opt types.str "https://www.google.com/generate_204" "HTTPS probe URL.";
        expectStatus = opt pos 204 "Expected HTTP status.";
        dnsHost = opt (types.nullOr types.str) null "Host resolved first (default: host of url).";
        timeout = opt pos 5 "Seconds per DNS/HTTPS step.";
        interval = opt pos 30 "Seconds between checks while online.";
        offlineInterval = opt pos 5 "Seconds between checks while offline.";
      };
      clock = {
        format = opt types.str "HH:mm" "Qt date/time format.";
        timeZone = opt (types.nullOr types.str) null "IANA time zone (default: system).";
      };
      compositor = {
        renderer = opt (types.enum [ "auto" "gles2" "vulkan" "pixman" ]) "auto" "wlroots renderer.";
        environment = opt (types.attrsOf types.str) { } "Extra environment for the compositor session.";
        extraAutostart = opt types.lines "" "Shell lines run at session start (e.g. wlr-randr output setup).";
      };
    };

    session.enable = opt types.bool true ''
      Boot straight into the launcher: greetd autologin for `user` running a
      single persistent labwc. Disable to integrate with your own session.
    '';
  };

  config = mkIf cfg.enable {
    assertions =
      let ids = map (x: x.id) cfg.apps;
      in [
        {
          assertion = lib.length ids == lib.length (lib.unique ids);
          message = "services.home-theatre-launcher.apps: duplicate ids in ${toString ids}";
        }
      ] ++ map (app: {
        assertion = lib.hasPrefix "/" (lib.head app.command);
        message = "services.home-theatre-launcher.apps.${app.id}.command: argv[0] must be an absolute path";
      }) cfg.apps;

    environment.etc."home-theatre-launcher/config.json".source = checkedConfig;
    environment.etc."${labwcDir}/rc.xml".text = rcXml;
    environment.etc."${labwcDir}/menu.xml".text = ''<?xml version="1.0"?><openbox_menu></openbox_menu>'';
    environment.etc."${labwcDir}/autostart" = { source = autostart; mode = "0755"; };
    environment.etc."${labwcDir}/environment".text = ''
      XDG_CURRENT_DESKTOP=labwc
      QT_QPA_PLATFORM=wayland
      ${rendererEnv}
    '';

    users.users.${cfg.user} = mkIf cfg.createUser {
      isNormalUser = true;
      extraGroups = [ "input" "video" "render" "audio" ];
      hashedPassword = mkDefault "!";
    };

    fonts.packages = lib.filter (p: p != null) [ a.fonts.serif.package a.fonts.mono.package ];
    hardware.graphics.enable = mkDefault true;
    security.polkit.enable = true;
    services.dbus.enable = true;

    security.polkit.extraConfig = mkIf (allSystemUnits != [ ]) ''
      // home-theatre-launcher: start/stop only the system units its apps declare.
      polkit.addRule(function (action, subject) {
        var units = ${builtins.toJSON allSystemUnits};
        if (action.id == "org.freedesktop.systemd1.manage-units" &&
            subject.user == ${builtins.toJSON cfg.user} &&
            units.indexOf(action.lookup("unit")) >= 0 &&
            ["start", "stop"].indexOf(action.lookup("verb")) >= 0) {
          return polkit.Result.YES;
        }
      });
    '';

    # Keep BlueZ running (and the menu usable) even before an adapter is attached.
    boot.kernelModules = lib.optional b.bluetooth.enable "bluetooth";
    hardware.bluetooth = mkIf b.bluetooth.enable {
      enable = mkDefault true;
      powerOnBoot = mkDefault true;
      settings.General = {
        Privacy = mkDefault "device";
        JustWorksRepairing = mkDefault "always";
        FastConnectable = mkDefault true;
      };
    };
    security.rtkit.enable = mkDefault true;
    services.pipewire = {
      enable = mkDefault true;
      alsa.enable = mkDefault true;
      pulse.enable = mkDefault true;
      wireplumber.enable = mkDefault true;
    };

    systemd.user.services.home-theatre-launcher = {
      description = "Home Theatre launcher";
      unitConfig = {
        ConditionUser = cfg.user;
        StartLimitIntervalSec = 0;
      };
      path = [ pkgs.systemd ];
      environment = {
        QT_QPA_PLATFORM = "wayland";
        XDG_CURRENT_DESKTOP = "labwc";
      };
      serviceConfig = {
        ExecStart = "${cfg.package}/bin/home-theatre-launcher --config /etc/home-theatre-launcher/config.json";
        Restart = "always";
        RestartSec = 1;
        MemoryMax = "512M";
      };
    };

    services.greetd = mkIf cfg.session.enable {
      enable = true;
      restart = true;
      settings = {
        initial_session = { command = "${sessionScript}"; user = cfg.user; };
        default_session = { command = "${sessionScript}"; user = cfg.user; };
      };
    };
  };
}
