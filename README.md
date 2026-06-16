# LILAK
Low- and Intermediate-energy nucLear experiment Analysis toolKit

LILAK is a ROOT-based C++ framework for data analysis in low and intermediate energy nuclear physics experiments.
It provides a task pipeline architecture where each step of the analysis — data conversion, hit extraction, tracking, particle identification — is implemented as an independent, configurable task.
Detector geometry, electronics readout, and analysis parameters are all controlled through plain-text parameter files, making it straightforward to adapt the framework to different experimental setups such as TPCs and silicon detector arrays.

---

## Requirements

- [ROOT](https://root.cern) 6.x
- CMake 3.5+
- C++11 or later
- *(optional)* Geant4 — for simulation
- *(optional)* NPTool — for additional detector definitions

---

## Download

Clone the LILAK repository from [LILAK Github](https://github.com/lilak-project/lilak):
```sh
git clone git@github.com:lilak-project/lilak.git
cd lilak/
```

Then clone the project repository you want to work with from [LILAK Projects](https://github.com/lilak-project):
```sh
git clone git@github.com:lilak-project/atomx.git   # AToMX experiment
git clone git@github.com:lilak-project/stark.git   # STARK experiment
```

---

## Build

Run `lilak.sh` and follow the instructions:
```sh
./lilak.sh
```

---

## Quick Start

After building, run the example analysis macro:
```sh
root macros/run_lilak.C
```

To open the parameter editor or flow editor:
```sh
python3 macros/lilak_parameter_editor.py
python3 macros/lilak_configure_flow_editor.py
```

---

## Deactivate

Remove or comment out the `Rint.Logon` line in `~/.rootrc`:
```
#Rint.Logon: /path/to/lilak/macros/rootlogon.C
```

---

## Links

* [LILAK main repository](https://github.com/lilak-project/lilak)
* [LILAK repositories](https://github.com/orgs/lilak-project/repositories)
* [LILAK doxygen](https://lilak-project.github.io/lilak_doxygen/)
