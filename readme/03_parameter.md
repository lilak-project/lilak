\page parameter Parameter System

# Parameter System

All configurable values in LILAK — run settings, task options, detector geometry, histogram binning —
are controlled through plain-text parameter files.
The `LKParameterContainer` class reads these files, stores the parameters, evaluates expressions,
and provides type-safe access from C++ code.

---

## Parameter file syntax

A parameter file is a plain-text file (`.mac`, `.par`, `.conf`).
Each non-empty line defines one parameter, a comment, or a special directive.

```
# this is a comment line

GroupName/ParameterName   value   # inline comment
```

### Basic rules

- `#` starts a comment. Lines beginning with `#` are ignored.
- A parameter name must not contain spaces. Use `/` to separate group and name.
- A value is everything between the name and the `#` comment (trailing spaces stripped).
- Multiple values on one line are separated by spaces or commas.

---

## Groups

Groups organise related parameters under a common prefix.

**Inline group prefix** (most common):
```
LKRun/InputFile   /data/run0042.root
LKRun/OutputFile  /data/run0042_ana.root
MyTask/threshold  100
MyTask/tbRange    0  512
```

**Block group** (indent-based, useful for many parameters under one class):
```
MyTask/
    threshold   100
    tbRange     0  512
    scale       1.5
```

The final parameter name is always `group/name`, so both forms are equivalent.
Groups can be nested:

```
detector/
    tpc/
        driftVelocity   0.5
        padSize         2.0
    silicon/
        threshold       50
```

---

## Value types

The type of a parameter is determined at read time by the getter you call —
the same raw value can be interpreted as different types.

```
size       100
title      this is title 5
dimension  50  60  70
ratio      0.5
flag       true
color      kRed+1
axis       z
```

| Getter | Example result |
|--------|---------------|
| `GetParInt("size")` | `100` |
| `GetParDouble("ratio")` | `0.5` |
| `GetParBool("flag")` | `true` |
| `GetParString("title")` | `"this is title 5"` |
| `GetParString("title", 0)` | `"this"` |
| `GetParString("title", 3)` | `"5"` |
| `GetParDouble("title", 3)` | `5.0` |
| `GetParV3("dimension")` | `TVector3(50, 60, 70)` |
| `GetParColor("color")` | `633` (ROOT Color_t) |
| `GetParAxis("axis")` | `LKVector3::kZ` |

**Multiple values** (comma or space separated):
```
tbRange    0  512
channels   1, 3, 5, 7
```
```cpp
int tb1 = fPar -> GetParInt("tbRange", 0);   // 0
int tb2 = fPar -> GetParInt("tbRange", 1);   // 512

auto chList = fPar -> GetParVInt("channels"); // std::vector<int>{1,3,5,7}
```

---

## Expressions and references

### Reference another parameter

Use `{name}` to substitute the value of another parameter:

```
beamEnergy   210
targetMass   {beamEnergy}*4    # evaluated to 840
length       {dimension[2]}+30 # uses 3rd value of "dimension"
```

`GetParDouble("targetMass")` returns `840.0`.
`GetParString("targetMass")` returns the raw string `"210*4"` (unevaluated).

### Reference an environment variable

Use `e{VAR}` to substitute an environment variable:

```
dataPath    e{LILAK_DATA}/run0042.root
rootSys     e{ROOTSYS}/lib
```

---

## Include another file

Use `<` to include another parameter file:

```
<common    path/to/common_parameters.mac
```

Later parameters override earlier ones, so project-specific files can override common defaults.

---

## Conditional parameters

Use `@` to define a parameter that is only applied when its group name has been set as a parameter value elsewhere:

```
using       option1

@option1/par   value1   # applied — "option1" was set above
@option2/par   value2   # ignored — "option2" was not set
@option3/par   value3   # ignored
```

This is useful for switching between named configurations in one file.

---

## Duplicate names

If the same name appears more than once, the later value overwrites the earlier one:

```
color   kRed+1
# ... many lines later ...
color   kBlue+3          # this wins
```

If you need multiple values under the same name intentionally, use indexed access:
```
SameName   1
SameName   2
SameName   3
```
```cpp
fPar -> GetParInt("SameName", 0);  // 1
fPar -> GetParInt("SameName", 1);  // 2
fPar -> GetParInt("SameName", 2);  // 3
```

