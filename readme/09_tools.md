\page tools Analysis Tools

# Analysis Tools

LILAK collects a number of reusable, mostly self-contained tools under
`source/tool/`. They are not tied to any particular detector and can be used
from a task, a macro, or interactively. This page surveys the most useful ones.

| Tool | Header | Purpose |
|------|--------|---------|
| `LKMisc` | `LKMisc.h` | string-option parsing, pedestal/FWHM helpers |
| `LKSAM` | `LKSAM.h` | "simple analysis methods": pedestal, FWHM, polar hist |
| `LKODRFitter` | `LKODRFitter.h` | orthogonal-distance line/plane fit |
| `LKHTLineTracker` | `hough_transform/` | Hough-transform line finding |
| `LKChannelAnalyzer` | `pulse_analysis/` | pulse finding + fitting on a waveform |
| `LKPulseAnalyzer` | `pulse_analysis/` | build a reference pulse from data |
| `LKDrawing` / `LKDataViewer` | `drawing/` | grouped drawables + interactive viewer |
| `LKBeamPID` | `beam_pid/` | beam particle identification |
| `LKAttenuationCalculator` | `attenuator/` | gas/material attenuation |
| `LKNoiseAnalyzer` | `LKNoiseAnalyzer.h` | coherent-noise analysis |

Several tools are singletons reached through a static getter
(`LKMisc::GetMisc()`, `LKSAM::GetSAM()`, `LKODRFitter::GetFitter()`).

---

## LKMisc — string and option utilities

`LKMisc` is heavily used by LILAK's own `Draw(Option_t*)` methods to parse the
space-separated option strings ROOT passes around.

```cpp
TString opt = "logy threshold=100 name=raw";

bool    logy = LKMisc::CheckOption(opt, "logy");           // presence
int     thr  = LKMisc::FindOptionInt(opt, "threshold", 0); // value, with default
TString name = LKMisc::FindOptionString(opt, "name", "");

LKMisc::AddOption(opt, "inverted");          // append a flag
LKMisc::AddOption(opt, "scale", 1.5);        // append key=value
LKMisc::RemoveOption(opt, "logy");
```

It also offers numeric helpers (`FWHM`, `EvalPedestal`,
`EvalPedestalSamplingMethod`), array membership tests (`ValueIsInArray`),
trailing-zero trimming (`RemoveTrailing0`), and palette/marker preview
(`DrawColors`, `DrawMarkers`, `DrawFonts`).

---

## LKSAM — simple analysis methods

`LKSAM` bundles common waveform/histogram routines:

```cpp
auto sam = LKSAM::GetSAM();

double fwhm = sam -> FWHM(buffer, length);
double ped  = sam -> EvalPedestalSamplingMethod(buffer, length,
                                                /*sampleLength*/ 50,
                                                /*cvCut*/ 0.2,
                                                /*subtract*/ true);
double cont = sam -> ContinuityIndex(hist);    // 0..1, how gap-free a hist is
```

`MakeTH1Polar(hist, ...)` turns a 0–360° histogram into a polar plot
(returns an `LKDrawing`), and `SmoothCorners(graph)` rounds the corners of a
polygon graph.

The sampling-method pedestal divides the buffer into segments, keeps the most
stable ones (low coefficient of variation), and averages them — robust against
pulses sitting in the baseline region.

---

## LKODRFitter — orthogonal distance regression

Fits a line or a plane by minimising the *perpendicular* distance from the
points (not the vertical residual), by solving the eigenvalue problem of the
scatter matrix. This is the engine behind `LKTracklet::FitLine/FitPlane`
(see \ref container).

```cpp
auto fitter = LKODRFitter::GetFitter();
fitter -> Reset();
fitter -> SetCentroid(meanX, meanY, meanZ);
for (auto h : hits)
    fitter -> AddPoint(h->X(), h->Y(), h->Z(), h->W());

fitter -> FitLine();                       // or FitPlane()
TVector3 dir    = fitter -> GetDirection();
TVector3 normal = fitter -> GetNormal();   // for a plane
double   rms    = fitter -> GetRMSLine();
```

The centroid must be set before adding points. For a plane the smallest
eigenvalue is chosen; for a line the largest.

---

## LKHTLineTracker — Hough-transform tracking

