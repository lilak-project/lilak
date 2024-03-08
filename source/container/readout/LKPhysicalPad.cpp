#include "TLine.h"
#include "TText.h"
#include "TF1.h"
#include "LKLogger.h"
#include "LKPhysicalPad.h"
#include <iomanip>
#include <iostream>
using namespace std;

ClassImp(LKPhysicalPad)

void LKPhysicalPad::Clear(Option_t *option)
{
    LKChannel::Clear(option);

    //fPlaneID = 0;
    //fCoboID = -1;
    //fAsadID = -1;
    //fAgetID = -1;
    //fChanID = -1;
    //fSection = -1
    //fLayer = -1
    //fRow = -1
    fDataIndex = 1;
    fTime = -1;
    fEnergy = -1;
    fPedestal = -1;

    //fPosition;
    //fPadCorners;
    //fNeighborPadArray;

    fHitArray.clear();
    //fSortValue;
}

void LKPhysicalPad::Print(Option_t *option) const
{
    e_info << "[LKPhysicalPad]" << endl;
    e_info << "- CAAC: " << fCoboID << " " << fAsadID << " " << fAgetID << " " << fChanID << endl;
    e_info << "- SLR : " << fSection << " " << fLayer << " " << fRow << endl;
    e_info << "- TEP : " << fTime << " " << fEnergy << " " << fPedestal << endl;
    e_info << "- Pos.: " << fPosition.I() << " " << fPosition.J() << endl;
    e_info << "- #Hit: " << fHitArray.size() << endl;
}

int LKPhysicalPad::Compare(const TObject *obj) const
{
    auto pad2 = (LKPhysicalPad *) obj;
    if      (fSortValue < pad2 -> GetSortValue()) return +1; //Sort this pad earlier than pad2;
    else if (fSortValue > pad2 -> GetSortValue()) return -1; //Sort this pad latter  than pad;
    return 0; // no change
}
