#include "LKPulse.h"
#include "TFile.h"
#include "TTree.h"
#include "TParameter.h"
#include "TMath.h"
#include "TCanvas.h"
#include "TVirtualPad.h"
#include "TLine.h"
#include "TLatex.h"
#include "TMarker.h"
#include "TAxis.h"
#include "LKParameterContainer.h"
#include "TGButton.h"
#include "TGClient.h"
#include "TGFrame.h"
#include "TGLabel.h"
#include "TGNumberEntry.h"
#include "TGSlider.h"
#include "TRootEmbeddedCanvas.h"
#include <cfloat>
#include <cstring>
#include <iostream>
using namespace std;

ClassImp(LKPulse);

class LKPulseDrawPanel : public TGMainFrame
{
    private:
        enum {
            kPulseDrawBaseline = 1001,
            kPulseDrawPeak,
            kPulseDrawT0,
            kPulseDrawAlpha,
            kPulseDrawTau,
            kPulseDrawTbMin,
            kPulseDrawTbMax,
            kPulseDrawApply
        };

        LKPulse fPulse;
        TRootEmbeddedCanvas *fCanvas = nullptr;
        TGHSlider *fBaselineSlider = nullptr;
        TGHSlider *fPeakSlider = nullptr;
        TGHSlider *fT0Slider = nullptr;
        TGHSlider *fAlphaSlider = nullptr;
        TGHSlider *fTauSlider = nullptr;
        TGHSlider *fTbMinSlider = nullptr;
        TGHSlider *fTbMaxSlider = nullptr;
        TGLabel *fBaselineValue = nullptr;
        TGLabel *fPeakValue = nullptr;
        TGLabel *fT0Value = nullptr;
        TGLabel *fAlphaValue = nullptr;
        TGLabel *fTauValue = nullptr;
        TGLabel *fTbMinValue = nullptr;
        TGLabel *fTbMaxValue = nullptr;

    public:
        LKPulseDrawPanel(const LKPulse *pulse, const TGWindow *parent, UInt_t w, UInt_t h)
        : TGMainFrame(parent, w, h)
        {
            fPulse.SetPulseFunctionParameters(pulse->GetPulseFunctionBaseline(), pulse->GetPulseFunctionPeak(), pulse->GetPulseFunctionT0(), pulse->GetPulseFunctionAlpha(), pulse->GetPulseFunctionTau());
            fPulse.SetPulseFunctionRange(pulse->GetPulseFunctionTbMin(), pulse->GetPulseFunctionTbMax());

            auto main = new TGHorizontalFrame(this);
            AddFrame(main, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

            auto controls = new TGVerticalFrame(main, 240, h);
            main -> AddFrame(controls, new TGLayoutHints(kLHintsLeft | kLHintsExpandY, 8, 8, 8, 8));

            AddSlider(controls, "baseline", pulse->GetPulseFunctionBaseline(), -0.05, 0.05, fBaselineSlider, fBaselineValue, kPulseDrawBaseline);
            AddSlider(controls, "peak", pulse->GetPulseFunctionPeak(), 0.1, 2.0, fPeakSlider, fPeakValue, kPulseDrawPeak);
            AddSlider(controls, "t0 (tb)", pulse->GetPulseFunctionT0(), -20.0, 10.0, fT0Slider, fT0Value, kPulseDrawT0);
            AddSlider(controls, "alpha", pulse->GetPulseFunctionAlpha(), 0.2, 12.0, fAlphaSlider, fAlphaValue, kPulseDrawAlpha);
            AddSlider(controls, "tau (tb)", pulse->GetPulseFunctionTau(), 0.5, 20.0, fTauSlider, fTauValue, kPulseDrawTau);
            AddSlider(controls, "tb min", pulse->GetPulseFunctionTbMin(), -60., 0., fTbMinSlider, fTbMinValue, kPulseDrawTbMin);
            AddSlider(controls, "tb max", pulse->GetPulseFunctionTbMax(), 20., 140., fTbMaxSlider, fTbMaxValue, kPulseDrawTbMax);

            auto apply = new TGTextButton(controls, "Apply", kPulseDrawApply);
            apply -> Associate(this);
            controls -> AddFrame(apply, new TGLayoutHints(kLHintsExpandX, 0, 0, 10, 0));

            fCanvas = new TRootEmbeddedCanvas("pulse_canvas", main, 820, h);
            main -> AddFrame(fCanvas, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 0, 8, 8, 8));

            SetWindowName("LKPulse");
            MapSubwindows();
            Resize(GetDefaultSize());
            MapWindow();
            DrawPulse();
        }

