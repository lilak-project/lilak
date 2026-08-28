\page container Containers — Data Classes

# Containers — Data Classes

A *container* is the unit of data that LILAK stores in a branch and writes to
the output ROOT file. Every container inherits from `LKContainer`, which itself
inherits from ROOT's `TObject`, so containers can live inside a `TClonesArray`
and be streamed to disk.

Tasks fill containers in `Exec()`; `LKRun` writes them out at the end of each
event. See \ref task and \ref run for how branches are created and connected.

```
TObject
 └─ LKContainer
     ├─ LKWPoint            weighted 3-D point
     │   └─ ( base for hit-like data )
     ├─ LKHit               reconstructed hit (position + charge + ids)
     ├─ LKTracklet          abstract track base
     │   ├─ LKLinearTrack
     │   └─ LKHelixTrack
     ├─ LKChannel           electronics channel base
     │   └─ GETChannel      raw GET waveform
     │       └─ LKPad       waveform + pad geometry
     └─ LKEventHeader       per-event metadata
```

---

## LKContainer — the base

All containers share a small virtual interface used by `LKRun`, the event
display, and the parameter system.

```cpp
class LKContainer : public TObject
{
    virtual void Clear(Option_t *option = "");   // reset to default state
    virtual void Copy (TObject &object) const;   // deep copy
    virtual bool SetValue(TString keyword, TString value);
    virtual bool DrawByDefault();                // true → drawn on the event display
};
```

Every concrete container should override `Clear()` and `Copy()` so that
`TClonesArray::Clear("C")` resets objects correctly between events
(see the branch rules in \ref task).

---

## LKWPoint — weighted point

The simplest spatial container: a 3-D position with a weight `w`
(usually charge).

```cpp
LKWPoint p(x, y, z, w);
p.SetPosition(x, y, z);
p.SetWeight(w);

double x = p.X();          // or p.x()
double w = p.W();
TVector3 v = p.GetPosition();
double xi = p[0];          // index access: 0→x, 1→y, 2→z
```

`GetPosition(LKVector3::Axis ref)` returns an `LKVector3` with a chosen
reference axis, so local `(i, j, k)` coordinates are available
(see \ref geometry).

---

## LKHit — reconstructed hit

`LKHit` is the work-horse container for reconstructed space points. It carries
position, position error, charge/weight, time bucket, and a set of identifiers
linking the hit back to its channel, pad, track, and detector segment.

| Field | Accessor | Meaning |
|-------|----------|---------|
| `fX, fY, fZ` | `GetX()`, `x()`, `GetPosition()` | position [mm] |
| `fDX, fDY, fDZ` | `GetPositionError()` | position error [mm] |
| `fW` | `GetCharge()`, `W()` | charge / weight |
| `fTb` | `GetTb()` | time bucket |
| `fHitID` | `GetHitID()` | hit index |
| `fTrackID` | `GetTrackID()` | owning track id |
| `fChannelID`, `fPadID` | `GetChannelID()`, `GetPadID()` | electronics / pad id |
| `fSection`, `fLayer`, `fRow` | `GetSection()`, … | detector segmentation |
| `fAlpha` | `GetAlpha()` | polar angle (tracking) |

```cpp
auto hit = (LKHit *) fOutputArray -> ConstructedAt(n);
hit -> SetHitID(n);
hit -> SetPosition(x, y, z);
hit -> SetPositionError(dx, dy, dz);
hit -> SetCharge(q);
hit -> SetPadID(padID);
```

### Sorting

`LKHit` is `Sortable`. Choose the sort key, then call
`TClonesArray::Sort()` or `std::sort`:

```cpp
hit -> SetSortByCharge(false);   // largest charge first
hit -> SetSortByZ(true);         // smallest z first
```

A large family of stand-alone comparators is also provided
(`LKHitSortZ`, `LKHitSortR`, `LKHitSortCharge`, `LKHitSortByDistanceTo`, …)
for use with `std::sort`:

```cpp
std::sort(hits.begin(), hits.end(), LKHitSortCharge());
std::sort(hits.begin(), hits.end(), LKHitSortByDistanceTo(vertex));
```

### Sub-hits and track candidates

A hit can aggregate other hits through its internal `LKHitArray`
(`AddHit`, `GetHitArray`), and statistics over that set are available directly:

