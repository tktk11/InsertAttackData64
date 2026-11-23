# InsertAttackData64

SKSE plugin (`InsertAttackData.dll`) used by Ultimate Combat and Ultimate Dragons. It reads the JSON
files in `Data/Meshes/AttackData` and adds or overrides the attack data (`BGSAttackData`) of races
when they are loaded.

libskyrim code from himika https://github.com/himika/libSkyrim

## Supported game versions

One DLL is built per game version. Use the DLL that matches your `SkyrimSE.exe`.

| Game version | SKSE |
| --- | --- |
| 1.7.104 (AE) | 2.3.x |
| 1.6.1170 (AE) | 2.2.x |
| 1.5.97 (SE) | 2.0.19 / 2.0.20 |

A DLL refuses to load on any other game version (AE: through `SKSEPlugin_Version`, SE: in
`SKSEPlugin_Query`), so a wrong DLL never touches game memory.

## JSON keys

Each top-level key selects races by behavior graph model name, editor ID, or
`"<plugin name> <6-digit hex local form ID>"` (lower case; races from `Skyrim.esm` use the 6-digit
hex ID only). Races from light (ESL) plugins can be selected by behavior graph or editor ID only.

## Installation

Copy the files of the archive for your game version to `Data/SKSE/Plugins` (or install the archive
with a mod manager), overwriting the old `InsertAttackData.dll`.
The [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444) is **not** required.

## Building

The same sources are built once per SKSE SDK (skse64 2.3.1 for 1.7.104, 2.2.6 for 1.6.1170,
2.0.20 for 1.5.97); each build targets the runtime of its SDK (`CURRENT_RELEASE_RUNTIME`).

1. Get the SKSE source and its `common` folder.
2. Clone this repository into the skse64 solution folder.
3. Set the `skse64` project to *Static Library* and retarget the SKSE projects to your toolset (v143).
4. Build `insertattackdata_plugin.vcxproj` as `Release | x64` (C++17).

Game addresses for each runtime are in `main.cpp` / [libskyrim/GameAddresses.h](libskyrim/GameAddresses.h),
together with how they were located.
