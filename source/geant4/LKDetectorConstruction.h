#ifndef LKDETECTORCONSTRUCTION_HH
#define LKDETECTORCONSTRUCTION_HH

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
#include "G4VUserDetectorConstruction.hh"
#include "globals.hh"

class LKDetectorConstruction : public G4VUserDetectorConstruction
{
    public:
        LKDetectorConstruction(){}
        virtual ~LKDetectorConstruction(){}
        G4VPhysicalVolume* Construct();
};

#endif
