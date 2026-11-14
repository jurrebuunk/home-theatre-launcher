{ self }:
# Moonlight library against fake GameStream hosts: a paired one (HTTPS with
# client certificates, apps, box art, running app, quit) and an unpaired one
# announced over mDNS. moonlight-qt is replaced by a stub that records its
# arguments and opens a window, so a "stream" runs like the real client.
{
  name = "home-theatre-launcher-moonlight";

  nodes.machine = { pkgs, lib, ... }:
    let
      # Host and client (moonlight) identities, as pairing would leave them.
      certs = pkgs.runCommand "fake-sunshine-certs" { nativeBuildInputs = [ pkgs.openssl ]; } ''
        mkdir $out
        for n in server client; do
          openssl req -x509 -newkey rsa:2048 -nodes -days 3650 -subj "/CN=$n" \
            -keyout $out/$n.key -out $out/$n.pem 2>/dev/null
        done
      '';
      # moonlight-qt's settings with the paired host (QSettings INI format).
      conf = pkgs.runCommand "Moonlight.conf" { nativeBuildInputs = [ pkgs.python3 ]; } ''
        python3 - <<EOF
        def ba(path):
            return '"@ByteArray(' + open(path).read().replace("\n", "\\\\n") + ')"'
        open("$out", "w").write("\n".join([
            "[General]",
            "certificate=" + ba("${certs}/client.pem"),
            "key=" + ba("${certs}/client.key"),
            "",
            "[hosts]",
            "1\\\\hostname=Gaming PC",
            "1\\\\uuid=F00D-CAFE",
            "1\\\\localaddress=127.0.0.1",
            "1\\\\localport=47989",
            "1\\\\srvcert=" + ba("${certs}/server.pem"),
            "size=1",
            ""]))
        EOF
      '';
      # Like moonlight-qt: a menu window first, then it is replaced by the
      # stream window (a different surface with the same app id).
      fakeMoonlight = pkgs.writeShellScript "moonlight" ''
        echo "$*" >> /tmp/moonlight-args
        ${pkgs.foot}/bin/foot --app-id=com.moonlight_stream.Moonlight ${pkgs.coreutils}/bin/sleep 3
        exec ${pkgs.foot}/bin/foot --app-id=com.moonlight_stream.Moonlight ${pkgs.coreutils}/bin/sleep infinity
      '';
      sunshine = pkgs.writeText "fake-sunshine.py" (builtins.readFile ./fake-sunshine.py);
    in
    {
      imports = [ self.nixosModules.default ];

      virtualisation.memorySize = 2048;
      virtualisation.qemu.options = [ "-vga none" "-device virtio-gpu-pci" ];
      environment.systemPackages = [ pkgs.curl pkgs.avahi ];
      environment.etc."htl-test/Moonlight.conf".source = conf;
      environment.etc."htl-test/client.pem".source = "${certs}/client.pem";
      environment.etc."htl-test/client.key".source = "${certs}/client.key";

      systemd.services.fake-gaming-pc = {
        wantedBy = [ "multi-user.target" ];
        serviceConfig.ExecStart = "${pkgs.python3}/bin/python3 ${sunshine} 'Gaming PC' F00D-CAFE 47989 47984 "
          + "${certs}/server.pem ${certs}/server.key ${certs}/client.pem";
      };
      systemd.services.fake-office-pc = {
        wantedBy = [ "multi-user.target" ];
        serviceConfig.ExecStart = "${pkgs.python3}/bin/python3 ${sunshine} 'Office PC' BEEF-0FF1CE 48989";
      };
      services.avahi.publish = { enable = true; userServices = true; };

      services.home-theatre-launcher = {
        enable = true;
        behavior.compositor.renderer = "pixman";
        behavior.internet.interface = "eth1";
        behavior.moonlight.refreshSeconds = 1;
        apps = [
          ((self.lib.apps.moonlight { inherit pkgs; }) // {
            command = [ "${fakeMoonlight}" ];
            streamArguments = [ "--fps" "60" ];
            timeouts = { start = 30; stop = 5; };
          })
        ];
      };
    };

  testScript = ''
    import json
    import time

    def user(cmd):
        return machine.succeed(
            "runuser -u theatre -- env XDG_RUNTIME_DIR=/run/user/1000 "
            "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus " + cmd)

    def call(method, *args):
        return user("busctl --user --json=short call nl.jurrebuunk.HomeTheatre /nl/jurrebuunk/HomeTheatre "
                    f"nl.jurrebuunk.HomeTheatre1 {method} " + " ".join(args))

    def value(method, *args):
        return json.loads(call(method, *args))["data"][0]

    def state():
        out = user("busctl --user get-property nl.jurrebuunk.HomeTheatre /nl/jurrebuunk/HomeTheatre "
                   "nl.jurrebuunk.HomeTheatre1 State")
        return out.split('"')[1]

    def wait(pred, what, timeout=60):
        deadline = time.time() + timeout
        last = None
        while time.time() < deadline:
            try:
                last = pred()
                if last:
                    return last
            except Exception as e:
                last = e
            time.sleep(0.5)
        raise Exception(f"timed out waiting for {what} (last: {last!r})")

    def host_state():
        return json.loads(machine.succeed("curl -s http://127.0.0.1:47989/test/state"))

    machine.wait_for_unit("multi-user.target")
    machine.wait_for_unit("fake-gaming-pc.service")
    machine.succeed("install -D -o theatre -m 600 /etc/htl-test/Moonlight.conf "
                    "'/home/theatre/.config/Moonlight Game Streaming Project/Moonlight.conf'")
    machine.wait_until_succeeds("pgrep -u theatre -f home-theatre-launcher", timeout=120)
    wait(lambda: state() == "launcher", "launcher")
    machine.succeed("curl -s http://127.0.0.1:47989/test/run/102")  # Steam Big Picture runs on the host

    with subtest("library: paired host online, apps and box art over HTTPS"):
        call("OpenMoonlight")
        wait(lambda: any(h.startswith("F00D-CAFE online") for h in value("MoonlightHosts")), "host online")
        apps = wait(lambda: value("MoonlightApps") if len(value("MoonlightApps")) == 3 else None, "apps")
        assert "103 Ember Coast" in apps, apps
        wait(lambda: "102 Steam Big Picture *" in value("MoonlightApps"), "running app marked")
        machine.wait_until_succeeds(
            "test -s /home/theatre/.cache/home-theatre-launcher/boxart/F00D-CAFE/103.png", timeout=30)
        machine.sleep(1)
        machine.screenshot("moonlight-library")

    with subtest("the library leaves no connection open that blocks the host"):
        # Polling every second; the host serves one HTTPS connection at a time.
        for _ in range(8):
            machine.succeed("curl -sk --max-time 3 --cert /etc/htl-test/client.pem --key /etc/htl-test/client.key "
                            "https://127.0.0.1:47984/serverinfo?uniqueid=0123456789ABCDEF | grep -q PairStatus")
            time.sleep(0.5)

    with subtest("discovery: an unpaired host announced over mDNS is offered"):
        machine.succeed("systemd-run --unit=announce avahi-publish -s 'Office PC' _nvstream._tcp 48989")
        wait(lambda: any(f.endswith("Office PC") for f in value("MoonlightFound")), "found host", timeout=60)

    with subtest("stream: moonlight stream <host> <app>, session in the menu"):
        assert value("MoonlightStream", "ss", "'Gaming PC'", "'Ember Coast'") == ""
        wait(lambda: state() == "running", "running")
        machine.sleep(5)  # the menu window has been replaced by the stream window
        assert state() == "running", state()
        args = machine.succeed("cat /tmp/moonlight-args").strip()
        assert args == "stream F00D-CAFE Ember Coast --fps 60", args
        assert value("Session") == "Ember Coast@Gaming PC"
        call("ToggleOverlay")
        machine.sleep(1)
        machine.screenshot("moonlight-overlay")

    with subtest("stop playing: the stream ends and the game quits on the host"):
        call("MoonlightStopPlaying")
        wait(lambda: state() == "launcher", "launcher")
        wait(lambda: host_state()["cancels"] == 1, "cancel on host")
        assert host_state()["current"] == 0
        wait(lambda: any("Stopped Ember Coast" in n for n in value("Notifications")), "notification")
        assert value("Session") == ""

    with subtest("disconnect (Home): the game keeps running on the host"):
        machine.succeed("curl -s http://127.0.0.1:47989/test/run/103")
        assert value("MoonlightStream", "ss", "F00D-CAFE", "'Ember Coast'") == ""
        wait(lambda: state() == "running", "running")
        call("Home")
        wait(lambda: state() == "launcher", "launcher")
        machine.sleep(2)
        assert host_state()["cancels"] == 1, host_state()
        assert host_state()["current"] == 103
  '';
}
