#include "LKDetectorConstruction.h"

G4VPhysicalVolume* LKDetectorConstruction::Construct()
{
    auto runManager = (LKG4RunManager *) G4RunManager::GetRunManager();
    auto par = runManager -> GetParameterContainer();

    auto dmWorld = par -> InitPar(TVector3(2000,2000,2000), "LKDetectorConstruction/WorldSize ?? # mm");
    dmWorld = dmWorld * mm;

    G4Box* solidWorld = new G4Box ("solidWorld", 0.5*dmWorld.x(), 0.5*dmWorld.y(), 0.5*dmWorld.z());

    G4NistManager *nist = G4NistManager::Instance();
    G4Material *materialVacuum = nist -> FindOrBuildMaterial("G4_Galactic");
    auto logicWorld = new G4LogicalVolume(solidWorld, materialVacuum, "logicWorld", 0, 0, 0);

    G4PVPlacement *physicsWorld = new G4PVPlacement(0, G4ThreeVector(0., 0., 0.), logicWorld, "World", 0, false, 0, true);

    auto visWorld = new G4VisAttributes(G4Colour::Grey());
    visWorld -> SetForceWireframe(true);
    logicWorld -> SetVisAttributes(visWorld);
    
    return physicsWorld;
}
