# Third party licences

WGRD Mod Manager is distributed under the GNU General Public License, version 3.
Everything below is linked statically into the shipped executable.

| Component | Licence |
|---|---|
| libtorrent-rasterbar | BSD-3-Clause |
| Dear ImGui | MIT |
| FreeType | FTL |
| SQLite | public domain |
| libsodium | ISC |
| BLAKE3 | CC0-1.0 or Apache-2.0 |
| libgit2 | GPL-2.0 with linking exception |
| nlohmann/json | MIT |
| Catch2 | BSL-1.0 |

Catch2 is used by the test targets and is not shipped.

## Source availability

Distributing a binary built from this repository requires offering the complete corresponding source, including the CMake files, presets, and the vcpkg manifest that produce it.

## Runtime components that are not linked

The WRG-Patcher proxy DLL is a separate program under the Apache License 2.0.
