#include "LKGeometry.h"
#include "LKMisc.h"

ClassImp(LKGeometry)

TGraph *LKGeometry::GetGraph(TVector3 offset, TString option)
{
    auto graph = GetGraph(offset);
    if (graph==nullptr)
        return graph;
    if (LKMisc::CheckOption(option, "fstyle")) graph -> SetFillStyle  (LKMisc::FindOptionInt   (option, "fstyle",1));
    if (LKMisc::CheckOption(option, "fcolor")) graph -> SetFillColor  (LKMisc::FindOptionInt   (option, "fcolor",1));
    if (LKMisc::CheckOption(option, "msize") ) graph -> SetMarkerSize (LKMisc::FindOptionDouble(option, "msize" ,1));
    if (LKMisc::CheckOption(option, "mstyle")) graph -> SetMarkerStyle(LKMisc::FindOptionInt   (option, "mstyle",20));
    if (LKMisc::CheckOption(option, "mcolor")) graph -> SetMarkerColor(LKMisc::FindOptionInt   (option, "mcolor",1));
    if (LKMisc::CheckOption(option, "lwidth")) graph -> SetLineWidth  (LKMisc::FindOptionInt   (option, "lwidth",1));
    if (LKMisc::CheckOption(option, "lstyle")) graph -> SetLineStyle  (LKMisc::FindOptionInt   (option, "lstyle",1));
    if (LKMisc::CheckOption(option, "lcolor")) graph -> SetLineColor  (LKMisc::FindOptionInt   (option, "lcolor",1));
    return graph;
}
