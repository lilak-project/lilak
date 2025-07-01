#ifndef LKNPDETECTORCONSTRUCTION_HH
#define LKNPDETECTORCONSTRUCTION_HH

#include "LKLogger.h"
#include "LKG4RunManager.h"
#include "LKParameterContainer.h"

#include "G4Box.hh"
#include "G4Colour.hh"
#include "G4UserLimits.hh"
#include "G4RunManager.hh"
#include "G4UnionSolid.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4VisAttributes.hh"
#include "G4LogicalVolume.hh"
#include "G4SystemOfUnits.hh"
#include "DetectorConstruction.hh"
#include "globals.hh"

class LKNPDetectorConstruction : public DetectorConstruction
{
    public:
        LKNPDetectorConstruction(){}
        virtual ~LKNPDetectorConstruction(){}
        G4VPhysicalVolume* Construct();
};

#endif
