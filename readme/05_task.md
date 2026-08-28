\page task LKTask — Analysis Task

# LKTask — Analysis Task

`LKTask` is the basic building block of a LILAK analysis pipeline.
Each step of the analysis — data conversion, hit extraction, tracking, etc. — is implemented as an independent task.
`LKTask` inherits from ROOT's `TTask` and from `LKGear`, which provides access to the parameter container (`fPar`) and the run object (`fRun`).

```
LKRun
 └─ LKTask  (parent task)
     ├─ LKTask  (child task A)
     └─ LKTask  (child task B)
```

---

## Minimal Implementation

**Header:**

```cpp
#include "LKTask.h"

class MyTask : public LKTask
{
    public:
        MyTask();
        virtual ~MyTask() {}

        bool Init();
        void Exec(Option_t *option = "");
        bool EndOfRun();            // optional

    private:
        TClonesArray *fInputArray  = nullptr;   // branch owned by another task
        TClonesArray *fOutputArray = nullptr;   // branch created by this task

        int fThreshold = 100;       // parameter with default value

    ClassDef(MyTask, 1)
};
```

**Source:**

```cpp
#include "LKRun.h"
#include "LKLogger.h"
#include "MyTask.h"

ClassImp(MyTask)

MyTask::MyTask() : LKTask("MyTask", "MyTask") {}

bool MyTask::Init()
{
    // ── 1. Load parameters FIRST ──────────────────────────────────────────
    //   Parameters must be loaded before any line that can return false,
    //   so that parameter collection works even without an input file.
    fPar -> UpdatePar(fThreshold, "MyTask/threshold");

    // ── 2. Register output branches ───────────────────────────────────────
    //   Branches created by this task must be registered here.
    fOutputArray = fRun -> RegisterBranchA("MyOutput", "LKHit", 100);

    // ── 3. Connect input branches ─────────────────────────────────────────
    //   These may return nullptr if the branch does not exist yet.
    fInputArray = fRun -> GetBranchA("RawData");
    if (fInputArray == nullptr) {
        lk_error << "Branch [RawData] not found!" << endl;
        return false;
    }

    return true;
}

void MyTask::Exec(Option_t *option)
{
    // Clear the output branch at the start of every event.
    // This is required for all branches registered by this task.
    fOutputArray -> Clear("C");

    // --- analysis logic ---
    int numData = fInputArray -> GetEntriesFast();
    for (auto i = 0; i < numData; ++i) {
        auto data = (MyData *) fInputArray -> At(i);
        // ... process data, fill fOutputArray ...
    }

    lk_info << fOutputArray->GetEntriesFast() << " objects created" << endl;
}

bool MyTask::EndOfRun()
{
    // Called once after all events are processed.
    // Use this to write histograms, print summaries, etc.
    return true;
}
```

---

## Callback Methods

| Method | When called | Typical use |
|--------|-------------|-------------|
| `Init()` | Once before the event loop | Load parameters, register/connect branches, book histograms |
| `Exec()` | Once per event | Analysis logic |
| `EndOfRun()` | Once after all events | Save histograms, print run summary |

---

## Built-in Members

`LKTask` provides two members that are automatically wired up when the task is added to the run.

### `fPar` — Parameter container

**Prefer `UpdatePar` over `GetPar*`.**
`UpdatePar` silently keeps the member's default value when the parameter is absent.
`GetPar*` functions throw an error if the parameter does not exist, so only use them
when the parameter is absolutely required and its absence is a hard error.

```cpp
// ✅ Preferred: updates fThreshold only if "MyTask/threshold" exists in the .mac file.
//    If the parameter is missing, fThreshold keeps its default value.
fPar -> UpdatePar(fThreshold, "MyTask/threshold");

// ✅ Safe pattern when the parameter is truly required:
if (fPar -> CheckPar("MyTask/inputFile")) {
    TString path = fPar -> GetParString("MyTask/inputFile");
    // ... use path ...
} else {
    lk_error << "MyTask/inputFile is required!" << endl;
    return false;
}

// ⚠️  Only use GetPar* when you are certain the parameter exists,
//     or when a missing parameter should be a fatal error.
int    n   = fPar -> GetParInt("MyTask/count");
double val = fPar -> GetParDouble("MyTask/scale");
TString s  = fPar -> GetParString("MyTask/name");
```

### `fRun` — Run access

```cpp
// Register a new output branch (call in Init)
fRun -> RegisterBranchA("BranchName", "ClassName", expectedSize);

// Get an existing branch (call in Init, after parameters)
fRun -> GetBranchA("BranchName");

// Current event number (call in Exec)
fRun -> GetCurrentEventID();

// Run header (call in EndOfRun)
fRun -> GetRunHeader();
```

---

## Branch Rules

| Branch type | How to obtain | `Clear("C")` in `Exec()` |
|-------------|---------------|--------------------------|
| Created by this task | `fRun -> RegisterBranchA(...)` in `Init()` | **Required** |
| Created by another task | `fRun -> GetBranchA(...)` in `Init()` | Never — owned by the other task |

`Clear("C")` resets the `TClonesArray` at the start of each event so that
objects from the previous event do not persist into the current one.

---

## Registering Tasks

Tasks are added to `LKRun` in the order they should execute.