---

## `lilak/` reserved parameters

Parameters under the `lilak/` group are reserved for controlling `LKRun` behaviour directly from
the parameter file. They are temporary — written during a run and not persisted to the output.

| Parameter | Default | Description |
|-----------|---------|-------------|
| `lilak/add` | — | Add a task or detector class by name |
| `lilak/run` | `0` | Run N events after init (0 = all) |
| `lilak/execute` | `0` | Execute a single event number after init |
| `lilak/collect_par` | — | Write collected parameters to this file |
| `lilak/print` | — | Print run info after init (e.g. `all`, `par`, `task`) |
| `lilak/auto_exit` | `0` | Auto-exit ROOT after run finishes |
| `lilak/draw` | `0` | Call Draw() after init |

Example — run 100 events and exit automatically:
```
lilak/run        100
lilak/auto_exit  1
```

---

## Accessing parameters in C++

### `UpdatePar` — preferred

`UpdatePar` updates the member variable only if the parameter exists.
If the parameter is absent the variable keeps its default value.
**Always use `UpdatePar` as the first choice.**

```cpp
int    fThreshold = 100;    // default
double fScale     = 1.0;
bool   fInverted  = false;
TString fName     = "default";

fPar -> UpdatePar(fThreshold, "MyTask/threshold");
fPar -> UpdatePar(fScale,     "MyTask/scale");
fPar -> UpdatePar(fInverted,  "MyTask/inverted");
fPar -> UpdatePar(fName,      "MyTask/name");
```

### `CheckPar` + `GetPar*` — when parameter is required

`GetPar*` throws an error if the parameter does not exist.
Only use it after `CheckPar`, or when the parameter is truly required and its absence is a hard error.

```cpp
// Safe pattern for a required parameter:
if (!fPar -> CheckPar("MyTask/inputFile")) {
    lk_error << "MyTask/inputFile is required!" << endl;
    return false;
}
TString path = fPar -> GetParString("MyTask/inputFile");
```

### `GetPar*` with index

```cpp
int tb1 = fPar -> GetParInt("MyTask/tbRange", 0);
int tb2 = fPar -> GetParInt("MyTask/tbRange", 1);
```

### Histogram binning

`UpdateBinning` is a convenient wrapper for the common pattern of reading
`n`, `x1`, `x2` for a histogram axis from one parameter:

```
# config.mac
analysis/binning_energy   400  0  10    # n  xmin  xmax
```

```cpp
int ne = 200; double e1 = 0, e2 = 5;   // defaults
fPar -> UpdateBinning("analysis/binning_energy", ne, e1, e2);
// ne=400, e1=0, e2=10 if the parameter exists; defaults otherwise

auto hist = new TH1D("hEnergy", ";Energy (MeV)", ne, e1, e2);
```

For 2-D histograms:
```cpp
int nx, ny; double x1, x2, y1, y2;
fPar -> UpdateBinning("analysis/binning_xy", nx, x1, x2, ny, y1, y2);
```

---

## Loading parameter files

### In a macro

```cpp
auto run = LKRun::GetRun();
run -> AddPar("config.mac");           // primary config
run -> AddPar("common/detector.mac");  // additional file (appended)
```

### In a task

```cpp
// In Init(), before Init() can return false:
fPar -> AddFile("extra_settings.mac");
fPar -> UpdatePar(fValue, "MyTask/value");
```

### Multiple files and override order

Files added later override parameters from earlier files.
Use this to have a common base config that project-specific files override:

```
# project_config.mac
<common   /path/to/common/detector.mac   # loaded first
MyTask/threshold  200                    # overrides common value
```

---

## Collecting parameters

To discover all parameters a set of tasks reads — including their defaults — without processing any data:

```sh
lilak collect_par config.mac
```

This rewrites `config.mac` in place with all collected parameters included.
You can then open it in the parameter editor to adjust values:

```sh
lilak par config.mac
```

Alternatively, from C++:

```cpp
run -> Add(new MyTask());
run -> InitAndCollectParameters("collected.mac");
```
