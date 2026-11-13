{
  description = "Home Theatre Launcher — minimal Nix-configured TV launcher (labwc + Qt/QML)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

  outputs = { self, nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      package = pkgs.callPackage ./nix/package.nix { };
    in
    {
      packages.${system} = {
        home-theatre-launcher = package;
        default = package;
      };

      nixosModules = rec {
        home-theatre-launcher = import ./nix/module.nix { inherit self; };
        default = home-theatre-launcher;
      };

      # Ready-made app entries, e.g. (home-theatre-launcher.lib.apps.moonlight { inherit pkgs; })
      lib.apps = import ./nix/apps.nix;

      # Windowed demo with fake apps/Bluetooth: `nix run .#demo`
      apps.${system}.demo = {
        type = "app";
        program = toString (pkgs.writeShellScript "home-theatre-launcher-demo" ''
          export FONTCONFIG_FILE=${pkgs.makeFontsConf { fontDirectories = [ pkgs.stix-two pkgs.ibm-plex ]; }}
          exec ${package}/bin/home-theatre-launcher --demo --windowed "$@"
        '');
      };

      checks.${system} = {
        inherit package;
        vm = pkgs.testers.runNixOSTest (import ./nix/tests/vm.nix { inherit self; });
      };

      devShells.${system}.default = pkgs.mkShell {
        inputsFrom = [ package ];
      };
    };
}
