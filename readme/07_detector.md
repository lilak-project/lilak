\page detector Detectors

# Detectors

A *detector* describes the physical hardware of an experiment: its geometry,
its readout segmentation, and the transforms between electronics channels and
positions in space. Tasks query detectors to turn raw channel data into
3-D hits and to draw event displays.

There are three cooperating classes:

```
LKDetectorSystem            container of all detectors (singleton, via GetDS())
 └─ LKDetector              one physical detector (geometry + planes)
     └─ LKDetectorPlane     one readout plane (pad/strip lookup, drift, hits)
```

`LKDetector` and `LKDetectorPlane` both inherit from `LKGear`, so they have
direct access to the parameter container (`fPar`) — all geometry constants are
read from the `.mac`/`.par` file (see \ref parameter).

---

## Registering detectors with the run

Detectors are added to `LKRun` before `Init()`:

```cpp
run -> AddDetector(new STARK());        // a full detector
run -> AddDetectorPlane(new MyPlane()); // or a bare plane
```

After `Init()`, any task can look one up by name:

```cpp
auto plane = (MyPlane*) fRun -> FindDetectorPlane("MyPlane");
```

`LKRun` forwards these to the global `LKDetectorSystem`, whose `Init()` calls
`Init()` on each detector and plane.

---

## LKDetector

`LKDetector` owns the TGeo geometry and one or more detector planes. Subclass
it and implement the two protected builders:

```cpp
class MyDetector : public LKDetector
{
    public:
        MyDetector();
        virtual ~MyDetector() {}

    protected:
        bool BuildGeometry();        // build the TGeoVolume tree
        bool BuildDetectorPlane();   // create & AddPlane() the readout planes

    ClassDef(MyDetector, 1)
};
```

| Method | Purpose |
|--------|---------|
| `BuildGeometry()` | Construct the TGeo volume hierarchy (returns `true` on success) |
| `BuildDetectorPlane()` | Create plane objects and register with `AddPlane()` |
| `CreateGeoTop(name)` | Helper that returns the top `TGeoVolume` |
| `IsInBoundary(x,y,z)` | Whether a global point is inside the active volume |
| `GetDetectorPlane(idx)` | Access a registered plane |
| `GetChannelAnalyzer(id)` | Pulse analyzer shared with the planes (see \ref tools) |

```cpp
void MyDetector::MyDetector() : LKDetector("MyDetector","") {}

bool MyDetector::BuildDetectorPlane()
{
    AddPlane(new MyPlane());
    return true;
}
```

---

## LKDetectorPlane

This is where most of the experiment-specific logic lives. A plane manages an
array of channels/pads and provides the maps that tracking and event display
rely on. The base class defines the geometry of the plane via three axes plus a
drift axis, all read from parameters in `Init()`:

| Member | Set from parameter | Meaning |
|--------|--------------------|---------|
| `fAxis1`, `fAxis2` | required | the two axes lying in the plane |
| `fAxis3` | derived | `fAxis1 × fAxis2`, normal to the plane |
| `fAxisDrift` | required | electron-drift direction |
| `fTbToLength` | required | time-bucket → length conversion |
| `fPosition` | required | global position of the plane centre |

### Methods to implement

The base class marks many methods *"implementation recommended"*. The important
ones for a working plane are:

```cpp
class MyPlane : public LKDetectorPlane
{
    public:
        MyPlane();
        bool Init();                                   // build pads, read params

        int FindPadID(double i, double j);             // position → pad id
        int FindPadID(int section, int layer, int row);
        bool IsInBoundary(double i, double j);         // inside active area?

        TVector3 GlobalToLocalAxis(TVector3 g);        // global ↔ local
        TVector3 LocalToGlobalAxis(TVector3 l);

        TH2 *GetHist(Option_t *option = "-1");         // event-display histogram
        TCanvas *GetCanvas(Option_t *option = "");

    ClassDef(MyPlane, 1)
};
```

In `Init()` you must:

1. Read `fAxis1/2`, `fAxisDrift`, `fTbToLength`, `fPosition` from `fPar`.
2. Create the pads (`LKPad`), set their id / position / section-layer-row /
   neighbours, and register each one with `AddPad()` (or `AddChannel()`).

```cpp
bool MyPlane::Init()
{
    auto pad = new LKPad();
    pad -> SetSectionLayerRow(section, layer, row);
    pad -> SetPosition(i, j);
    AddPad(pad);
    // ... repeat for all pads ...
    return true;
}
```

### Looking up pads

Once built, pads are reachable many ways:

```cpp
LKPad *p = plane -> GetPad(padID);
LKPad *p = plane -> GetPad(i, j);                  // by position
LKPad *p = plane -> GetPad(section, layer, row);
LKPad *p = plane -> GetPad(cobo, asad, aget, chan);
int    n = plane -> GetNumPads();
```

### Drift and reconstruction

The plane converts between drifted-electron positions and pad+time-bucket:

```cpp
plane -> DriftElectron(posGlobal, posFinal, driftLength);          // simulation
plane -> DriftElectronBack(pad, tb, posReco, driftLength);         // reconstruction
```

The default `DriftElectronBack` uses `post = fTbToLength * tb + fPosition`
along the drift axis, so for many TPC-like planes you only need to set those
parameters correctly.

### Hit map (tracking support)

For tracking, the plane can hand out hits one neighbourhood at a time:

```cpp
plane -> ResetHitMap();
LKHit *seed = plane -> PullOutNextFreeHit();
plane -> PullOutNeighborHits(x, y, range, &neighbors);
```

---

## Self-checks

`LKDetectorPlane` ships three debugging tools that print a report — run them
once after writing a new plane to catch mapping mistakes:

```cpp
plane -> PadPositionChecker();   // FindPadID(pad.pos) == pad.id ?
plane -> PadNeighborChecker();   // each pad has neighbours registered ?
plane -> PadMapChecker();        // section/layer/row and cobo/asad/aget/chan maps
```

---

## Built-in planes

LILAK provides reusable plane and detector implementations you can subclass or
use directly:

| Class | Description |
|-------|-------------|
| `LKPolygonPadPlane` | Polygon-tiled pad plane (TPC) |
| `LKMicromegas` | Micromegas detector |
| `LKEvePlane` | Plane wrapper for the EVE 3-D display |

For per-experiment detectors, generate the skeleton with the class creator
(`detector` / `detector_plane` / `pad_plane` modes) and fill in the
methods above — see \ref project.