        void AddSlider(TGVerticalFrame *frame, const char *label, double value, double min, double max, TGHSlider *&slider, TGLabel *&valueLabel, int id)
        {
            auto labelRow = new TGHorizontalFrame(frame);
            frame -> AddFrame(labelRow, new TGLayoutHints(kLHintsExpandX, 0, 0, 6, 0));

            auto text = new TGLabel(labelRow, label);
            labelRow -> AddFrame(text, new TGLayoutHints(kLHintsLeft | kLHintsCenterY));

            valueLabel = new TGLabel(labelRow, Form("%.6g", value));
            labelRow -> AddFrame(valueLabel, new TGLayoutHints(kLHintsRight | kLHintsCenterY));

            slider = new TGHSlider(frame, 210, kSlider1 | kScaleBoth, id, kHorizontalFrame);
            slider -> Associate(this);
            slider -> SetRange(0, 1000);
            slider -> SetPosition(ValueToSlider(value, min, max));
            frame -> AddFrame(slider, new TGLayoutHints(kLHintsExpandX, 0, 0, 0, 3));
        }

        Bool_t ProcessMessage(Long_t msg, Long_t parm1, Long_t parm2)
        {
            auto message = GET_MSG(msg);
            auto subMessage = GET_SUBMSG(msg);
            if ((message==kC_HSLIDER && (subMessage==kSL_POS || subMessage==kSL_TRACK || subMessage==kSL_RELEASE)) || (message==kC_COMMAND && subMessage==kCM_BUTTON && parm1==kPulseDrawApply))
                DrawPulse();
            return kTRUE;
        }

        int ValueToSlider(double value, double min, double max)
        {
            if (value<min) value = min;
            if (value>max) value = max;
            return int(1000. * (value - min) / (max - min));
        }

        double SliderToValue(TGHSlider *slider, double min, double max)
        {
            return min + (max - min) * slider -> GetPosition() / 1000.;
        }

        void SetLabel(TGLabel *label, double value)
        {
            label -> SetText(Form("%.6g", value));
        }

        void DrawPulse()
        {
            double baseline = SliderToValue(fBaselineSlider, -0.05, 0.05);
            double peak = SliderToValue(fPeakSlider, 0.1, 2.0);
            double t0 = SliderToValue(fT0Slider, -20.0, 10.0);
            double alpha = SliderToValue(fAlphaSlider, 0.2, 12.0);
            double tau = SliderToValue(fTauSlider, 0.5, 20.0);
            double tbMin = SliderToValue(fTbMinSlider, -60., 0.);
            double tbMax = SliderToValue(fTbMaxSlider, 20., 140.);

            SetLabel(fBaselineValue, baseline);
            SetLabel(fPeakValue, peak);
            SetLabel(fT0Value, t0);
            SetLabel(fAlphaValue, alpha);
            SetLabel(fTauValue, tau);
            SetLabel(fTbMinValue, tbMin);
            SetLabel(fTbMaxValue, tbMax);

            if (alpha<=0 || tau<=0 || tbMax<=tbMin)
                return;

            fPulse.SetPulseFunctionParameters(baseline, peak, t0, alpha, tau);
            fPulse.SetPulseFunctionRange(tbMin, tbMax);

            auto canvas = fCanvas -> GetCanvas();
            canvas -> cd();
            fPulse.Draw("static");
            canvas -> Modified();
            canvas -> Update();
        }
};