```cpp
// run_analysis.C
auto run = LKRun::GetRun();
run -> AddPar("config.mac");

run -> Add(new ConversionTask());   // runs first
run -> Add(new ExtractionTask());   // runs second
run -> Add(new TrackingTask());     // runs third

run -> Init();
run -> Run();
```

A task can also own child tasks. The parent's parameter container is
automatically shared with all children.

```cpp
auto parent = new ParentTask();
parent -> Add(new ChildTask());     // shares fPar automatically
run -> Add(parent);
```

---

## Logging

```cpp
lk_info    << "informational message" << endl;
lk_warning << "warning message"       << endl;
lk_error   << "error message"         << endl;
lk_debug   << "debug message"         << endl;
```

---

## Event Trigger Task

Normally, `LKRun` drives the event loop by reading entries from a LILAK-format
ROOT input file. An **Event Trigger Task** takes over this role when:

- the input data is in a non-LILAK format (e.g. GET binary, MFM frames), or
- events are generated on-the-fly (e.g. online data acquisition, simulation).

The trigger task reads or produces data, then calls `SignalNextEvent()` to
tell `LKRun` that one event is ready. `LKRun` then calls `Exec()` on all
other tasks in the pipeline for that event.

**To implement an Event Trigger Task:**

```cpp
class MyTriggerTask : public LKTask
{
    public:
        MyTriggerTask();
        virtual ~MyTriggerTask() {}

        bool IsEventTrigger() { return true; }  // marks this as the trigger

        bool Init();
        void Run(Long64_t numEvents = -1);      // drives the event loop
        void SignalNextEvent();                 // called internally to fire one event
        bool EndOfRun();

    ClassDef(MyTriggerTask, 1)
};
```

```cpp
bool MyTriggerTask::Init()
{
    // Load parameters first, then register output branches
    fOutputArray = fRun -> RegisterBranchA("RawData", "MyChannel", 1000);
    // open external data source ...
    return true;
}

void MyTriggerTask::Run(Long64_t numEvents)
{
    // Read external source and fire events
    while (hasMoreData()) {
        readNextEvent(fOutputArray);   // fill the output branch
        SignalNextEvent();             // LKRun executes all other tasks
    }
}

void MyTriggerTask::SignalNextEvent()
{
    fRun -> ExecuteTask();   // triggers Exec() on all downstream tasks
}
```

Register it the same way as any other task — `LKRun` detects `IsEventTrigger()`
and uses `Run()` instead of `Exec()` to drive the loop:

```cpp
run -> Add(new MyTriggerTask());   // registered as event trigger automatically
run -> Add(new AnalysisTask());
run -> Init();
run -> Run();
```

> **Note:** Only one Event Trigger Task can be active per run.
> When an Event Trigger Task is present, `LKRun` should not be given a LILAK-format input file.

---

## Doxygen Documentation

Every task should be documented with Doxygen comments so that the auto-generated
reference pages are useful to other users.

**Minimum documentation for a task class:**

```cpp
/**
 * @brief One-line summary of what this task does.
 *
 * Longer description: what input branches it reads, what output branches it
 * produces, which detector or data format it targets, and any important
 * assumptions or limitations.
 *
 * **Input branches**
 * - `RawData` (`GETChannel`) — raw GET electronics data
 *
 * **Output branches**
 * - `Hit` (`LKHit`) — reconstructed 3-D hit positions
 *
 * **Parameters**
 * | Parameter key              | Type   | Default | Description              |
 * |----------------------------|--------|---------|--------------------------|
 * | `MyTask/threshold`         | int    | 100     | Minimum pulse height     |
 * | `MyTask/tbRange`           | int[2] | 0, 512  | Time-bucket range to use |
 */
class MyTask : public LKTask
{
    public:
        MyTask();
        virtual ~MyTask() {}

        bool Init();
        void Exec(Option_t *option = "");
        bool EndOfRun();

    private:
        TClonesArray *fInputArray  = nullptr;  ///< RawData branch (GETChannel)
        TClonesArray *fOutputArray = nullptr;  ///< Hit branch (LKHit), owned by this task

        int fThreshold = 100;  ///< Minimum pulse height cut. Set by MyTask/threshold.

    ClassDef(MyTask, 1)
};
```

Key points:
- The class Doxygen block (`/** ... */`) should list **input branches**, **output branches**, and a **parameter table**.
- Every private member should have a `///<` trailing comment describing its role and which parameter controls it.
- Method bodies do not need Doxygen blocks unless the method has non-obvious behaviour — the class block is the primary documentation.

---

## Built-in Tasks

| Class | Function |
|-------|----------|
| `LKGETConversionTask` | GET electronics binary → `GETChannel` branches (Event Trigger) |
| `LKMFMConversionTask` | MFM frame data → `GETChannel` branches (Event Trigger) |
| `LKPulseExtractionTask` | Pulse extraction from `GETChannel` |
| `LKPulseShapeAnalysisTask` | Pulse shape analysis → `LKHit` branch |
| `LKNoiseSubtractionTask` | Noise subtraction |
| `LKHTTrackingTask` | Hough-transform based track finding |
| `LKElectronicsTask` | Electronics simulation |
| `LKEveTask` | EVE 3-D visualisation |
