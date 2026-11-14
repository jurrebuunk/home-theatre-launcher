# Ready-made app definitions for services.home-theatre-launcher.apps.
# They are plain attribute sets: use them as-is, override fields with `//`,
# or write your own entries for any other application.
{
  # Moonlight game streaming: the tile opens the library of the paired hosts'
  # apps; a stream runs `moonlight stream <host> <app>`.
  moonlight = { pkgs, package ? pkgs.moonlight-qt }: {
    id = "moonlight";
    label = "Moonlight";
    description = "Stream games from the PC";
    icon = "builtin:stream";
    type = "moonlight";
    command = [ "${package}/bin/moonlight" ];
    ready.window.appId = "com.moonlight_stream.Moonlight";
    timeouts = { start = 30; stop = 10; };
  };

  # Android through Waydroid's full UI. The session runs inside the managed
  # process; the container service is started on demand and stopped again
  # (session *and* container) before returning home.
  waydroid = { pkgs, package ? pkgs.waydroid, id ? "android-tv", label ? "Android TV" }: {
    inherit id label;
    description = "Apps and streaming services";
    icon = "builtin:tv";
    command = [ "${package}/bin/waydroid" "show-full-ui" ];
    systemUnits = [ "waydroid-container.service" ];
    stopCommand = [ "${package}/bin/waydroid" "session" "stop" ];
    ready = {
      dbusName = "id.waydro.Session";
      window.any = true;
    };
    # Android may recreate its surface while booting; track the session instead.
    exitWhen.windowClosed = false;
    timeouts = { start = 90; stop = 30; };
  };
}