```cpp
hit -> AddHit(subHit);
TVector3 mean   = hit -> GetMean();
TVector3 stddev = hit -> GetStdDev();
```

During track finding, each hit can hold a list of candidate track ids
(`AddTrackCand`, `GetTrackCand`, `PropagateTrackCand`).

---

## Tracks — LKTracklet and friends

`LKTracklet` is the abstract base for all tracks. It owns a list of hit ids
(`fHitIDArray`) plus a transient `LKHitArray` (`fHitArray`, not written to
disk), and records a fit status.

```cpp
enum LKFitStatus { kNone, kBad, kLine, kPlane, kHelix, kGenfitTrack };
```

Common interface (pure-virtual parts implemented by the concrete tracks):

```cpp
track -> AddHit(hit);
track -> SortHits();
int n = track -> GetNumHits();
LKHit *h = track -> GetHit(i);

TVector3 head = track -> PositionAtHead();
TVector3 tail = track -> PositionAtTail();
double   len  = track -> TrackLength();
TVector3 p    = track -> Momentum(0.5);          // B field in Tesla
TVector3 poca = track -> ExtrapolateTo(point);   // closest approach
```

Two fitting helpers return geometry objects (see \ref geometry):

```cpp
LKGeoLine  line  = track -> FitLine();
LKGeoHelix helix = track -> FitHelix(LKVector3::kZ);
```

### LKHelixTrack

Concrete track for charged particles in a magnetic field. It multiply-inherits
from `LKTracklet` and `LKGeoHelix`, so all helix geometry
(center, radius, dip angle, alpha parameterisation) is available directly, plus
helix-specific extrapolation:

```cpp
helix -> Fit();                                  // ODR helix fit over the hits
double r   = helix -> GetHelixRadius();
double len = helix -> ExtrapolateToAlpha(alpha);
TVector3 q = helix -> InterpolateByRatio(0.5);   // mid-point of the track
TVector3 m = helix -> Map(point);                // straighten helix → line frame
```

`LKLinearTrack` is the straight-line counterpart, used by the Hough-transform
tracker (see \ref tools).

---

## Readout — channels and pads

These containers hold raw electronics data.

### LKChannel / GETChannel

`GETChannel` represents one raw waveform from GET electronics, identified by
`(cobo, asad, aget, chan)`. The samples live in an `LKBufferI`.

```cpp
channel -> SetWaveformY(arrayOf512Ints);
int *wf = channel -> GetWaveformY();
double q = channel -> GetIntegral(pedestal);
TH1D *h  = channel -> GetHist();
```

### LKPad

`LKPad` extends `GETChannel` with pad-plane geometry: position `(i, j)`,
segmentation `(section, layer, row)`, neighbour pads, and pad corners.
It carries both the raw signal and a shaped buffer.

```cpp
pad -> SetSectionLayerRow(section, layer, row);
pad -> SetPosition(i, j);
LKVector3 pos = pad -> GetPosition();
auto neighbors = pad -> GetNeighborPadArray();
pad -> Draw("out:hit");        // raw / shaped / hits
```

Pads are produced and owned by a detector plane — see \ref detector.

---

## LKEventHeader — per-event metadata

A single (non-array) container written once per event with bookkeeping
information: event number, good/bad flag, raw-buffer location, and timestamps.

```cpp
header -> SetEventNumber(n);
header -> SetIsGoodEvent(true);
if (!header -> IsGoodEvent()) return;
```

---

## Writing your own container

Most analysis projects define their own containers (e.g. a PID result, a
calibrated energy). The recommended path is to generate the skeleton with the
class creator (see \ref project), then fill in the fields. A minimal container
looks like:

```cpp
class MyResult : public LKContainer
{
    public:
        MyResult() { Clear(); }
        virtual ~MyResult() {}

        virtual void Clear(Option_t *option = "");
        virtual void Copy (TObject &object) const;
        virtual void Print(Option_t *option = "") const;

        void SetEnergy(double e) { fEnergy = e; }
        double GetEnergy() const { return fEnergy; }

    private:
        double fEnergy = -999;

    ClassDef(MyResult, 1)
};
```

Key rules:

- Initialise every field to a sentinel (`-999`) and reset it in `Clear()`.
- Implement `Copy()` so `TClonesArray` can duplicate objects.
- Bump the `ClassDef` version number whenever you change the member layout,
  so old ROOT files stay readable.
