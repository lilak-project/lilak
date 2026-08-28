\page run LKRun — Run Control

# LKRun — Run Control

`LKRun` is the central engine of a LILAK analysis.
It manages input/output files, the branch system, the task pipeline, and the event loop.
There is always exactly one `LKRun` instance per process, accessible via `LKRun::GetRun()`.

---

## Minimal macro

```cpp
void run_analysis()
{
    auto run = LKRun::GetRun();
    run -> AddPar("config.mac");

    run -> Add(new MyTask());

    run -> Init();
    run -> Run();
}
```

---

## Parameter file

All `LKRun` settings can be controlled from the parameter file.
Run `lilak collect_par config.mac` to see all available keys with their defaults.

### Run identity

```
LKRun/Name    myrun       # run name
LKRun/RunID   42          # run number (used in output file name)
LKRun/Tag     sim         # optional tag appended to file name
```

Output file name is automatically composed as:
```
myrun_0042.sim.[lilak_version].root
```

### Input file

```
LKRun/InputFile   /data/myrun_0042.root
```

To chain multiple runs by ID:
```
LKRun/RunIDList    1, 2, 3, 4
LKRun/InputPath    /data/
LKRun/SearchRun    conv           # searches /data/run_*.[conv].root
```

To specify a range:
```
LKRun/RunIDRange   1  10          # runs 1 through 10
```

### Output file

```
LKRun/OutputPath   /data/output/
```

If `LKRun/OutputPath` is not set, output goes to `lilak/data/` by default.

### Other options

| Parameter | Default | Description |
|-----------|---------|-------------|
| `LKRun/EntriesLimit` | — | Maximum number of events to process |
| `LKRun/EventCountForMessage` | 20000 | Print progress every N events |
| `LKRun/AutoTerminate` | true | Exit ROOT automatically after run |
| `LKRun/UpdateOutputFile` | false | Open output file with `UPDATE` instead of `RECREATE` |
| `LKRun/FriendFile` | — | Add a friend TChain to the input tree |

---

## Input and output in C++

### Adding input files

```cpp
run -> AddInputFile("/data/run0042.root");
run -> AddInputFile("/data/run0043.root");   // chained automatically
```

### Setting output

```cpp
run -> SetOutputFile("/data/run0042_ana.root");
run -> SetDataPath("/data/output/");          // or set output directory only
```

### Run name and ID

```cpp
run -> SetRunName("myrun", 42);          // name + ID
run -> SetRunName("myrun", 42, -1, "sim"); // with tag
```

---

## Branch system

Branches are `TClonesArray` objects shared between tasks through `LKRun`.

### Registering a branch (output)

Called in `Init()` by the task that **owns** the branch:

```cpp
fOutputArray = fRun -> RegisterBranchA("Hit", "LKHit", 100);
```

The third argument is the expected maximum number of objects per event
(the array grows automatically if exceeded).

Branch persistency (whether data is written to the output ROOT file) defaults to `true`.
It can be overridden from the parameter file:

```
Hit/persistency   false    # keep branch in memory but do not write to file
```

### Getting a branch (input)

Called in `Init()` by tasks that **read** the branch:

```cpp
fInputArray = fRun -> GetBranchA("Hit");
if (fInputArray == nullptr) {
    lk_error << "Hit branch not found" << endl;
    return false;
}
```

### Saving a non-array object

To write a single object (not a `TClonesArray`) directly to the output file:

```cpp
run -> RegisterObject("MySummary", myParContainer);
```

---

## Detectors

Detectors are registered with `LKRun` before `Init()`.
They provide geometry, pad lookup, and coordinate transforms to tasks.

```cpp
run -> AddDetector(new STARK());
run -> AddDetectorPlane(new MyPlane());
```

After `Init()`, any task can look up a detector or plane by name:

```cpp
auto plane = (SKSiArrayPlane*) fRun -> FindDetectorPlane("SKSiArrayPlane");
```

---

## Event loop

### Full run

```cpp
run -> Init();
run -> Run();            // all events
run -> Run(100);         // first 100 events
run -> Run(200, 300);    // events 200 to 300
```

### Interactive stepping

Useful for debugging or event display:

```cpp
run -> Init();
run -> ExecuteNextEvent();      // step forward
run -> ExecutePreviousEvent();  // step backward
run -> ExecuteFirstEvent();
run -> ExecuteLastEvent();
run -> RunEvent(42);            // jump to event 42
```

### Controlled from parameter file

```
lilak/run       100    # run 100 events after Init()
lilak/execute   42     # execute single event 42 after Init()
```

### Stopping early from a task

A task can signal the run to stop after the current event:

```cpp
fRun -> SignalEndOfRun();
```

---

## Run header

The run header is a `LKParameterContainer` written to the output file.
It records run metadata (name, ID, git tags, parameter snapshot).
Read it back from an output file:

```cpp
auto file = new TFile("output.root");
auto header = (LKParameterContainer*) file -> Get("RunHeader");
header -> Print();
```

---

## Printing run info

```cpp
run -> Print();          // print everything
run -> Print("par");     // parameters only
run -> Print("task");    // task list only
run -> Print("in");      // input files
run -> Print("out");     // output file
run -> Print("det");     // detectors
```

Or from the parameter file after `Init()`:
```
lilak/print   all
```

---

## Typical run sequence

```
AddPar()  →  Add(tasks)  →  Add(detectors)  →  Init()  →  Run()
                                                  │
                                    ┌─────────────┘
                                    │
                              StartOfRun()
                              for each event:
                                  GetEntry()         ← read input tree
                                  ClearArrays()      ← clear all branches
                                  ExecTasks()        ← call Exec() on all tasks
                                  FillOutputTree()   ← write event to output
                              EndOfRunTasks()        ← call EndOfRun() on all tasks
                              WriteOutputFile()      ← flush output ROOT file
                              EndOfRun()
```
