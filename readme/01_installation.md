\page installation Installation

# Installation

## Requirements

| Requirement | Version | Notes |
|-------------|---------|-------|
| [ROOT](https://root.cern) | 6.x | Required |
| CMake | 3.5+ | Required |
| C++ compiler | C++11 or later | Required |
| Geant4 | any recent | Optional — simulation only |
| NPTool | any recent | Optional — requires Geant4 |
| GRU / GET | — | Optional — GET electronics raw data converter |

---

## Download

Clone the LILAK repository:
```sh
git clone git@github.com:lilak-project/lilak.git
cd lilak/
```

Then clone the project repository you want to work with:
```sh
git clone git@github.com:lilak-project/atomx.git   # AToMX experiment
git clone git@github.com:lilak-project/stark.git   # STARK experiment
```

Each project lives inside the LILAK directory as a sibling to `source/`.
You can clone more than one project at a time.

---

## Build

Run `lilak.sh` from the LILAK root directory:
```sh
./lilak.sh
```

This launches an interactive configuration script (`macros/lilak_configuration.py`) that:
1. Asks which projects to include
2. Asks which optional features to enable (Geant4, NPTool, MFM converter, EVE, etc.)
3. Writes `meta/build_options.cmake`
4. Runs CMake and builds the shared library

After a successful build, the shared library is placed in `build/libLILAK.so` (or `.dylib` on macOS),
and the ROOT login script is registered in `~/.rootrc` so that the library is loaded automatically
whenever ROOT starts.

### Build options

Optional features are controlled in `meta/build_options.cmake`:

| Option | Default | Description |
|--------|---------|-------------|
| `ACTIVATE_EVE` | OFF | Enable ROOT EVE 3-D visualisation |
| `BUILD_GEANT4_SIM` | OFF | Build Geant4 simulation executable |
| `BUILD_NPTOOL` | OFF | Link against NPTool (requires Geant4) |
| `BUILD_MFM_CONVERTER` | OFF | Build MFM frame data converter |
| `BUILD_JSONCPP` | OFF | Build with JsonCpp support |
| `LILAK_PROJECT_LIST` | — | Whitespace-separated list of project subdirectories to include |

You can edit `meta/build_options.cmake` by hand and then rebuild with:
```sh
cmake --build build
```

Or re-run `./lilak.sh` to use the interactive configurator again.

---

## Rebuild after changes

For source code changes, a simple rebuild is enough:
```sh
cmake --build build
```

If you add new source files or change `CMakeLists.txt`, re-run CMake first:
```sh
cmake -B build && cmake --build build
```

---

## Deactivate

To stop LILAK from loading automatically when ROOT starts, comment out the `Rint.Logon` line in `~/.rootrc`:
```
#Rint.Logon: /path/to/lilak/macros/rootlogon.C
```
