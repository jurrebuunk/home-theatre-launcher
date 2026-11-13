{ self }:
# Network screen against a real NetworkManager and a simulated Wi-Fi access
# point (mac80211_hwsim + hostapd), plus the idle screen timer.
{
  name = "home-theatre-launcher-network";

  nodes.machine = { pkgs, lib, ... }: {
    imports = [ self.nixosModules.default ];

    virtualisation.memorySize = 2048;
    virtualisation.qemu.options = [ "-vga none" "-device virtio-gpu-pci" ];
    virtualisation.vlans = [ ]; # only the user-mode NIC (eth0, DHCP from QEMU)

    # Two simulated radios: wlan0 for NetworkManager, wlan1 as the access point.
    boot.kernelModules = [ "mac80211_hwsim" ];
    boot.extraModprobeConfig = "options mac80211_hwsim radios=2";
    networking.networkmanager.unmanaged = [ "wlan1" ];
    # qemu-vm.nix switches wpa_supplicant off for VMs; NetworkManager needs it.
    networking.wireless.enable = lib.mkOverride 9 true;
    services.hostapd = {
      enable = true;
      radios.wlan1 = {
        band = "2g";
        channel = 6;
        countryCode = "NL";
        networks.wlan1 = {
          ssid = "HtlTest";
          authentication = { mode = "wpa2-sha1"; wpaPassword = "correct horse"; };
        };
      };
    };
    systemd.services.ap-address = {
      wantedBy = [ "multi-user.target" ];
      after = [ "hostapd.service" ];
      requires = [ "hostapd.service" ];
      serviceConfig = { Type = "oneshot"; RemainAfterExit = true; };
      script = "${pkgs.iproute2}/bin/ip addr replace 10.42.0.1/24 dev wlan1";
    };
    networking.firewall.trustedInterfaces = [ "wlan1" ]; # DHCP for the client
    services.dnsmasq = {
      enable = true;
      resolveLocalQueries = false;
      settings = {
        interface = "wlan1";
        bind-dynamic = true;
        dhcp-range = "10.42.0.10,10.42.0.50,12h";
      };
    };

    services.home-theatre-launcher = {
      enable = true;
      behavior.compositor.renderer = "pixman";
      behavior.internet.interface = "eth0";
      behavior.network.networkmanager = true;
      behavior.idle = {
        timeout = 4;
        background = { type = "slideshow"; images = [ "builtin:mountains" ]; directory = "/var/lib/photos"; };
      };
      apps = [{
        id = "term";
        label = "Window";
        command = [ "${pkgs.foot}/bin/foot" "--app-id=htl-test" ];
        ready.window.appId = "htl-test";
      }];
    };
  };

  testScript = ''
    import time

    def user(cmd):
        return machine.succeed(
            "runuser -u theatre -- env XDG_RUNTIME_DIR=/run/user/1000 "
            "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus " + cmd)

    def call(method, sig="", *args):
        return user("busctl --user call nl.jurrebuunk.HomeTheatre /nl/jurrebuunk/HomeTheatre "
                    f"nl.jurrebuunk.HomeTheatre1 {method} {sig} " + " ".join(args))

    def prop(name):
        return user("busctl --user get-property nl.jurrebuunk.HomeTheatre /nl/jurrebuunk/HomeTheatre "
                    f"nl.jurrebuunk.HomeTheatre1 {name}").strip()

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
            time.sleep(1)
        raise Exception(f"timed out waiting for {what}: {last!r}")

    machine.wait_for_unit("multi-user.target")
    machine.wait_for_unit("hostapd.service")
    machine.wait_until_succeeds("pgrep -u theatre -f home-theatre-launcher", timeout=120)
    wait(lambda: '"launcher"' in prop("State"), "launcher")

    with subtest("interfaces come from NetworkManager"):
        out = wait(lambda: (lambda o: "wlan0 wifi" in o and o)(call("NetworkInterfaces")), "interfaces")
        assert "eth0 ethernet" in out, out
        assert "wlan1" not in out, out  # the unmanaged access-point radio is hidden

    with subtest("scan finds the access point"):
        call("OpenNetwork")
        def found():
            call("WifiScan")
            return "HtlTest" in call("WifiNetworks")
        wait(found, "HtlTest in scan results", timeout=90)
        machine.screenshot("network")

    with subtest("wrong password is rejected and not saved"):
        assert call("WifiConnect", "ss", "HtlTest", "'wrong password'").strip() == 's ""'
        out = wait(lambda: (lambda s: "error" in s and s)(call("WifiConnectStatus")), "failure", timeout=90)
        assert "Wrong password" in out, out
        machine.wait_until_succeeds("! nmcli -t -f NAME connection show | grep -x HtlTest", timeout=30)

    with subtest("connects with the right password"):
        call("WifiConnect", "ss", "HtlTest", "'correct horse'")
        wait(lambda: call("WifiConnectStatus").strip() == 's ""', "connected", timeout=90)
        machine.wait_until_succeeds("nmcli -t -f GENERAL.STATE device show wlan0 | grep -q '100 (connected)'", timeout=60)
        machine.succeed("ip -4 addr show wlan0 | grep -q 10.42.0.")

    with subtest("manual IPv4 on ethernet, then automatic again"):
        r = call("SetIpv4", "sssss", "eth0", "manual", "10.0.2.77/24", "10.0.2.2", "'10.0.2.3 1.1.1.1'")
        assert r.strip() == 's ""', r
        machine.wait_until_succeeds("ip -4 addr show eth0 | grep -q 10.0.2.77/24", timeout=60)
        machine.succeed("nmcli -g ipv4.dns connection show \"$(nmcli -g GENERAL.CONNECTION device show eth0)\" | grep -q 1.1.1.1")
        r = call("SetIpv4", "sssss", "eth0", "auto", '""', '""', '""')
        assert r.strip() == 's ""', r
        machine.wait_until_succeeds("ip -4 addr show eth0 | grep -q 10.0.2.15", timeout=60)
        machine.fail("ip -4 addr show eth0 | grep -q 10.0.2.77")

    with subtest("idle screen after the timeout, never over a running app"):
        call("Home")  # leaves the Network screen
        call("Wake")
        assert prop("Idle") == "b false"
        wait(lambda: prop("Idle") == "b true", "idle", timeout=20)
        machine.screenshot("idle")
        call("Launch", "s", "term")
        wait(lambda: '"running"' in prop("State"), "running")
        assert prop("Idle") == "b false"
        time.sleep(8)
        assert prop("Idle") == "b false"
        call("Home")
        wait(lambda: '"launcher"' in prop("State"), "launcher")
        wait(lambda: prop("Idle") == "b true", "idle again", timeout=20)
  '';
}
