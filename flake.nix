# NixOS: `nix run github:michaelpfister/ch-nsl-snap`, or `nix build` in a
# checkout (docs/07-development-setup.md, "NixOS").
{
  description = "NSL Snap: screenshot tool for clean window captures";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (pkgs: rec {
        nsl-snap = pkgs.callPackage ./nix/package.nix { };
        default = nsl-snap;
      });

      # The build tools and libraries, for working on the code.
      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ self.packages.${pkgs.stdenv.hostPlatform.system}.nsl-snap ];
        };
      });
    };
}
