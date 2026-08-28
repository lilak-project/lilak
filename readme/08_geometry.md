\page geometry Geometry Utilities

# Geometry Utilities

LILAK provides a small geometry library used throughout tracking and
reconstruction. The classes fall into two groups:

- **`LKVector3`** — a `TVector3` that also understands a local `(i, j, k)`
  frame, so code can be written independently of which global axis is "up".
- **`LKGeometry` shapes** — lines, planes, circles, helices, boxes and
  spheres, each with closest-point / distance / drawing helpers.

```
TVector3
 └─ LKVector3                global (x,y,z) + local (i,j,k)

LKGeometry                   abstract shape base (carries an RMS)
 ├─ LKGeoLine
 ├─ LKGeoPlane / LKGeoPlaneWithCenter
 ├─ LKGeoCircle
 ├─ LKGeoHelix
 ├─ LKGeoBox / LKGeo2DBox / LKGeoBoxStack
 ├─ LKGeoSphere
 └─ LKGeoPolygon
```

---

## LKVector3 — axes made explicit

A reconstruction frame is often defined by a *reference axis* (the drift
direction, the beam axis, …). `LKVector3` pins a global axis as the local
`k` axis and derives `i`, `j` by the right-hand rule, so the same code works
whether the drift is along `+z`, `-y`, etc.

```cpp
enum Axis { kNon, kX, kMX, kY, kMY, kZ, kMZ, kI, kMI, kJ, kMJ, kK, kMK };
```

```cpp
LKVector3 v(x, y, z, LKVector3::kY);   // global xyz, local frame referenced to y
double i = v.I();                       // local coordinates
double j = v.J();
double k = v.K();                       // == y here
v.SetReferenceAxis(LKVector3::kZ);      // re-reference
```

Helpers convert names ↔ enum and classify axes:

```cpp
LKVector3::Axis a = LKVector3::GetAxis("-z");   // → kMZ
TString name      = LKVector3::AxisName(a);     // → "-z"
bool isLocal      = LKVector3::IsLocalAxis(a);
bool isPositive   = LKVector3::IsPositive(a);
```

`At(axis)`, `SetAt(value, axis)`, and `AddAt(value, axis)` read/write a single
component by axis, and arithmetic operators (`+ - *`) preserve the reference
axis. Most containers expose `GetPosition(LKVector3::Axis ref)` to obtain their
position already in the right frame (see \ref container).

---

## LKGeometry — the shape base

Every shape derives from `LKGeometry`, which stores a fit RMS and provides a
`GetGraph()` hook for drawing:

```cpp
shape.SetRMS(rms);
double rms = shape.GetRMS();
TGraph *g  = shape.GetGraph(offset, "option");
```

---

## LKGeoLine — straight line

A line through two points (or a point and a direction):

```cpp
LKGeoLine line(p1, p2);
TVector3 dir = line.Direction();                  // 1 → 2
double   len = line.Length();

TVector3 c   = line.ClosestPointOnLine(point);    // POCA
double   d   = line.DistanceToLine(point);        // perpendicular distance
double   t   = line.Length(point);                // arc length to POCA

TVector3 q   = line.GetPointAtZ(z);               // intersect a plane of const z
TVector3 x   = line.GetCrossingPoint(plane);      // line ∩ plane
```

Drawing helpers return `TGraph` / `TGraph2D` / `TArrow` for any axis pair
(`GetGraphXYZ`, `GetArrowZX`, …), and `SetRange(box)` clips the line to a box.

A straight-line **track** fit returns this object via `LKTracklet::FitLine()`
(see \ref container).

---

## LKGeoPlane / LKGeoCircle

`LKGeoPlane` (and `LKGeoPlaneWithCenter`) represent a flat plane and provide
crossing-point and distance queries; `LKTracklet::FitPlane()` returns one.

`LKGeoCircle` is a circle in a 2-D plane:

```cpp
LKGeoCircle circle(x0, y0, r);
TVector3 c = circle.ClosestPointToCircle(x, y);
TVector3 p = circle.PointAtPhi(phi);
double   a = circle.Phi(x, y);                    // angle of a point
TGraph  *g = circle.GetGraph(offset, 100, 0, 360);
```

---

## LKGeoHelix — helical trajectory

The helix is the geometry of a charged track in a uniform magnetic field. It is
parameterised about a reference axis `a` with center `(i, j)`, radius `r`, and a
linear relation between the axial coordinate `k` and the turning angle `alpha`:

```
k = s · alpha + k0          ( t = alpha at tail, h = alpha at head )
```

```cpp
LKGeoHelix helix(i, j, r, s, k0, alphaTail, alphaHead, LKVector3::kZ);

TVector3 pos = helix.PositionAtAlpha(alpha);      // point on helix
TVector3 dir = helix.Direction(alpha);            // tangent
double   len = helix.TravelLengthAtAlpha(alpha);  // arc length
double   a   = helix.AlphaAtTravelLength(len);    // inverse

double   dip = helix.DipAngle();                  // = atan(-s/r)
double   per = helix.LengthInPeriod();            // arc length of one turn
int      hel = helix.Helicity();                  // ±1
```

`LKHelixTrack` (see \ref container) *is* an `LKGeoHelix`, so a fitted track
exposes all of the above directly, plus extrapolation to planes
(`ExtrapolateToI`, `ExtrapolateToJ`) and helix↔line mapping (`Map`,
`HelicoidMap`).

---

## Other shapes

| Class | Use |
|-------|-----|
| `LKGeoBox`, `LKGeo2DBox` | axis-aligned boxes; range clipping, containment |
| `LKGeoBoxStack` | a stack of boxes |
| `LKGeoSphere` | sphere |
| `LKGeoPolygon` | polygon (e.g. pad outlines) |
| `LKGeoRotated` | rotated wrapper around another shape |

All of them share the `LKGeometry` interface (`GetGraph`, `GetCenter`,
`SetRMS`) so they can be drawn and fit uniformly.

---

## Where geometry is used

- **Tracking** — `LKTracklet::FitLine/FitPlane/FitHelix` return these shapes;
  the ODR fitter (`LKODRFitter`, see \ref tools) does the underlying math.
- **Detector planes** — local `(i, j, k)` coordinates via `LKVector3`
  (see \ref detector).
- **Event display** — every shape provides `TGraph`/`TArrow` drawables.