LKPulse::LKPulse()
{
    fPulseIsGood = true;
    SetUsePulseFunction(true);
}

LKPulse::LKPulse(const char *fileName)
{
    TString inputName = fileName;
    if (!inputName.EndsWith(".root")) {
        LoadParameterFile(fileName);
        return;
    }

    auto file = new TFile(fileName);
    if (file->IsOpen()==false) {
        e_error << fileName << " is invalid file!" << endl;
        fPulseIsGood = false;
        return;
    }

    auto getBool = [file](const char *name, bool defaultValue) {
        auto par = (TParameter<bool>*) file -> Get(name);
        if (par==nullptr)
            return defaultValue;
        return par -> GetVal();
    };
    auto getInt = [file](const char *name, int defaultValue) {
        auto par = (TParameter<int>*) file -> Get(name);
        if (par==nullptr)
            return defaultValue;
        return par -> GetVal();
    };
    auto getDouble = [file](const char *name, double defaultValue) {
        auto par = (TParameter<double>*) file -> Get(name);
        if (par==nullptr)
            return defaultValue;
        return par -> GetVal();
    };

    fGraphPulse = (TGraphErrors *) file -> Get("pulse");
    fGraphError = (TGraph*) file -> Get("error");
    fGraphError0 = (TGraph*) file -> Get("error0");

    if (fGraphPulse!=nullptr)
        fNumPoints = fGraphPulse -> GetN();

    fNumAnalyzedChannels = getInt("numAnaChannels", fNumAnalyzedChannels);
    fThreshold           = getInt("threshold", fThreshold);
    fHeightMin           = getInt("yMin", fHeightMin);
    fHeightMax           = getInt("yMax", fHeightMax);
    fTbMin               = getInt("xMin", fTbMin);
    fTbMax               = getInt("xMax", fTbMax);
    fFWHM                = getDouble("FWHM", fFWHM);
    fFloorRatio          = getDouble("ratio", fFloorRatio);
    fRefWidth            = getDouble("width", fRefWidth);
    fWidthLeading        = getDouble("widthLeading", fWidthLeading);
    fWidthTrailing       = getDouble("widthTrailing", fWidthTrailing);
    fPulseRefTbMin       = getInt("pulseRefTbMin", fPulseRefTbMin);
    fPulseRefTbMax       = getInt("pulseRefTbMax", fPulseRefTbMax);
    fBackGroundLevel     = getDouble("backGroundLevel", fBackGroundLevel);
    fBackGroundError     = getDouble("backGroundError", fBackGroundError);
    fFluctuationLevel    = getDouble("fluctuationLevel", fFluctuationLevel);

    if (getBool("usePulseFunction", false))
    {
        fUsePulseFunction = true;
        fPulseFunctionBaseline = getDouble("pulseFunctionBaseline", fPulseFunctionBaseline);
        fPulseFunctionPeak = getDouble("pulseFunctionPeak", fPulseFunctionPeak);
        fPulseFunctionT0 = getDouble("pulseFunctionT0", fPulseFunctionT0);
        fPulseFunctionAlpha = getDouble("pulseFunctionAlpha", fPulseFunctionAlpha);
        fPulseFunctionTau = getDouble("pulseFunctionTau", fPulseFunctionTau);
        fPulseFunctionTbMin = getDouble("pulseFunctionTbMin", fPulseFunctionTbMin);
        fPulseFunctionTbMax = getDouble("pulseFunctionTbMax", fPulseFunctionTbMax);
        UpdatePulseFunctionProperties();
    }

    if (getBool("inverted", false))
        fInversion = -1;
    else
        fInversion = 1;

    if (fGraphPulse==nullptr && !fUsePulseFunction) {
        e_error << fileName << " does not contain pulse graph or pulse function parameters!" << endl;
        fPulseIsGood = false;
        file -> Close();
        return;
    }

    fPulseIsGood = true;
    file -> Close();
}

