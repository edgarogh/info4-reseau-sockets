# Twiiiiiter

A simple TUI Twitter clone written in C, for the TR class at Polytech Grenoble.

## Installing with Nix

### NixOS

If you have NixOS, with [Flakes enabled](https://nixos.wiki/wiki/Flakes#NixOS) :

```nix
# in /etc/nixos/flake.nix
{
  # add this repository as an input
  inputs.twiiiiiter = "github:edgarogh/info4-reseau-sockets";

  # if it is the first extra input, you may need to edit the
  # output to take extra arguments, and pass them to your configuration
  outputs = { self, nixpkgs, ... } @ attrs: { # the important part here is the `... } @ attrs`
    nixosConfigurations.yourHostnameHere = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      specialArgs = attrs; # and here we pass our inputs to our configuration
      modules = [ ./configuration.nix ];
    };
  };
}
```

```nix
# in /etc/nixos/configuration.nix
{ config, lib, nixpkgs, twiiiiiter, ... }: {
  environment.systemPackages = [
    twiiiiter.packages.x86_64-linux.default
  ];
}
```

It will install the client and the server.

You can also run the server as a systemd service with:

```nix
services.twiiiiiter-server = {
  enable = true;
  port = 7700; # optional  
};
```

A demo server (powered by Nix) is running at `wp-corp.eu.org:7700`.

### Non-NixOS

```bash
nix build github:edgarogh/info4-reseau-sockets
# run the server with
./result/bin/twiiiiiter-server
# and then in another shell, start a client with
./result/bin/twiiiiiter-client 127.0.0.1
```

### Starting a development shell and compiling by hand

Nix provides us a reproducible development environment, that can be accessed with this command:

```bash
# in the cloned repository

# if you have nix flakes enabled
# see <https://nixos.wiki/wiki/Flakes#Permanent> to enable them
nix develop

# otherwise, you can still use the legacy `nix-shell` command
nix-shell
```

Once you are in the shell, you can build the project with CMake:

```bash
cmake -S . -B build
cd build
make

# to run the server
./server/twiiiiiter-server

./client/twiiiiiter-client 127.0.0.1
```

## Utilisation

> requires a running twiiiiit server

1. Connection with an user id.
2. Commands
```
Publish a twiiiiit : P <message>
Subscribe: S <user>
Unsubscribe : U <user>
List all your subscriptions : L
Help : H
```
3. Close the program to disconnect