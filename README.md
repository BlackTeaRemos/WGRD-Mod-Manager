# WGRD Mod Manager

A signed, peer to peer mod manager for Wargame: Red Dragon.

## What it does

**Distributes mods peer to peer.**
Releases move over BitTorrent v2 between players.

**Only downloads what changed.**
Content is split into chunks by content, not by offset.
An update to a 4 GiB mod transfers the few megabytes that actually differ, and patches the existing files in place rather than rewriting the folder.

**Verifies everything before it exists.**
Every release is an ed25519 signed manifest.
A signature that does not check against a registered key is discarded before it is parsed, shown, or counted.
Mod identity is namespaced under the publisher key, so nobody can shadow another author's mod.

**Handles revocation.**
A publisher can retire their own key with a signed certificate.
Managers stop trusting it, stop seeding its content, and mark anything installed from it as unsigned.
Your files are never deleted.

**Manages load order.**
Drag entries, toggle them, and the manager writes `Mods/load_order.txt`.
That is the only file it puts in your game directory.
Warnings annotate, they never block.

**Manages profiles.**
Each Steam account gets its own profiles.
A profile pairs a load order with a copy of your `.wargameprofile` save, so switching setups switches both.
The active profile follows your live order automatically.

**Installs the patcher.**
Fetches and installs [WRG-Patcher](https://github.com/BlackTeaRemos/WRG-Patcher), the proxy DLL that makes the game load mods at all, and keeps it current.

**Updates itself.**
Checks its own releases and swaps the executable in place.

## Installing

Drop `wgrd-mod-manager.exe` into your Wargame: Red Dragon folder, beside `WarGame3.exe`, and run it.

## Publishing a mod

1. CREATE KEY in the Publish tab, choosing where to store it
2. Submit `keys/<fingerprint>.json` to the [signature registry](https://github.com/BlackTeaRemos/WGRD-Mod-Manager-Signatures)
3. Pick your mod folder, unlock the key, SIGN AND ANNOUNCE

Your key file stays on your machine.

## Building

C++26, MSVC, CMake, Ninja, vcpkg.
Windows only, x64.

```powershell
. ./scripts/Enter-BuildEnvironment.ps1
cmake --preset windows-release
cmake --build --preset windows-release
```

Copy `scripts/ToolchainPaths.local.ps1.example` to `scripts/ToolchainPaths.local.ps1` and point it at your tools first.

## Licence

GNU General Public License, version 3.
See [LICENSE](LICENSE).

Contributions are accepted under the [Contributor License Agreement](CLA.md), which lets the project owner relicense the combined work.
Third party components and their licences are listed in [THIRD-PARTY.md](THIRD-PARTY.md).