LKPulse::LKPulse(LKParameterContainer *par, TString groupName)
{
    UpdatePulseFunctionParameters(par, groupName);
}

void LKPulse::SetUsePulseFunction(bool value)
{
    fUsePulseFunction = value;
    if (fUsePulseFunction) {
        fPulseIsGood = true;
        UpdatePulseFunctionProperties();
    }
}

void LKPulse::SetPulseFunctionParameters(double baseline, double peak, double t0, double alpha, double tau)
{
    fPulseFunctionBaseline = baseline;
    fPulseFunctionPeak = peak;
    fPulseFunctionT0 = t0;
    fPulseFunctionAlpha = alpha;
    fPulseFunctionTau = tau;
    UpdatePulseFunctionProperties();
}

void LKPulse::SetPulseFunctionRange(double tbMin, double tbMax)
{
    fPulseFunctionTbMin = tbMin;
    fPulseFunctionTbMax = tbMax;
    UpdatePulseFunctionProperties();
}

bool LKPulse::LoadParameterFile(const char *fileName, TString groupName)
{
    auto par = new LKParameterContainer(fileName);
    if (par->IsEmpty())
    {
        e_error << fileName << " does not contain LKPulse parameters!" << endl;
        fPulseIsGood = false;
        delete par;
        return false;
    }

    auto ok = UpdatePulseFunctionParameters(par, groupName);
    delete par;
    return ok;
}

bool LKPulse::UpdatePulseFunctionParameters(LKParameterContainer *par, TString groupName)
{
    if (par==nullptr)
        return false;

    if (!par->CheckPar(groupName+"/UsePulseFunction") && !par->CheckPar(groupName+"/PulseFunctionT0"))
    {
        e_error << "Cannot find " << groupName << " pulse-function parameters." << endl;
        fPulseIsGood = false;
        return false;
    }

    auto getBool = [par](TString name, bool value) {
        if (par->CheckPar(name))
            return par->GetParBool(name);
        return value;
    };
    auto getInt = [par](TString name, int value) {
        if (par->CheckPar(name))
            return par->GetParInt(name);
        return value;
    };
    auto getDouble = [par](TString name, double value) {
        if (par->CheckPar(name))
            return par->GetParDouble(name);
        return value;
    };

    fUsePulseFunction = getBool(groupName+"/UsePulseFunction", true);
    fPulseFunctionBaseline = getDouble(groupName+"/PulseFunctionBaseline", fPulseFunctionBaseline);
    fPulseFunctionPeak = getDouble(groupName+"/PulseFunctionPeak", fPulseFunctionPeak);
    fPulseFunctionT0 = getDouble(groupName+"/PulseFunctionT0", fPulseFunctionT0);
    fPulseFunctionAlpha = getDouble(groupName+"/PulseFunctionAlpha", fPulseFunctionAlpha);
    fPulseFunctionTau = getDouble(groupName+"/PulseFunctionTau", fPulseFunctionTau);
    fPulseFunctionTbMin = getDouble(groupName+"/PulseFunctionTbMin", fPulseFunctionTbMin);
    fPulseFunctionTbMax = getDouble(groupName+"/PulseFunctionTbMax", fPulseFunctionTbMax);
    fPulseFunctionErrorScale = getDouble(groupName+"/PulseFunctionErrorScale", fPulseFunctionErrorScale);

    TString extractedGroupName = "LKPulseExtracted";
    fPulseFunctionBaseline = getDouble(extractedGroupName+"/PulseFunctionBaseline", fPulseFunctionBaseline);
    fNumAnalyzedChannels = getInt(extractedGroupName+"/NumAnalyzedChannels", getInt(groupName+"/NumAnalyzedChannels", fNumAnalyzedChannels));
    fThreshold = getInt(extractedGroupName+"/Threshold", getInt(groupName+"/Threshold", fThreshold));
    fHeightMin = getInt(extractedGroupName+"/HeightMin", getInt(groupName+"/HeightMin", fHeightMin));
    fHeightMax = getInt(extractedGroupName+"/HeightMax", getInt(groupName+"/HeightMax", fHeightMax));
    fTbMin = getInt(extractedGroupName+"/TbMin", getInt(groupName+"/TbMin", fTbMin));
    fTbMax = getInt(extractedGroupName+"/TbMax", getInt(groupName+"/TbMax", fTbMax));
    fFWHM = getDouble(extractedGroupName+"/FWHM", getDouble(groupName+"/FWHM", fFWHM));
    fFloorRatio = getDouble(extractedGroupName+"/FloorRatio", getDouble(groupName+"/FloorRatio", fFloorRatio));
    fRefWidth = getDouble(extractedGroupName+"/Width", getDouble(groupName+"/Width", fRefWidth));
    fWidthLeading = getDouble(extractedGroupName+"/WidthLeading", getDouble(groupName+"/WidthLeading", fWidthLeading));
    fWidthTrailing = getDouble(extractedGroupName+"/WidthTrailing", getDouble(groupName+"/WidthTrailing", fWidthTrailing));
    fPulseRefTbMin = getInt(extractedGroupName+"/PulseRefTbMin", getInt(groupName+"/PulseRefTbMin", fPulseRefTbMin));
    fPulseRefTbMax = getInt(extractedGroupName+"/PulseRefTbMax", getInt(groupName+"/PulseRefTbMax", fPulseRefTbMax));
    fBackGroundLevel = getDouble(extractedGroupName+"/BackGroundLevel", getDouble(groupName+"/BackGroundLevel", fBackGroundLevel));
    fBackGroundError = getDouble(extractedGroupName+"/BackGroundError", getDouble(groupName+"/BackGroundError", fBackGroundError));
    fFluctuationLevel = getDouble(extractedGroupName+"/FluctuationLevel", getDouble(groupName+"/FluctuationLevel", fFluctuationLevel));

    if (getBool(groupName+"/Inverted", false))
        fInversion = -1;
    else
        fInversion = 1;

    if (fUsePulseFunction)
        UpdatePulseFunctionProperties();

    fPulseIsGood = fUsePulseFunction;
    return fPulseIsGood;
}

