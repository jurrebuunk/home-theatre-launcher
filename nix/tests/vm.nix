{ self }:
# Boots the full session (greetd → labwc → launcher) and drives the
# lifecycle through the launcher's D-Bus control interface.
{
  name = "home-theatre-launcher";

  nodes.machine = { pkgs, ... }: {
    imports = [ self.nixosModules.default ];

    virtualisation.memorySize = 2048;
    environment.systemPackages = [ pkgs.pulseaudio ]; # pactl for the sound test
    virtualisation.qemu.options = [ "-vga none" "-device virtio-gpu-pci" ];

    systemd.services.htl-dummy = {
      description = "Stand-in for an app's system service";
      serviceConfig.ExecStart = "${pkgs.coreutils}/bin/sleep infinity";
    };

    services.home-theatre-launcher = {
      enable = true;
      behavior.compositor.renderer = "pixman";
      behavior.internet.interface = "eth1";
      apps = [
        {
          id = "term";
          label = "Window";
          icon = "builtin:stream";
          command = [ "${pkgs.foot}/bin/foot" "--app-id=htl-test" ];
          ready.window.appId = "htl-test";
          timeouts = { start = 30; stop = 5; };
        }
        {
          id = "crash";
          label = "Crash";
          command = [ "${pkgs.bash}/bin/bash" "-c" "sleep 1; exit 3" ];
          ready.window.appId = "never";
          timeouts = { start = 20; stop = 5; };
        }
        {
          id = "stubborn";
          label = "Stubborn";
          icon = "material:block";
          command = [ "${pkgs.bash}/bin/bash" "-c" "trap '' TERM; while true; do ${pkgs.coreutils}/bin/sleep 1; done" ];
          timeouts = { start = 10; stop = 2; };
        }
        {
          id = "withunit";
          label = "With unit";
          icon = "builtin:tv";
          command = [ "${pkgs.coreutils}/bin/sleep" "infinity" ];
          systemUnits = [ "htl-dummy.service" ];
          timeouts = { start = 10; stop = 5; };
        }
      ];
    };
  };

  testScript = ''
    import time

    def user(cmd):
        return machine.succeed(
            "runuser -u theatre -- env XDG_RUNTIME_DIR=/run/user/1000 "
            "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus " + cmd)

    def call(method, *args):
        user("busctl --user call nl.jurrebuunk.HomeTheatre /nl/jurrebuunk/HomeTheatre "
             f"nl.jurrebuunk.HomeTheatre1 {method} " + " ".join(args))

    def state():
        out = user("busctl --user get-property nl.jurrebuunk.HomeTheatre /nl/jurrebuunk/HomeTheatre "
                   "nl.jurrebuunk.HomeTheatre1 State")
        return out.split('"')[1]

    def wait_state(want, timeout=60):
        deadline = time.time() + timeout
        last = None
        while time.time() < deadline:
            try:
                last = state()
            except Exception:
                last = None
            if last == want:
                return
            time.sleep(0.5)
        raise Exception(f"state {last!r}, wanted {want!r}")

    def unit_active(unit, system=False):
        flag = "" if system else "--user "
        cmd = f"systemctl {flag}is-active {unit}"
        if system:
            return machine.execute(cmd)[0] == 0
        return machine.execute(
            "runuser -u theatre -- env XDG_RUNTIME_DIR=/run/user/1000 "
            "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus " + cmd)[0] == 0

    machine.wait_for_unit("multi-user.target")
    machine.wait_until_succeeds("pgrep -u theatre labwc", timeout=120)
    machine.wait_until_succeeds("pgrep -u theatre -f home-theatre-launcher", timeout=120)
    wait_state("launcher")
    machine.sleep(2)
    machine.screenshot("home")

    with subtest("window readiness, Home returns and cleans up"):
        call("Launch", "s", "term")
        wait_state("running")
        assert unit_active("htl-app-term.service")
        machine.screenshot("running")
        call("Home")
        wait_state("launcher")
        assert not unit_active("htl-app-term.service")

    with subtest("double activation launches once"):
        call("Launch", "s", "term")
        call("Launch", "s", "stubborn")
        wait_state("running")
        assert not unit_active("htl-app-stubborn.service")
        call("Home")
        wait_state("launcher")

    with subtest("exit during start shows an error"):
        call("Launch", "s", "crash")
        wait_state("error")
        machine.screenshot("error")
        call("Home")  # dismisses
        wait_state("launcher")

    with subtest("SIGTERM-ignoring app is killed after its stop timeout"):
        call("Launch", "s", "stubborn")
        wait_state("running")
        call("Home")
        wait_state("launcher", timeout=30)
        assert not unit_active("htl-app-stubborn.service")
        machine.fail("pgrep -u theatre -f 'trap'")

    with subtest("system units start and stop with the app (polkit)"):
        assert not unit_active("htl-dummy.service", system=True)
        call("Launch", "s", "withunit")
        wait_state("running")
        assert unit_active("htl-dummy.service", system=True)
        call("Home")
        wait_state("launcher")
        assert not unit_active("htl-dummy.service", system=True)

    with subtest("launcher crash: restarted and adopts the running app"):
        call("Launch", "s", "term")
        wait_state("running")
        user("systemctl --user kill --signal=SIGKILL home-theatre-launcher.service")
        machine.sleep(3)
        wait_state("running")
        assert unit_active("htl-app-term.service")
        call("Home")
        wait_state("launcher")
        assert not unit_active("htl-app-term.service")

    def overlay_open():
        out = user("busctl --user get-property nl.jurrebuunk.HomeTheatre /nl/jurrebuunk/HomeTheatre "
                   "nl.jurrebuunk.HomeTheatre1 OverlayOpen")
        return out.strip() == "b true"

    with subtest("in-app menu only over a running app, which keeps running"):
        call("ToggleOverlay")
        assert not overlay_open(), "menu must not open on the launcher"
        call("Launch", "s", "term")
        wait_state("running")
        call("ToggleOverlay")
        assert overlay_open()
        machine.sleep(1)
        machine.screenshot("overlay")
        assert state() == "running"
        assert unit_active("htl-app-term.service")
        call("ToggleOverlay")
        assert not overlay_open()
        assert unit_active("htl-app-term.service")
        # leaving the app closes the menu
        call("ToggleOverlay")
        call("Home")
        wait_state("launcher")
        assert not overlay_open()

    with subtest("sound: outputs, switching and volume via PipeWire"):
        user("pactl load-module module-null-sink sink_name=htl_a sink_properties=device.description=Speakers")
        user("pactl load-module module-null-sink sink_name=htl_b sink_properties=device.description=Headset")
        user("pactl set-default-sink htl_a")
        def outputs():
            return user("busctl --user call nl.jurrebuunk.HomeTheatre /nl/jurrebuunk/HomeTheatre "
                        "nl.jurrebuunk.HomeTheatre1 SoundOutputs")
        deadline = time.time() + 30
        while "*htl_a Speakers" not in outputs():
            assert time.time() < deadline, outputs()
            time.sleep(0.5)
        call("SetSoundOutput", "s", "htl_b")
        machine.wait_until_succeeds(
            "runuser -u theatre -- env XDG_RUNTIME_DIR=/run/user/1000 pactl get-default-sink | grep -qx htl_b", timeout=15)
        def notes():
            return user("busctl --user call nl.jurrebuunk.HomeTheatre /nl/jurrebuunk/HomeTheatre "
                        "nl.jurrebuunk.HomeTheatre1 Notifications")
        deadline = time.time() + 10
        while "Headset" not in notes():
            assert time.time() < deadline, notes()
            time.sleep(0.5)
        call("SetVolume", "i", "42")
        machine.wait_until_succeeds(
            "runuser -u theatre -- env XDG_RUNTIME_DIR=/run/user/1000 pactl get-sink-volume htl_b | grep -q ' 42%'", timeout=15)
        deadline = time.time() + 10
        while user("busctl --user get-property nl.jurrebuunk.HomeTheatre /nl/jurrebuunk/HomeTheatre "
                   "nl.jurrebuunk.HomeTheatre1 Volume").strip() != "i 42":
            assert time.time() < deadline
            time.sleep(0.5)
        call("VolumeStep", "i", "1")
        machine.wait_until_succeeds(
            "runuser -u theatre -- env XDG_RUNTIME_DIR=/run/user/1000 pactl get-sink-volume htl_b | grep -q ' 47%'", timeout=15)

    with subtest("notifications show over a running app and over the in-app menu"):
        call("Launch", "s", "term")
        wait_state("running")
        call("ToggleOverlay")
        call("SetSoundOutput", "s", "htl_a")
        deadline = time.time() + 10
        while "Speakers" not in notes():
            assert time.time() < deadline, notes()
            time.sleep(0.5)
        machine.sleep(1)
        machine.screenshot("notification-over-menu")
        assert overlay_open()
        call("Home")
        wait_state("launcher")

    machine.screenshot("final")
  '';
}
