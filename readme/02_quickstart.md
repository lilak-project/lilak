\page quickstart Quick Start

# Quick Start

## Setup

After building LILAK, source the setup script to activate the `lilak` command:

```sh
source lilak.sh
```

You can add this line to your `~/.bashrc` or `~/.zshrc` so it is loaded automatically.

---

## The `lilak` command

The `lilak` command is the main entry point for all LILAK operations.

```sh
lilak                  # print help
lilak config.mac       # run analysis with a parameter file
lilak par config.mac   # open parameter file editor
lilak configure        # open flow editor
lilak build            # rebuild LILAK
```

Full command reference:

| Command | Description |
|---------|-------------|
| `lilak [file]` | Run analysis with the given parameter file |
| `lilak collect_par [file]` | Collect parameters and rewrite the parameter file |
| `lilak par [file]` | Open the parameter editor in a browser |
| `lilak configure [file]` | Open the flow editor in a browser |
| `lilak make_run [root]` | Create a run parameter file from a ROOT file |
| `lilak read [root]` | Generate a ROOT macro that reads a LILAK output file |
| `lilak draw [file]` | Draw histograms from a parameter file or ROOT file |
| `lilak make_meta [class]` | Generate meta parameter files for a class |
| `lilak build` | Build LILAK |
| `lilak update` | Pull updates for LILAK and all project repositories |
| `lilak find [name]` | Find a file and navigate to its directory |
| `lilak doc [class]` | Print the Doxygen documentation link for a class |
| `lilak new` | Run the project/class creator script |

---

## Running an analysis

### 1. Create a parameter file

All run settings live in a plain-text `.mac` parameter file.
The minimum needed to point the run at an input file:

```
# config.mac
LKRun/InputFile    /data/run0042.root
LKRun/OutputFile   /data/run0042_ana.root
```

To generate a parameter file from an existing ROOT file automatically:

```sh
lilak make_run /data/run0042.root
```

### 2. Open the flow editor to set up tasks

```sh
lilak configure config.mac
```

This opens a browser-based editor where you can attach tasks and parameter
containers to the run, and see the full analysis pipeline.

### 3. Edit parameters

```sh
lilak par config.mac
```

Opens the parameter file editor in the browser so you can enable, disable,
and change parameter values without editing the file by hand.

### 4. Run

```sh
lilak config.mac
```

This executes `root macros/run_lilak.C("config.mac")` and processes all events.

---

## Writing your first task

### 1. Generate boilerplate

```sh
lilak new
```

The interactive script asks for a class name and type (task, container, detector, etc.)
and writes the header and source files into the correct project directory.

### 2. Implement `Init()` and `Exec()`

**MyTask.h**
```cpp
#include "LKTask.h"

/**
 * @brief One-line description of what MyTask does.
 *
 * **Input branches**
 * - `RawData` (`GETChannel`)
 *
 * **Output branches**
 * - `Hit` (`LKHit`)
 *
 * **Parameters**
 * | Key                | Type | Default | Description          |
 * |--------------------|------|---------|----------------------|
 * | `MyTask/threshold` | int  | 100     | Minimum pulse height |
 */
class MyTask : public LKTask
{
    public:
        MyTask();
        virtual ~MyTask() {}

        bool Init();
        void Exec(Option_t *option = "");

    private:
        TClonesArray *fInputArray  = nullptr;  ///< RawData branch (GETChannel)
        TClonesArray *fOutputArray = nullptr;  ///< Hit branch (LKHit), owned by this task

        int fThreshold = 100;  ///< Minimum pulse height. Set by MyTask/threshold.

    ClassDef(MyTask, 1)
};
```

**MyTask.cpp**
```cpp
#include "LKRun.h"
#include "LKLogger.h"
#include "MyTask.h"

ClassImp(MyTask)

MyTask::MyTask() : LKTask("MyTask", "MyTask") {}

bool MyTask::Init()
{
    // 1. Load parameters FIRST — before any line that can return false.
    //    This ensures parameter collection works even without an input file.
    fPar -> UpdatePar(fThreshold, "MyTask/threshold");

    // 2. Register output branches owned by this task.
    fOutputArray = fRun -> RegisterBranchA("Hit", "LKHit", 100);

    // 3. Connect input branches created by upstream tasks.
    fInputArray = fRun -> GetBranchA("RawData");
    if (fInputArray == nullptr) {
        lk_error << "Branch [RawData] not found!" << endl;
        return false;
    }

    return true;
}

void MyTask::Exec(Option_t *option)
{
    // Always clear branches owned by this task at the start of each event.
    fOutputArray -> Clear("C");

    int n = fInputArray -> GetEntriesFast();
    for (auto i = 0; i < n; ++i) {
        auto ch = (GETChannel *) fInputArray -> At(i);
        // ... analyse ch, fill fOutputArray ...
    }

    lk_info << fOutputArray -> GetEntriesFast() << " hits found" << endl;
}
```

### 3. Collect parameters

Once the task is compiled, collect its parameters into the config file:

```sh
lilak collect_par config.mac
```

This runs the task pipeline without an input file, records every parameter
the tasks tried to read (including defaults), and rewrites `config.mac` with
them included. You can then adjust the values in the editor:

```sh
lilak par config.mac
```

### 4. Run

```sh
lilak config.mac
```

---

## Reading the output

After the run, generate a read macro from the output ROOT file:

```sh
lilak read /data/run0042_ana.root
```

This writes a ROOT macro that loops over the output tree and prints branch contents,
which you can use as a starting point for further analysis.

To draw histograms directly:

```sh
lilak draw /data/run0042_ana.root
```

See \ref task for the full `LKTask` documentation.