TF1* LKPulse::GetPulseFunction(TString name, bool subtractBaseline)
{
    auto function = new TF1(name, subtractBaseline ? PulseFunctionBaselineSubtracted : PulseFunction, fPulseFunctionTbMin, fPulseFunctionTbMax, 5);
    function -> SetParNames("baseline", "peak", "t0", "alpha", "tau");
    function -> SetParameters(fPulseFunctionBaseline, fPulseFunctionPeak, fPulseFunctionT0, fPulseFunctionAlpha, fPulseFunctionTau);
    if (fFixPulseFunctionAlpha) function -> FixParameter(3, fPulseFunctionAlpha);
    if (fFixPulseFunctionTau) function -> FixParameter(4, fPulseFunctionTau);
    return function;
}

bool LKPulse::FitPulseFunction(TGraphErrors *graph, Option_t *option)
{
    if (graph==nullptr)
        return false;

    auto function = GetPulseFunction("pulse_function_fit", true);
    auto status = graph -> Fit(function, option);
    fPulseFunctionBaseline = function -> GetParameter(0);
    fPulseFunctionPeak = function -> GetParameter(1);
    fPulseFunctionT0 = function -> GetParameter(2);
    fPulseFunctionAlpha = function -> GetParameter(3);
    fPulseFunctionTau = function -> GetParameter(4);
    fUsePulseFunction = true;
    UpdatePulseFunctionProperties();
    return (status==0);
}

double LKPulse::PulseFunction(double *x, double *p)
{
    double u = x[0] - p[2];
    if (u<=0)
        return p[0];

    double alpha = p[3];
    double tau = p[4];
    double peakU = alpha * tau;
    return p[0] + p[1] * TMath::Power(u/peakU, alpha) * TMath::Exp(alpha - u/tau);
}

