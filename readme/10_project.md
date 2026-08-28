\page project Creating a Project

# Creating a Project

LILAK separates the **framework** (`source/`) from **projects** — one directory
per experiment (`atomx`, `stark`, …) holding that experiment's containers,
detectors, tasks, tools, and run macros. A project is a sibling directory of
`lilak/` that is registered with the build and pulled in as a CMake
sub-directory.

This page shows how to create, register, populate, and build a project.

---

## Directory layout

A project mirrors the framework's source categories. A typical project looks
like:

```
myexp/
├── CMakeLists.txt        # registers this project's source directories
├── .lilak                # project marker / description
├── container/            # project-specific containers  (→ LKContainer)
├── detector/             # detectors and detector planes (→ LKDetector*)
├── task/                 # analysis tasks               (→ LKTask)
├── tool/                 # helper tools
├── macros/               # run / draw / read macros (+ macros/data/)
├── simulation/           # (optional) Geant4 simulation
└── nptool/               # (optional) NPTool detector definitions
```

Only the directories you need have to exist; the `CMakeLists.txt` lists exactly
which ones to compile.

---

## 1. Scaffold with `lilak new`

The easiest way to start is the interactive creator (after sourcing
`lilak.sh`):

```sh
lilak new
```

This runs `macros/project_class_creator.py`, which:

1. Asks for the project name and description, and creates the directory next to
   `lilak/` with a `.lilak` marker and a copied `.gitignore`.
2. Lets you create each sub-directory (`container`, `detector`, `task`,
   `tool`, `macros`, …).
3. For each code sub-directory, prompts for class names and generates the
   header + source skeletons via the class creator (see below).

The result is a ready-to-build project with a correct `CMakeLists.txt`.

---

## 2. The project CMakeLists.txt

A project's `CMakeLists.txt` simply appends its source directories to the
framework's global lists. For example, `atomx/CMakeLists.txt`:

```cmake
set(LILAK_SOURCE_DIRECTORY_LIST ${LILAK_SOURCE_DIRECTORY_LIST}
    ${CMAKE_CURRENT_SOURCE_DIR}/container
    ${CMAKE_CURRENT_SOURCE_DIR}/detector
    ${CMAKE_CURRENT_SOURCE_DIR}/task
    ${CMAKE_CURRENT_SOURCE_DIR}/tool
    CACHE INTERNAL ""
)

# optional: NPTool detector definitions
set(LILAK_NPTOOL_SOURCE_DIRECTORY_LIST ${LILAK_NPTOOL_SOURCE_DIRECTORY_LIST}
    ${CMAKE_CURRENT_SOURCE_DIR}/nptool
    CACHE INTERNAL ""
)
```

Each listed directory is globbed for `*.cpp`/`*.h`, a ROOT dictionary is
generated, and the files are compiled into the LILAK library. To add a new
category later, just add another line here.

---

## 3. Register the project with the build

Projects are built only if they appear in `LILAK_PROJECT_LIST`. This list lives
in `meta/build_options.cmake` and is managed by the configuration step — do not
hand-edit unless you know what you are doing:

```cmake
set(LILAK_PROJECT_LIST ${LILAK_PROJECT_LIST}
atomx
stark
myexp
CACHE INTERNAL ""
)

set(LILAK_PROJECT_MAIN myexp CACHE INTERNAL "")
```

- **`LILAK_PROJECT_LIST`** — every project compiled into the build.
- **`LILAK_PROJECT_MAIN`** — the default project (used by `lilak <macro>`, git-log
  collection, etc.).

To set these interactively, run the configuration flow editor:

```sh
./lilak.sh            # opens the config flow editor, then builds
lilak configure       # config editor only (same as above without build)
```

The top of `lilak/CMakeLists.txt` iterates the list and adds each project:

```cmake
foreach(lilak_project ${LILAK_PROJECT_LIST})
    add_subdirectory(${lilak_project})
endforeach()
```

---

## 4. Add classes with the class creator

New classes can be generated at any time (not only at `lilak new` time). The
creator (`macros/project_class_creator.py`, class `lilakcc`) writes a fully
commented header + source pair for the right base class. The mode determines
the base:

| Mode | Base class | Generates |
|------|------------|-----------|
| `container` | `LKContainer` | data container (\ref container) |
| `task` | `LKTask` | analysis task (\ref task) |
| `detector` | `LKDetector` + `LKDetectorPlane` | detector and its plane (\ref detector) |
| `pad_plane` | `LKPolygonPadPlane` | pad-plane detector |
| `tool` | `TObject` | helper tool (\ref tools) |
| `geant4dc` | Geant4 detector construction | simulation geometry |

The skeletons include the boilerplate that LILAK expects — `Clear()`,
`Copy()`, `Print()`, parameter `UpdatePar` calls, branch registration, and
`ClassDef`/`ClassImp` — so you only fill in the physics.

---

## 5. A minimal run macro

Put run macros under `myexp/macros/`. A run macro wires up the pipeline
(see \ref run and \ref task):

```cpp
// myexp/macros/run_myexp.C
void run_myexp()
{
    auto run = LKRun::GetRun();
    run -> AddPar("config.mac");

    run -> AddDetector(new MyDetector());
    run -> Add(new MyConversionTask());   // event trigger
    run -> Add(new MyHitTask());
    run -> Add(new MyTrackingTask());

    run -> Init();
    run -> Run();
}
```

Generate a starting `config.mac` from an input ROOT file with:

```sh
lilak make_run /data/myrun_0001.root
```

and inspect or edit parameters with the web editor:

```sh
lilak par config.mac
```

---

## 6. Build and run

```sh
lilak build          # configure (if needed) + compile, parallel with -jN
lilak build -j10
```

After a successful build, source the environment and run:

```sh
source lilak.sh
lilak myexp/macros/run_myexp.C
```

`lilak build` also runs `make_meta` for new/changed classes and warns about
containers missing a read/draw configuration, so a freshly added container is
immediately usable from `lilak read` / `lilak draw`.

---

## Useful project commands

| Command | Action |
|---------|--------|
| `lilak new` | Create a new project (interactive) |
| `lilak configure [input]` | Open the configuration / input flow editor |
| `lilak build [-jN \| configure]` | Build the package |
| `lilak build_new` | Clean `build/` and rebuild from scratch |
| `lilak update` | Pull updates for `lilak` and all project dirs |
| `lilak find <name>` | Jump to the directory containing `<name>` |
| `lilak make_meta [class]` | (Re)generate meta parameter files |
| `lilak <macro>` | Run a macro in the LILAK environment |
| `lilak par <file>` | Open the parameter editor in the browser |
| `lilak si_mapping <file>` | Open the silicon-detector mapping editor |
