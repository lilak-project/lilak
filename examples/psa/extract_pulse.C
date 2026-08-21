R__ADD_INCLUDE_PATH(/Users/jungwoo/Research/lilak/source/base)
R__ADD_INCLUDE_PATH(/Users/jungwoo/Research/lilak/source/tool)
R__ADD_INCLUDE_PATH(/Users/jungwoo/Research/lilak/source/tool/drawing)
R__ADD_INCLUDE_PATH(/Users/jungwoo/Research/lilak/source/tool/pulse_analysis)
R__ADD_INCLUDE_PATH(/Users/jungwoo/Research/lilak/source/geometry)

#include "LKDrawingGroup.h"
#include "LKChannelSimulator.h"
#include "LKPulseExtractor.h"
#include "LKPulse.h"

#include "draw_channels.C"

void extract_pulse(bool parametersOnly=true)
{
    TString name = "channel_simulator";
    TString path = ".";
    TString fileName = Form("%s/pulse_reference_%s.mac", path.Data(), name.Data());

    auto ext = new LKPulseExtractor(name, path);
    ext -> SetThreshold(400);
    ext -> SetTbRange(0, 512);
    ext -> SetPulseTbCuts(0, 512);
    ext -> SetPulseWidthCuts(1, 120);
    ext -> SetPulseHeightCuts(100, 5000);
    ext -> SetPedestalTBRange(0, 40, 460, 512);
    ext -> SetWritePulseFunctionParametersOnly(parametersOnly);

    auto top = draw_channels(10000, false, false);
    auto num = top -> GetNumDrawings();
    for (auto i=0; i<num; ++i)
    {
        auto drawing = top -> GetDrawing(i);
        auto hist = drawing -> GetMainHist();
        ext -> AddHist(hist, i);
    }

    cout << "Collected pulse channels: " << ext -> GetNumGoodChannels() << endl;
    auto graph = ext -> GetReferencePulse(20, 40);
    if (graph==nullptr || graph->GetN()==0)
    {
        cout << "No final pulse was extracted. Check threshold/height/width cuts." << endl;
        return;
    }
    if (!parametersOnly)
    {
        auto cvs = new TCanvas("cvs_extracted_pulse", "extracted pulse", 900, 600);
        graph -> SetTitle(";time bucket from reference;normalized pulse");
        graph -> Draw("apl");
        cvs -> Modified();
        cvs -> Update();
    }

    auto writtenFileName = ext -> WritePulseParameterFile(20, 40, fileName);
    if (writtenFileName.IsNull())
        return;

    ext -> Draw("x");
    ext -> GetTop() -> Draw("viewer");
    return;
}