double LKPulse::PulseFunctionBaselineSubtracted(double *x, double *p)
{
    double value = PulseFunction(x,p) - p[0];
    if (value<0)
        return 0;
    return value;
}

double LKPulse::EvalPulseFunction(double x, double amplitude) const
{
    if (x<fPulseFunctionTbMin || x>fPulseFunctionTbMax)
        return 0;

    double u = x - fPulseFunctionT0;
    if (u<=0)
        return 0;

    double peakU = fPulseFunctionAlpha * fPulseFunctionTau;
    double value = fPulseFunctionPeak * TMath::Power(u/peakU, fPulseFunctionAlpha) * TMath::Exp(fPulseFunctionAlpha - u/fPulseFunctionTau);
    if (value<0)
        value = 0;
    return fInversion * amplitude * value;
}

double LKPulse::ErrorPulseFunction(double x, double amplitude) const
{
    auto error = TMath::Abs(amplitude) * fPulseFunctionErrorScale;
    if (error<=0)
        error = fPulseFunctionErrorScale;
    return error;
}

void LKPulse::UpdatePulseFunctionProperties()
{
    double peakTb = fPulseFunctionT0 + fPulseFunctionAlpha * fPulseFunctionTau;
    fFloorRatio = 0.05;
    fPulseRefTbMin = int(TMath::Floor(fPulseFunctionTbMin));
    fPulseRefTbMax = int(TMath::Ceil(fPulseFunctionTbMax));
    fNumPoints = fPulseRefTbMax - fPulseRefTbMin + 1;
    if (fNumPoints<1)
        fNumPoints = 1;

    auto findCrossing = [&](double ratio, bool leading)
    {
        double target = fPulseFunctionPeak * ratio;
        double bestX = leading ? fPulseFunctionTbMin : peakTb;
        double bestDiff = DBL_MAX;
        double x1 = leading ? fPulseFunctionTbMin : peakTb;
        double x2 = leading ? peakTb : fPulseFunctionTbMax;
        for (double x=x1; x<=x2; x+=0.02) {
            double y = TMath::Abs(EvalPulseFunction(x, 1.));
            double diff = TMath::Abs(y-target);
            if (diff<bestDiff) {
                bestDiff = diff;
                bestX = x;
            }
        }
        return bestX;
    };

    double tbFloorLeading = findCrossing(fFloorRatio, true);
    double tbFloorTrailing = findCrossing(fFloorRatio, false);
    double tbHalfLeading = findCrossing(0.5, true);
    double tbHalfTrailing = findCrossing(0.5, false);

    fWidthLeading = TMath::Abs(peakTb - tbFloorLeading);
    fWidthTrailing = TMath::Abs(tbFloorTrailing - peakTb);
    fRefWidth = fWidthLeading + fWidthTrailing;
    fFWHM = TMath::Abs(tbHalfTrailing - tbHalfLeading);
}

bool LKPulse::Init()
{
    return true;
}

void LKPulse::Clear(Option_t *option)
{
}

void LKPulse::Print(Option_t *option) const
{
    e_info << "fUsePulseFunction   : " << fUsePulseFunction << endl;
    if (fUsePulseFunction)
    {
        e_info << "function baseline   : " << fPulseFunctionBaseline << endl;
        e_info << "function peak       : " << fPulseFunctionPeak << endl;
        e_info << "function t0         : " << fPulseFunctionT0 << " tb" << endl;
        e_info << "function alpha      : " << fPulseFunctionAlpha << endl;
        e_info << "function tau        : " << fPulseFunctionTau << " tb" << endl;
        e_info << "function range      : " << fPulseFunctionTbMin << " - " << fPulseFunctionTbMax << " tb" << endl;
    }
    e_info << "fNumAnalyzedChannels : " << fNumAnalyzedChannels << endl;
    e_info << "fInversion           : " << fInversion           << endl;
    e_info << "fThreshold           : " << fThreshold           << endl;
    e_info << "fHeightMin           : " << fHeightMin           << endl;
    e_info << "fHeightMax           : " << fHeightMax           << endl;
    e_info << "fTbMin               : " << fTbMin               << endl;
    e_info << "fTbMax               : " << fTbMax               << endl;
    e_info << "fFWHM                : " << fFWHM                << endl;
    e_info << "fFloorRatio          : " << fFloorRatio          << endl;
    e_info << "fRefWidth            : " << fRefWidth            << endl;
    e_info << "fWidthLeading        : " << fWidthLeading        << endl;
    e_info << "fWidthTrailing       : " << fWidthTrailing       << endl;
    e_info << "fBackGroundLevel     : " << fBackGroundLevel     << endl;
    e_info << "fBackGroundError     : " << fBackGroundError     << endl;
}

