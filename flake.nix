{
  description = "Twiiiiiter, a TUI Twitter clone written in C.";

  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.flake-compat = {
    url = "github:edolstra/flake-compat";
    flake = false;
  };

  outputs = { self, nixpkgs, flake-utils, flake-compat }:
    (flake-utils.lib.eachDefaultSystem
      (system:
        let pkgs = nixpkgs.legacyPackages.${system}; in
        {
          packages.default = pkgs.stdenv.mkDerivation {
            name = "twiiiiiter";
            src = ./.;
            nativeBuildInputs = [ pkgs.cmake ];
            buildInputs = [ pkgs.sqlite.dev ];

            installPhase = ''
              mkdir $out
              mkdir $out/bin
              mv client/twiiiiiter-client $out/bin
              mv server/twiiiiiter-server $out/bin
            '';
          };
        }
      )) // {
        nixosModules.default = { config, pkgs, lib, ... }:
          let cfg = config.services.twiiiiiter-server; in
          with lib;
          {
            options.services.twiiiiiter-server = {
              enable = mkEnableOption "Twiiiiiter server";
              port = mkOption {
                type = types.int;
                description = "The TCP port the server should use";
              };
            };
            
            config.systemd.services.twiiiiiter-server = mkIf cfg.enable {
              description = "Twiiiiiter server";
              wantedBy = [ "multi-user.target" ];
              after = [ "network.target" ];
              serviceConfig = {
                WorkingDirectory = "${self.packages.${pkgs.system}.default}/";
                ExecStart = "${self.packages.${pkgs.system}.default}/bin/twiiiiiter-server ${cfg.port}";
                Type = "simple";
              };
            };
          };
      };
}