Finds straight-line tracks by voting in a parameter space of
`(radius, theta)` relative to a chosen transform center. Useful for *grouping*
hits into track candidates; the author recommends following up with a
least-squares fit (`FitTrackWithParamPoint`) rather than using the Hough peak
itself as the final fit.

```cpp
auto tracker = new LKHTLineTracker();
tracker -> SetTransformCenter(0, 0);
tracker -> SetImageSpaceRange(120, -150, 150, 120, 0, 500);
tracker -> SetParamSpaceBins(numBinsR, numBinsT);

for (auto h : hits)
    tracker -> AddImagePoint(x, xErr, y, yErr, weight);

tracker -> SetCorrelateBoxBand();          // recommended correlator
tracker -> Transform();
auto paramPoint = tracker -> FindNextMaximumParamPoint();
auto track      = tracker -> FitTrackWithParamPoint(paramPoint);
```

Correlators trade speed for fidelity to the point/parameter errors —
`Point-Band` (fastest) → `Box-Line` → `Box-Band` / `Box-Ribbon` (default).
Weighting functions (`SetWFConst/Linear/Inverse/GivenWeight`) control how each
hit votes; the default `Inverse` weights by `error/(distance+error)`.
The header documents the full vocabulary (image space, parameter space, bands).

---

## Pulse analysis

### LKPulseAnalyzer

Builds a **reference pulse** by averaging clean single-hit waveforms from real
data and writes a pulse-data ROOT file. Run it once per electronics
configuration; the result feeds `LKChannelAnalyzer`.

### LKChannelAnalyzer

Finds and fits pulses on a single waveform buffer. `Analyze()` chains
`FindAndSubtractPedestal()` → `FindPeak()` → `FitPulse()` → `TestPulse()` →
`FitAmplitude()`.

```cpp
auto ana = new LKChannelAnalyzer();
ana -> SetPulse("pulse_data_from_LKPulseAnalyzer.root");  // sets many params
ana -> SetTbRange(0, 512);
ana -> SetThreshold(100);
ana -> SetIterMax(15);
ana -> SetScaleTbStep(0.2);

ana -> Analyze(buffer);
for (int i = 0; i < ana->GetNumHits(); ++i) {
    double tb  = ana -> GetTbHit(i);
    double amp = ana -> GetAmplitude(i);
    double q   = ana -> GetIntegral(i);
    double chi = ana -> GetChi2NDF(i);
}
```

Key knobs: `fThreshold` (peak recognition), `fIterMax` / `fTbStepCut` /
`fScaleTbStep` (fit speed vs. resolution), and the analyzer mode
(`SetSigAtMaximum`, `SetSigAtThreshold`, or full pulse fitting). The header
explains each parameter and provides `DEBUG_CHANA_*` compile switches that
expose diagnostic graphs of the fit iterations.

A detector exposes a configured analyzer via
`LKDetector::GetChannelAnalyzer()` (see \ref detector).

---

## Drawing — LKDrawing, LKDataViewer, LKPainter

The `drawing/` tools standardise how LILAK presents results:

- **`LKDrawing`** — a group of ROOT drawables (hists, graphs, text) drawn
  together with consistent options. Many tools return `LKDrawing*`.
- **`LKDrawingGroup`** — a tree of `LKDrawing`s, savable/loadable as a file.
- **`LKDataViewer`** — an interactive viewer that navigates a
  `LKDrawingGroup` (used as the basis of the web ROOT viewer).
- **`LKPainter`** — low-level canvas/pad layout helpers (`PreDivide`, palette).
- **`LKCut`** — 2-D graphical cuts; **`LKBinning`** — reusable histogram
  binning passed via parameters.

---

## Domain tools

| Class | Description |
|-------|-------------|
| `LKBeamPID`, `LKBeamPIDControl` | beam particle ID from ToF / energy-loss correlations, with systematic-error control |
| `LKAttenuationCalculator` | attenuation through a material/gas |
| `LKNoiseAnalyzer` | coherent-noise extraction across channels |
| `LKListFileParser` | parse list/run files |
| `LKRunTimeMeasure` | scoped timing measurements |
| `LKPadInteractive`, `LKPadInteractiveManager` | mouse interaction on ROOT pads (base for the trackers/analyzers above) |

To add a new tool, generate the skeleton with the class creator in `tool` mode
(see \ref project); a new sub-directory under `source/tool/` is picked up by
the build automatically.