void LKPulse::Draw(Option_t *option)
{
    TString drawOption = option;
    if (fUsePulseFunction && !drawOption.Contains("static", TString::kIgnoreCase))
    {
        new LKPulseDrawPanel(this, gClient->GetRoot(), 1100, 700);
        return;
    }

    if (gPad==nullptr)
        new TCanvas("cvs_pulse", "LKPulse", 1100, 700);

    if (!fUsePulseFunction)
    {
        auto graph = GetPulseGraph(0, 1, 0);
        graph -> SetTitle(";time from hit tb - tb0 + 0.5 (tb);normalized pulse");
        graph -> Draw(option&&strlen(option)>0 ? option : "apl");
        return;
    }

    double baseline = fPulseFunctionBaseline;
    double peak = fPulseFunctionPeak;
    double t0 = fPulseFunctionT0;
    double alpha = fPulseFunctionAlpha;
    double tau = fPulseFunctionTau;
    double tbMin = fPulseFunctionTbMin;
    double tbMax = fPulseFunctionTbMax;
    if (alpha<=0 || tau<=0 || tbMax<=tbMin)
        return;

    double tPeak = t0 + alpha * tau;
    double yPeak = baseline + peak;

    gPad -> Clear();

    auto f = GetPulseFunction(Form("pulse_shape_%p",this), false);
    f -> SetNpx(1000);
    f -> SetLineColor(kBlue+1);
    f -> SetLineWidth(3);
    f -> SetTitle(";time from hit tb - tb0 + 0.5 (tb);normalized pulse");
    f -> Draw("");
    f -> GetYaxis() -> SetRangeUser(TMath::Min(-0.08, baseline - 0.1), yPeak * 1.22);

    auto lBaseline = new TLine(tbMin, baseline, tbMax, baseline);
    lBaseline -> SetLineStyle(2);
    lBaseline -> SetLineColor(kGray+2);
    lBaseline -> Draw();

    auto lT0 = new TLine(t0, baseline - 0.06, t0, baseline);
    lT0 -> SetLineStyle(2);
    lT0 -> SetLineColor(kRed+1);
    lT0 -> Draw();

    auto lPeakX = new TLine(tPeak, baseline - 0.06, tPeak, yPeak);
    lPeakX -> SetLineStyle(2);
    lPeakX -> SetLineColor(kGreen+2);
    lPeakX -> Draw();

    auto lPeakY = new TLine(tbMin, yPeak, tPeak, yPeak);
    lPeakY -> SetLineStyle(2);
    lPeakY -> SetLineColor(kGreen+2);
    lPeakY -> Draw();

    auto marker = new TMarker(tPeak, yPeak, 20);
    marker -> SetMarkerColor(kGreen+2);
    marker -> SetMarkerSize(1.2);
    marker -> Draw();

    auto text = new TLatex();
    text -> SetTextSize(0.032);
    text -> DrawLatex(tbMin + 0.04*(tbMax-tbMin), yPeak*1.13, "f(t)=baseline+peak #times (u/(#alpha#tau))^{#alpha} exp(#alpha-u/#tau),  u=t-t_{0}");
    text -> DrawLatex(t0 + 0.02*(tbMax-tbMin), baseline + 0.07*yPeak, Form("t_{0}=%.3f tb", t0));
    text -> DrawLatex(tPeak + 0.02*(tbMax-tbMin), yPeak*0.92, Form("t_{peak}=%.3f tb", tPeak));
    text -> DrawLatex(tPeak + 0.02*(tbMax-tbMin), yPeak*0.82, Form("height=%.3f", peak));
    text -> DrawLatex(tbMin + 0.04*(tbMax-tbMin), baseline + 0.04*yPeak, Form("baseline=%.6f", baseline));
    text -> DrawLatex(tbMin + 0.62*(tbMax-tbMin), yPeak*0.38, Form("#alpha=%.3f, #tau=%.3f tb", alpha, tau));

    gPad -> Modified();
    gPad -> Update();
}

double LKPulse::EvalTb(double tb, double tb0, double amplitude)
{
    if (fUsePulseFunction)
        return EvalPulseFunction(tb-tb0+0.5, amplitude);
    return fInversion * (amplitude * fGraphPulse -> Eval(tb-tb0+0.5));
}

double LKPulse::ErrorTb(double tb, double tb0, double amplitude)
{
    if (fUsePulseFunction)
        return ErrorPulseFunction(tb-tb0+0.5, amplitude);
    return amplitude * fGraphError -> Eval(tb-tb0+0.5);
}

double LKPulse::Error0Tb(double tb, double tb0, double amplitude)
{
    if (fUsePulseFunction)
        return ErrorPulseFunction(tb-tb0+0.5, amplitude);
    return amplitude * fGraphError0 -> Eval(tb-tb0+0.5);
}

double LKPulse::Eval(double tb, double tb0, double amplitude)
{
    if (fUsePulseFunction)
        return EvalPulseFunction(tb-tb0, amplitude);
    return fInversion * (amplitude * fGraphPulse -> Eval(tb-tb0));
}

double LKPulse::Error(double tb, double tb0, double amplitude)
{
    if (fUsePulseFunction)
        return ErrorPulseFunction(tb-tb0, amplitude);
    return amplitude * fGraphError -> Eval(tb-tb0);
}

double LKPulse::Error0(double tb, double tb0, double amplitude)
{
    if (fUsePulseFunction)
        return ErrorPulseFunction(tb-tb0, amplitude);
    return amplitude * fGraphError0 -> Eval(tb-tb0);
}

TGraphErrors *LKPulse::GetPulseGraph(double tb0, double amplitude, double pedestal)
{
    auto graphPulse = new TGraphErrors();
    return FillPulseGraph(graphPulse, tb0, amplitude, pedestal);
}

TGraphErrors* LKPulse::FillPulseGraph(TGraphErrors* graphPulse, double tb0, double amplitude, double pedestal)
{
    graphPulse -> Set(0);
    if (fUsePulseFunction)
    {
        int iPoint = 0;
        for (double xValue=fPulseFunctionTbMin; xValue<=fPulseFunctionTbMax; xValue+=1.)
        {
            auto yValue = EvalPulseFunction(xValue, amplitude) + pedestal;
            auto yError = ErrorPulseFunction(xValue, amplitude);
            graphPulse -> SetPoint(iPoint,xValue+tb0,yValue);
            graphPulse -> SetPointError(iPoint,0,yError);
            iPoint++;
        }
        graphPulse -> SetLineColor(kRed);
        graphPulse -> SetMarkerColor(kRed);
        return graphPulse;
    }

    for (auto iPoint=0; iPoint<fNumPoints; ++iPoint)
    {
        auto xValue = fGraphPulse -> GetPointX(iPoint);
        auto yValue = fInversion * fGraphPulse -> GetPointY(iPoint);
        auto yError = fGraphPulse -> GetErrorY(iPoint);
        graphPulse -> SetPoint(iPoint,xValue+tb0,yValue*amplitude+pedestal);
        graphPulse -> SetPointError(iPoint,0,yError*amplitude);
    }
    graphPulse -> SetLineColor(kRed);
    graphPulse -> SetMarkerColor(kRed);
    return graphPulse;
}
