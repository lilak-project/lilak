#include <cmath>
#include "LKChannelAnalyzer.h"
#include "TLine.h"
#include "TLegend.h"
#include "LKMisc.h"

ClassImp(LKChannelAnalyzer);

LKChannelAnalyzer::LKChannelAnalyzer()
{
    fNumPedestalSamples = floor(fTbMax/fNumTbSample);
    fNumPedestalSamplesM1 = fNumPedestalSamples - 1;
    fNumTbSampleLast = fNumTbSample + fTbMax - fNumPedestalSamples*fNumTbSample;
}

bool LKChannelAnalyzer::Init()
{
    return true;
}

void LKChannelAnalyzer::SetPulse(const char* fileName)
{
    fPulseFileName = fileName;
    fPulse = new LKPulse(fPulseFileName);
    if (fPulse->IsGood()==false) {
        e_error << "Pulse is not initialized correctly. Using default mode kSigAtMaximumMode" << endl;
        return;
    }

    fAnalyzerMode = kPulseFittingMode;

    if (fDataIsInverted)
        fPulse -> SetInverted();

    auto numPulsePoints = fPulse -> GetNDF();
    fDataIsInverted = fPulse -> GetInverted();
    fFWHM           = fPulse -> GetFWHM();
    fFloorRatio     = fPulse -> GetFloorRatio();
    fWidth          = fPulse -> GetWidth();
    fWidthLeading   = fPulse -> GetLeadingWidth();
    fWidthTrailing  = fPulse -> GetTrailingWidth();
    fPulseRefTbMin  = fPulse -> GetPulseRefTbMin();
    fPulseRefTbMax  = fPulse -> GetPulseRefTbMax();
    fNDFPulse       = fWidthLeading + fWidthTrailing;
    //fNDFPulse       = fPulse -> GetNDF();

    fNDFFit = fWidthLeading + fFWHM/4;
    //fNumTbsCorrection = int(numPulsePoints);
    fTbStepIfFoundHit = fNDFFit;
    fTbStepIfSaturated = int(fWidth*1.2);
    fTbSeparationWidth = fNDFFit;
    if (fTbStartCut<0)
        fTbStartCut = fTbMax - fNDFFit;
    fNumTbAcendingCut = int(fWidthLeading*2/3);

    Init();
}

void LKChannelAnalyzer::Clear(Option_t *option)
{
    fPedestal = 0;
    fDynamicRange = fDynamicRangeOriginal;
    fNumHits = 0;
    fFitParameterArray.clear();

#ifdef DEBUG_CHANA_FINDPEAK
    if (dMGraphFP==nullptr) {
        lk_debug << "Creating multigraph for FindPeak" << endl;
        dMGraphFP = new TMultiGraph();
        dGraphFPArray = new TClonesArray("TGraph",10);
    }
    auto listGraph = dMGraphFP -> GetListOfGraphs();
    if (listGraph!=nullptr) {
        TIter next(listGraph);
        TGraph *graph;
        while ((graph = (TGraph*) next()))
            graph -> Set(0);
        listGraph -> Clear();
    }
    fNumGraphFP = 0;
    if (dGraphA==nullptr) {
        dGraphA = new TGraph();
        dGraphA -> SetLineColor(kBlue);
    }
    else {
        dGraphA -> Clear();
        dGraphA -> Set(0);
    }
#endif
}

void LKChannelAnalyzer::Print(Option_t *option) const
{
    if (fAnalyzerMode==kPulseFittingMode) {
        e_info << "LKChannelAnalyzer (pulse fitting mode)" << endl;
        e_info << "== General" << endl;
        e_info << "   fTbMax             = " << fTbMax               << endl;
        e_info << "   fTbStart           = " << fTbStart             << endl;
        e_info << "   fTbStartCut        = " << fTbStartCut          << endl;
        e_info << "   fNumTbAcendingCut  = " << fNumTbAcendingCut    << endl;
        e_info << "   fDynamicRange      = " << fDynamicRange        << endl;
        e_info << "   fDataIsInverted    = " << fDataIsInverted      << endl;
        e_info << "== Pulse information" << endl;
        e_info << "   fFWHM              = " << fFWHM                << endl;
        e_info << "   fFloorRatio        = " << fFloorRatio          << endl;
        e_info << "   fWidth             = " << fWidth               << endl;
        e_info << "   fWidthLeading      = " << fWidthLeading        << endl;
        e_info << "   fWidthTrailing     = " << fWidthTrailing       << endl;
        e_info << "   fPulseRefTbMin     = " << fPulseRefTbMin       << endl;
        e_info << "   fPulseRefTbMax     = " << fPulseRefTbMax       << endl;
        e_info << "== Peak Finding" << endl;
        e_info << "   fThreshold         = " << fThreshold           << endl;
        e_info << "   fThresholdOneStep  = " << fThresholdOneStep    << endl;
        e_info << "   fTbStepIfFoundHit  = " << fTbStepIfFoundHit    << endl;
        e_info << "   fTbStepIfSaturated = " << fTbStepIfSaturated   << endl;
        e_info << "   fTbSeparationWidth = " << fTbSeparationWidth   << endl;
        //e_info << "   fNumTbsCorrection =" << fNumTbsCorrection    << endl;
        e_info << "== Pulse Fitting" << endl;
        e_info << "   fNDFFit            = " << fNDFFit              << endl;
        e_info << "   fIterMax           = " << fIterMax             << endl;
        e_info << "   fTbStepCut         = " << fTbStepCut           << endl;
        e_info << "   fScaleTbStep       = " << fScaleTbStep         << endl;
    }
    else if (fAnalyzerMode==kSigAtMaximumMode)
    {
        e_info << "LKChannelAnalyzer (signal at maximum)" << endl;
        e_info << "   fThreshold         = " << fThreshold           << endl;
        e_info << "   fTbMax             = " << fTbMax               << endl;
        e_info << "   fTbStart           = " << fTbStart             << endl;
        e_info << "   fTbStartCut        = " << fTbStartCut          << endl;
        e_info << "   fNumTbAcendingCut  = " << fNumTbAcendingCut    << endl;
        e_info << "   fDynamicRange      = " << fDynamicRange        << endl;
        e_info << "   fDataIsInverted    = " << fDataIsInverted      << endl;
    }
    else if (fAnalyzerMode==kSigAtThresholdMode)
    {
        e_info << "LKChannelAnalyzer (signal at threshold)" << endl;
        e_info << "   fThreshold         = " << fThreshold           << endl;
        e_info << "   fTbMax             = " << fTbMax               << endl;
        e_info << "   fTbStart           = " << fTbStart             << endl;
        e_info << "   fTbStartCut        = " << fTbStartCut          << endl;
        e_info << "   fNumTbAcendingCut  = " << fNumTbAcendingCut    << endl;
        e_info << "   fDynamicRange      = " << fDynamicRange        << endl;
        e_info << "   fDataIsInverted    = " << fDataIsInverted      << endl;
        e_info << "   fTbSeparationWidth = " << fTbSeparationWidth   << endl;
    }
    e_info << "== Number of found hits: " << fNumHits << endl;
    if (fNumHits>0)
        for (auto iHit=0; iHit<fNumHits; ++iHit) {
            auto tbHit = GetTbHit(iHit);
            auto amplitude = GetAmplitude(iHit);
            e_info << "   Hit-" << iHit << ": tb=" << tbHit << " amplitude=" << amplitude << endl;
        }
}

TH1D* LKChannelAnalyzer::NewHistBuffer()
{
    if      (fAnalyzerMode==kPulseFittingMode)   fHistBuffer = new TH1D(Form("hbuffer_PulseAit_%d",fNumHists++),";tb",fTbMax,0,fTbMax);
    else if (fAnalyzerMode==kSigAtMaximumMode)   fHistBuffer = new TH1D(Form("hbuffer_SigAtMax_%d",fNumHists++),";tb",fTbMax,0,fTbMax);
    else if (fAnalyzerMode==kSigAtThresholdMode) fHistBuffer = new TH1D(Form("hbuffer_SigAtThr_%d",fNumHists++),";tb",fTbMax,0,fTbMax);
    for (auto tb=0; tb<fTbMax; ++tb)
        fHistBuffer -> SetBinContent(tb+1,fBufferOrigin[tb]);
    fHistBuffer -> SetStats(0);
    return fHistBuffer;
}

void LKChannelAnalyzer::Draw(Option_t *option)
{
    TLegend* legend = nullptr;

    if (fNumHits>0 && GetTbHit(0)>fTbMax/2.)
        legend = new TLegend(0.17, 0.70, 0.40, 0.90);
    else
        legend = new TLegend(0.72, 0.70, 0.95, 0.90);

    if (TString(option).Index("new")>=0||fHistBuffer==nullptr) {
        if      (fAnalyzerMode==kPulseFittingMode)   fHistBuffer = new TH1D(Form("hbuffer_PulseAit_%d",fNumHists++),";tb",fTbMax,0,fTbMax);
        else if (fAnalyzerMode==kSigAtMaximumMode)   fHistBuffer = new TH1D(Form("hbuffer_SigAtMax_%d",fNumHists++),";tb",fTbMax,0,fTbMax);
        else if (fAnalyzerMode==kSigAtThresholdMode) fHistBuffer = new TH1D(Form("hbuffer_SigAtThr_%d",fNumHists++),";tb",fTbMax,0,fTbMax);
        else
            return;
    }
    if (TString(option).Index("new")>=0||fHistBufferIntegral==nullptr) {
        if      (fAnalyzerMode==kPulseFittingMode)   fHistBufferIntegral = new TH1D(Form("hbufferIntegral_PulseAit_%d",fNumHists++),";tb",fTbMax,0,fTbMax);
        else if (fAnalyzerMode==kSigAtMaximumMode)   fHistBufferIntegral = new TH1D(Form("hbufferIntegral_SigAtMax_%d",fNumHists++),";tb",fTbMax,0,fTbMax);
        else if (fAnalyzerMode==kSigAtThresholdMode) fHistBufferIntegral = new TH1D(Form("hbufferIntegral_SigAtThr_%d",fNumHists++),";tb",fTbMax,0,fTbMax);
        else
            return;
    }
    fHistBufferIntegral -> Clear();
    fHistBufferIntegral -> SetStats(0);
    fHistBufferIntegral -> SetFillColor(29);
    for (auto tb : fIntegralTbArray)
        fHistBufferIntegral -> SetBinContent(tb+1,fBufferOrigin[tb]);

    for (auto tb=0; tb<fTbMax; ++tb)
        fHistBuffer -> SetBinContent(tb+1,fBufferOrigin[tb]);
    fHistBuffer -> SetStats(0);

    fHistBufferIntegral -> SetMaximum(fHistBuffer->GetMaximum()*1.15);
    fHistBufferIntegral -> SetMinimum(fHistBuffer->GetMinimum());
    //fHistBufferIntegral -> Draw();
    //fHistBuffer -> Draw("same");
    fHistBuffer -> Draw();
    legend -> AddEntry(fHistBuffer,"data","f");
    legend -> AddEntry(fHistBufferIntegral,"integral","f");

    auto linePedestal = new TLine(0,fPedestal,fTbMax,fPedestal);
    //linePedestal -> SetLineColor(kYellow);
    linePedestal -> SetLineColor(kOrange-2);
    linePedestal -> SetLineWidth(4);
    linePedestal -> Draw("samel");
    legend -> AddEntry(linePedestal,"pedestal","l");

    for (auto iSample=0; iSample<fNumPedestalSamples; ++iSample) {
        auto x1 = fNumTbSample*iSample;
        auto x2 = fNumTbSample*(iSample+1);
        auto y = fPedestalSample[iSample];
        if (iSample==fNumPedestalSamplesM1)
            x2 = x2 - fNumTbSample + fNumTbSampleLast;
        auto line = new TLine(x1,y,x2,y);
        line -> SetLineStyle(2);
        line -> SetLineColor(kGreen+1);
        line -> SetLineWidth(2);
        //line -> SetLineColor(kBlack);
        if (fUsedSample[iSample])
            line -> SetLineStyle(1);
        line -> Draw("samel");
        if (iSample==0)
            legend -> AddEntry(line,"PD-sample","l");
    }

    fHistBuffer -> Draw("same");

#ifdef DEBUG_CHANA_FINDPEAK
    if (dGraphA->GetN()>0) {
        legend -> AddEntry(dGraphA,"ascending","l");
        dGraphA -> Draw("samel");
    }

    auto listGraph = dMGraphFP -> GetListOfGraphs();
    if (listGraph!=nullptr && listGraph->GetEntries()>0) {
        legend -> AddEntry((TGraph*) listGraph->At(0),"find peak","l");
        dMGraphFP -> Draw();
    }
#endif

    for (auto iHit=0; iHit<fNumHits; ++iHit) {
        auto tbHit = GetTbHit(iHit);
        auto amplitude = GetAmplitude(iHit);
        auto graphHit = GetPulseGraph(tbHit,amplitude,fPedestal);
        if (graphHit -> GetN()>1) {
            graphHit -> Draw("samelx");
            legend -> AddEntry(graphHit,"hit","l");
        }
    }

    legend -> Draw("same");
}

TGraphErrors* LKChannelAnalyzer::FillPulseGraph(TGraphErrors* graph, double tbHit, double amplitude, double pedestal)
{
    if      (fAnalyzerMode==kPulseFittingMode) return FillGraphPulseFitting(graph,tbHit,amplitude,pedestal);
    else if (fAnalyzerMode==kSigAtMaximumMode) return FillGraphSigAtMaximum(graph,tbHit,amplitude,pedestal);
    else if (fAnalyzerMode==kSigAtThresholdMode) return FillGraphSigAtThreshold(graph,tbHit,amplitude,pedestal);
    return (TGraphErrors*) nullptr;
}

TGraphErrors* LKChannelAnalyzer::GetPulseGraph(double tbHit, double amplitude, double pedestal)
{
    if      (fAnalyzerMode==kPulseFittingMode) return GetGraphPulseFitting(tbHit,amplitude,pedestal);
    else if (fAnalyzerMode==kSigAtMaximumMode) return GetGraphSigAtMaximum(tbHit,amplitude,pedestal);
    else if (fAnalyzerMode==kSigAtThresholdMode) return GetGraphSigAtThreshold(tbHit,amplitude,pedestal);
    return (TGraphErrors*) nullptr;
}

void LKChannelAnalyzer::Analyze(TH1D* hist, bool inverted)
{
    double buffer[fTbMax];
    double *bufferh = hist -> GetArray();
    for (auto i=0; i<fTbMax; ++i)
        buffer[i] = bufferh[i+1];
    Analyze(buffer, inverted);
}

void LKChannelAnalyzer::Analyze(int* data, bool inverted)
{
    double buffer[fTbMax];
    for (auto tb=0; tb<fTbMax; ++tb)
        buffer[tb] = (double)data[tb];
    Analyze(buffer, inverted);
}

void LKChannelAnalyzer::Analyze(double* data, bool inverted)
{
    Clear();

    if (fDataIsInverted || inverted) {
        for (auto tb=0; tb<fTbMax; ++tb)
            data[tb] = fDynamicRangeOriginal - data[tb];
    }

    memcpy(&fBufferOrigin, data, sizeof(double)*fTbMax);
    if (!fOmitPedestalSubtraction) FindAndSubtractPedestal(data);
    memcpy(&fBuffer, data, sizeof(double)*fTbMax);

    fIntegralTbArray.clear();
    if      (fAnalyzerMode==kPulseFittingMode) AnalyzePulseFitting();
    else if (fAnalyzerMode==kSigAtMaximumMode) AnalyzeSigAtMaximum();
    else if (fAnalyzerMode==kSigAtThresholdMode) AnalyzeSigAtThreshold();

    fNumHits = fFitParameterArray.size();
}

double LKChannelAnalyzer::CollectTbAboveThresholdAndIntegrate(int tbHit)
{
    int tbLast = -1;
    if (fIntegralTbArray.size())
        tbLast = fIntegralTbArray.back();

    double integral = 0.;
    for (auto tb=tbHit; tb>=0; --tb)
    {
        if (fBuffer[tb]>=fThreshold) {
            integral += fBuffer[tb];
            if (tb>tbLast) fIntegralTbArray.push_back(tb);
        }
        else break;
    }
    for (auto tb=tbHit+1; tb<fTbMax; ++tb)
    {
        if (fBuffer[tb]>=fThreshold) {
            integral += fBuffer[tb];
            if (tb>tbLast) fIntegralTbArray.push_back(tb);
        }
        else break;
    }

    return integral;
}

void LKChannelAnalyzer::AnalyzeSigAtMaximum()
{
    int tbHit = 0;
    double amplitude = -DBL_MAX;
    for (auto tb=0; tb<fTbMax; ++tb) {
        if (amplitude<fBuffer[tb]) {
            tbHit = tb;
            amplitude = fBuffer[tb];
        }
    }
    if (amplitude >= fThreshold)
    {
        //XXX
        double xMin, xMax;
        double width = LKMisc::FWHM(fBuffer,fTbMax,tbHit,amplitude,fPedestal,xMin,xMax);
        double integral = CollectTbAboveThresholdAndIntegrate(tbHit);
        fFitParameterArray.push_back(LKPulseFitParameter(tbHit,width,integral,amplitude,1,1));
    }
}

void LKChannelAnalyzer::AnalyzeSigAtThreshold()
{
    int tbAtThreshold = -1;
    int tMax = 0;
    double xMin, xMax, width, integral;
    double amplitude = -DBL_MAX;
    for (auto tb=0; tb<fTbMax; ++tb)
    {
        auto value = fBuffer[tb];
        if (tbAtThreshold<0 && value>fThreshold)
            tbAtThreshold = tb;

        if (tbAtThreshold>=0)
        {
            if (value<fThreshold)
            {
                if (fFitParameterArray.size()==0)
                {
                    width = LKMisc::FWHM(fBuffer,fTbMax,tMax,amplitude,fPedestal,xMin,xMax);
                    integral = CollectTbAboveThresholdAndIntegrate(tbAtThreshold);
                    fFitParameterArray.push_back(LKPulseFitParameter(tbAtThreshold,width,integral,amplitude,1,1));
                    tMax = 0;
                    amplitude = -DBL_MAX;
                    tbAtThreshold = -1;
                }
                else if (tbAtThreshold-fFitParameterArray.back().fTbHit>fTbSeparationWidth)
                {
                    width = LKMisc::FWHM(fBuffer,fTbMax,tMax,amplitude,fPedestal,xMin,xMax);
                    integral = CollectTbAboveThresholdAndIntegrate(tbAtThreshold);
                    fFitParameterArray.push_back(LKPulseFitParameter(tbAtThreshold,width,integral,amplitude,1,1));
                    tMax = 0;
                    amplitude = -DBL_MAX;
                    tbAtThreshold = -1;
                }
                else {
                    tMax = 0;
                    amplitude = -DBL_MAX;
                    tbAtThreshold = -1;
                }
            }
            else if (amplitude<value)
            {
                tMax = tb;
                amplitude = value;
            }
        }
    }
}

void LKChannelAnalyzer::AnalyzePulseFitting()
{
    // Found peak information
    int tbPointer = fTbStart; // start of tb for analysis = 0
    int tbStartOfPulse;

    // Fitted hit information
    double tbHit;
    double amplitude;
    double chi2NDF;
    int ndf = fNDFFit;
    double xMin, xMax, width, integral;

    // Previous hit information
    double tbHitPrev = fTbStart;
    double amplitudePrev = 0;

    for (auto it : {0,1})
    {
        while (FindPeak(fBuffer, tbPointer, tbStartOfPulse))
        {
#ifdef DEBUG_CHANA_ANALYZE
            lk_debug << tbPointer << " " << tbStartOfPulse << endl;
#endif
            if (tbStartOfPulse > fTbStartCut-1)
                break;

            bool isSaturated = false;
            if (FitPulse(fBuffer, tbStartOfPulse, tbPointer, tbHit, amplitude, chi2NDF, ndf, isSaturated) == false)
                continue;

            // Pulse is found!

            if (TestPulse(fBuffer, tbHitPrev, amplitudePrev, tbHit, amplitude))
            {
                width = LKMisc::FWHM(fBuffer,fTbMax,tbHit+fWidthLeading,amplitude,fPedestal,xMin,xMax);
                integral = CollectTbAboveThresholdAndIntegrate(tbHit+fWidthLeading);
                fFitParameterArray.push_back(LKPulseFitParameter(tbHit,width,integral,amplitude,ndf,chi2NDF));
                SubtractPulse(fBuffer,tbHit,amplitude);
#ifdef DEBUG_CHANA_ANALYZE_NHIT
                if (fFitParameterArray.size()>=DEBUG_CHANA_ANALYZE_NHIT)
                    break;
#endif
                tbHitPrev = tbHit;
                amplitudePrev = amplitude;
                if (isSaturated)
                    tbPointer = int(tbHit) + fTbStepIfSaturated;
                else
                    tbPointer = int(tbHit) + fTbStepIfFoundHit;
            }
        }
    }
}

double LKChannelAnalyzer::FindAndSubtractPedestal(double *buffer)
{
    for (auto i=0; i<fNumPedestalSamples; ++i) {
        fUsedSample[i] = false;
        fPedestalSample[i] = 0.;
        fStddevSample[i] = 0.;
    }

    int tbGlobal = 0;
    for (auto iSample=0; iSample<fNumPedestalSamplesM1; ++iSample) {
        for (int iTb=0; iTb<fNumTbSample; iTb++) {
            fPedestalSample[iSample] += buffer[tbGlobal];
            fStddevSample[iSample] += buffer[tbGlobal]*buffer[tbGlobal];
            tbGlobal++;
        }
        fPedestalSample[iSample] = fPedestalSample[iSample] / fNumTbSample;
        fStddevSample[iSample] = fStddevSample[iSample] / fNumTbSample;
        fStddevSample[iSample] = sqrt(fStddevSample[iSample] - fPedestalSample[iSample]*fPedestalSample[iSample]);
    }
    for (int iTb=0; iTb<fNumTbSampleLast; iTb++) {
        fPedestalSample[fNumPedestalSamplesM1] = fPedestalSample[fNumPedestalSamplesM1] + buffer[tbGlobal];
        tbGlobal++;
    }
    fPedestalSample[fNumPedestalSamplesM1] = fPedestalSample[fNumPedestalSamplesM1] / fNumTbSampleLast;
#ifdef DEBUG_CHANA_FINDPED
    for (auto iSample=0; iSample<fNumPedestalSamples; ++iSample)
        lk_debug << "i-" << iSample << " " << fPedestalSample[iSample] << ">0?, " << fStddevSample[iSample] << " " << fStddevSample[iSample]/fPedestalSample[iSample] << "<?" << fCVCut << endl;
#endif

    int countBelowCut = 0;
    for (auto iSample=0; iSample<fNumPedestalSamples; ++iSample) {
        if (fPedestalSample[iSample]>0&&fStddevSample[iSample]/fPedestalSample[iSample]<fCVCut)
            countBelowCut++;
    }
#ifdef DEBUG_CHANA_FINDPED
        lk_debug << "countBelowCut = " << countBelowCut << endl;
#endif

    double pedestalDiffMin = DBL_MAX;
    double pedestalMeanRef = 0;
    int idx1 = 0;
    int idx2 = 0;
    if (countBelowCut>=2) {
        for (auto iSample=0; iSample<fNumPedestalSamples; ++iSample) {
            if (fPedestalSample[iSample]<=0) continue;
            if (fStddevSample[iSample]/fPedestalSample[iSample]>=fCVCut) continue;
            for (auto jSample=0; jSample<fNumPedestalSamples; ++jSample) {
                if (iSample>=jSample) continue;
                if (fPedestalSample[jSample]<=0) continue;
                if (fStddevSample[jSample]/fPedestalSample[jSample]>=fCVCut) continue;
                double diff = abs(fPedestalSample[iSample] - fPedestalSample[jSample]);
#ifdef DEBUG_CHANA_FINDPED
                lk_debug << iSample << "|" << jSample << ") " << fStddevSample[jSample] << " " << fPedestalSample[jSample] << endl;
                lk_debug << iSample << "|" << jSample << ") " << diff << " <? " << pedestalDiffMin << endl;
#endif
                if (diff<pedestalDiffMin) {
                    pedestalDiffMin = diff;
                    pedestalMeanRef = 0.5 * (fPedestalSample[iSample] + fPedestalSample[jSample]);
                    idx1 = iSample;
                    idx2 = jSample;
                }
            }
        }
    }
    else {
        for (auto iSample=0; iSample<fNumPedestalSamples; ++iSample) {
            for (auto jSample=0; jSample<fNumPedestalSamples; ++jSample) {
                if (iSample>=jSample) continue;
                double diff = abs(fPedestalSample[iSample] - fPedestalSample[jSample]);
                if (diff<pedestalDiffMin) {
                    pedestalDiffMin = diff;
                    pedestalMeanRef = 0.5 * (fPedestalSample[iSample] + fPedestalSample[jSample]);
                    idx1 = iSample;
                    idx2 = jSample;
                }
            }
        }
    }

    double pedestalFinal = 0;
    int countNumPedestalTb = 0;

    double pedestalErrorRefSample = 0.1 * pedestalMeanRef;
    pedestalErrorRefSample = sqrt(pedestalErrorRefSample*pedestalErrorRefSample + pedestalDiffMin*pedestalDiffMin);
    //if (pedestalErrorRefSample>fPedestalErrorRefSampleCut)
    //    pedestalErrorRefSample = fPedestalErrorRefSampleCut;

#ifdef DEBUG_CHANA_FINDPED
    lk_debug << "diff min : " << pedestalDiffMin << endl;
    lk_debug << "ref-diff : " << pedestalMeanRef << endl;
    //lk_debug << "ref-error: " << pedestalErrorRef << endl;
    lk_debug << "ref-error-part: " << pedestalErrorRefSample << endl;
#endif

    tbGlobal = 0;
    for (auto iSample=0; iSample<fNumPedestalSamplesM1; ++iSample)
    {
        double diffSample = abs(pedestalMeanRef - fPedestalSample[iSample]);
#ifdef DEBUG_CHANA_FINDPED
        lk_debug << "i-" << iSample << " " << diffSample << "(" << pedestalMeanRef << " - " << fPedestalSample[iSample]<< ")" << " <? " << pedestalErrorRefSample << endl;
#endif
        if (diffSample<pedestalErrorRefSample)
        {
            fUsedSample[iSample] = true;
            countNumPedestalTb += fNumTbSample;
            for (int iTb=0; iTb<fNumTbSample; iTb++)
            {
                pedestalFinal += buffer[tbGlobal];
                tbGlobal++;
            }
#ifdef DEBUG_CHANA_FINDPED
            lk_debug << "1) diffSample < pedestalErrorRefSample : i-" << iSample << " diff=" << diffSample << " pdFinal=" << pedestalFinal << " countNumPedestalTb=" << countNumPedestalTb << endl;
#endif
        }
        else {
            tbGlobal += fNumTbSample;
#ifdef DEBUG_CHANA_FINDPED
            lk_debug << "2) diffSample > pedestalErrorRefSample : i-" << iSample << " diff=" << diffSample << " pdFinal=" << pedestalFinal << " countNumPedestalTb=" << countNumPedestalTb << endl;
#endif
        }
    }
    double diffSample = abs(pedestalMeanRef - fPedestalSample[fNumPedestalSamplesM1]);
    if (diffSample<pedestalErrorRefSample)
    {
        fUsedSample[fNumPedestalSamplesM1] = true;
        countNumPedestalTb += fNumTbSampleLast;
        for (int iTb=0; iTb<fNumTbSampleLast; iTb++) {
            pedestalFinal += buffer[tbGlobal];
            tbGlobal++;
        }
#ifdef DEBUG_CHANA_FINDPED
        lk_debug << fNumPedestalSamplesM1 << " diff=" << diffSample << " " << pedestalFinal/countNumPedestalTb << " " << countNumPedestalTb << endl;
#endif
    }

#ifdef DEBUG_CHANA_FINDPED
    lk_debug << pedestalFinal << " " <<  countNumPedestalTb << endl;;
#endif
    if (pedestalFinal!=0||countNumPedestalTb!=0)
        pedestalFinal = pedestalFinal / countNumPedestalTb;
    else
        pedestalFinal = 4096;

#ifdef DEBUG_CHANA_FINDPED
    lk_debug << pedestalFinal << endl;
#endif

    for (auto iTb=0; iTb<fTbMax; ++iTb)
        buffer[iTb] = buffer[iTb] - pedestalFinal;

    fDynamicRange = fDynamicRange - pedestalFinal;

    fPedestal = pedestalFinal;

    return pedestalFinal;
}

bool LKChannelAnalyzer::FindPeak(double *buffer, int &tbPointer, int &tbStartOfPulse)
{
#ifdef DEBUG_CHANA_FINDPEAK
    auto graphFP = (TGraph*) dGraphFPArray -> ConstructedAt(fNumGraphFP++);
    graphFP -> SetLineColor(kCyan+2);
    lk_debug << "Printing debug message up to tb " << DEBUG_CHANA_FINDPEAK << endl;
#endif

    int countAscending = 0;
    //int countAscendingBelowThreshold = 0;

    if (tbPointer==0)
        tbPointer = 1;

    double valuePrev = buffer[tbPointer-1];

    for (; tbPointer<fTbMax; tbPointer++)
    {
        double value = buffer[tbPointer];
        double yDiff = value - valuePrev;
#ifdef DEBUG_CHANA_FINDPEAK
        if (tbPointer<DEBUG_CHANA_FINDPEAK) lk_debug << "(t,y)=(" << tbPointer << ", " << value << "), dy=" << yDiff << " (#A=" << countAscending << ")" << endl;
#endif
        valuePrev = value;

        // If buffer difference of step is above threshold
        if (yDiff > fThresholdOneStep)
        {
#ifdef DEBUG_CHANA_FINDPEAK
            graphFP -> SetPoint(graphFP->GetN(),tbPointer+0.5,yDiff+fPedestal);
#endif
            //if (yDiff > 0)
            //if (value > fThreshold)
            if (value > 0 && yDiff > 0)
            {
                countAscending++;
#ifdef DEBUG_CHANA_FINDPEAK
                dGraphA -> SetPoint(dGraphA->GetN(),tbPointer+0.5,fPedestal+countAscending);
#endif
            }
            else
            {
                countAscending = 0;
            }
            //else countAscendingBelowThreshold++;
        }
        else
        {
#ifdef DEBUG_CHANA_FINDPEAK
            graphFP -> SetPoint(graphFP->GetN(),tbPointer+0.5,fPedestal);
#endif
            if (countAscending < fNumTbAcendingCut)
            {
#ifdef DEBUG_CHANA_FINDPEAK
                if (countAscending>2)
                    if (tbPointer<DEBUG_CHANA_FINDPEAK) lk_debug << "skip candidate from num-ascending-cut : " << countAscending << " < " << fNumTbAcendingCut << endl;
#endif
                countAscending = 0;
#ifdef DEBUG_CHANA_FINDPEAK
                dGraphA -> SetPoint(dGraphA->GetN(),tbPointer+0.5,fPedestal+countAscending);
#endif
                //countAscendingBelowThreshold = 0;
                continue;
            }

            tbPointer -= 1;
            if (value < fThreshold) {
#ifdef DEBUG_CHANA_FINDPEAK
                if (tbPointer<DEBUG_CHANA_FINDPEAK) lk_debug << "skip candidate from threshold-cut : " << value << " < " << fThreshold << endl;
#endif
                tbPointer += 1;
                continue;
            }

#ifdef DEBUG_CHANA_FINDPEAK
            if (tbPointer<DEBUG_CHANA_FINDPEAK) lk_debug << "Peak is found!" << endl;
#endif
            // Peak is found!
            tbStartOfPulse = tbPointer - countAscending;
            while (buffer[tbStartOfPulse] < value * fFloorRatio)
                tbStartOfPulse++;

#ifdef DEBUG_CHANA_FINDPEAK
            /*
            lk_debug << "found peak : " << tbStartOfPulse << " = " << tbPointer << " - " << countAscending << " + alpha" << endl;
            auto graphFP1 = (TGraph*) dGraphFPArray -> ConstructedAt(fNumGraphFP++);
            graphFP1 -> SetMarkerStyle(106);
            graphFP1 -> SetMarkerSize(2);
            graphFP1 -> SetPoint(0,tbPointer+0.5,buffer[tbPointer]);
            graphFP1 -> SetMarkerColor(kCyan+2);
            dMGraphFP -> Add(graphFP1,"samep");
            auto graphFP2 = (TGraph*) dGraphFPArray -> ConstructedAt(fNumGraphFP++);
            graphFP2 -> SetMarkerStyle(119);
            graphFP2 -> SetMarkerSize(2);
            graphFP2 -> SetPoint(0,tbStartOfPulse+0.5,buffer[tbStartOfPulse]);
            graphFP2 -> SetMarkerColor(kCyan+2);
            dMGraphFP -> Add(graphFP2,"samep");
            */
#endif

#ifdef DEBUG_CHANA_FINDPEAK
            if (graphFP->GetN()>0) dMGraphFP -> Add(graphFP,"samel");
#endif
            return true;
        }
    }

#ifdef DEBUG_CHANA_FINDPEAK
    if (graphFP->GetN()>0) dMGraphFP -> Add(graphFP,"samel");
#endif
    return false;
}

bool LKChannelAnalyzer::FitPulse(double *buffer, int tbStartOfPulse, int tbPeak,
        double &tbHit,
        double &amplitude,
        double &chi2Fitted,
        int    &ndf,
        bool   &isSaturated)
{
#ifdef DEBUG_CHANA_FITPULSE
    lk_debug << "tb0=" << tbStartOfPulse << ",  tbPeak=" << tbPeak << ",  ndf=" << ndf << endl;
#endif
    double valuePeak = buffer[tbPeak];
    isSaturated = false;

    //if (ndf > fNDFFit)
        //ndf = fNDFFit;

    // if peak value is larger than fDynamicRange, the pulse is isSaturated
    if (valuePeak >= fDynamicRange) {
        //ndf = tbPeak - tbStartOfPulse;
        //if (ndf > fNDFFit)
        //  ndf = fNDFFit;
        isSaturated = true;
    }

    LKTbIterationParameters par;
    par.SetScaleTbStep(fScaleTbStep);

    double stepChi2NDFCut = 1;

    double chi2NDFPrev = DBL_MAX; // Least-squares of previous fit
    double chi2NDFCurr = DBL_MAX; // Least-squares of current fit
    double tbStep = 0.1; // Time-bucket step to next fit
    double tbCurr = tbStartOfPulse; // Pulse starting time-bucket of current fit
    double tbPrev = tbCurr - tbStep; // Pulse starting time-bucket of previous fit

    ndf = fNDFFit;
    FitAmplitude(buffer, tbPrev, ndf, amplitude, chi2NDFPrev);
    ndf = fNDFFit;
    FitAmplitude(buffer, tbCurr, ndf, amplitude, chi2NDFCurr);

    par.Add(chi2NDFPrev,tbPrev);
    par.Add(chi2NDFCurr,tbCurr);
    double slope = par.Slope();

#ifdef DEBUG_CHANA_FITPULSE
    //lk_debug << "slope is " << slope << " " << chi2NDFCurr << " " << chi2NDFPrev << " " <<  tbCurr << " " << tbPrev << endl;
    double chi2NDF0 = chi2NDFPrev;
#endif

    int countIteration = 0;
    bool firstCheckFlag = false; // Checking flag to apply cut twice in a row
#ifdef DEBUG_CHANA_FITPULSE
    if (dGraph_it_tb==nullptr)      { dGraph_it_tb = new TGraph();      dGraph_it_tb      -> SetTitle(";iteration;tb"); }
    if (dGraph_it_tbStep==nullptr)  { dGraph_it_tbStep = new TGraph();  dGraph_it_tbStep  -> SetTitle(";iteration;tb step"); }
    if (dGraph_it_chi2==nullptr)    { dGraph_it_chi2 = new TGraph();    dGraph_it_chi2    -> SetTitle(";iteration;#Chi^{2}/NDF"); }
    if (dGraph_tb_chi2==nullptr)    { dGraph_tb_chi2 = new TGraph();    dGraph_tb_chi2    -> SetTitle(";tb;#Chi^{2}/NDF"); }
    if (dGraph_it_slope==nullptr)    { dGraph_it_slope = new TGraph();    dGraph_it_slope    -> SetTitle(";iteration;#slope = d(#Chi^{2}/NDF)/dtb"); }
    if (dGraph_tb_slope==nullptr)    { dGraph_tb_slope = new TGraph();    dGraph_tb_slope    -> SetTitle(";tb;#slope = d(#Chi^{2}/NDF)/dtb"); }
    if (dGraph_it_slopeInv==nullptr) { dGraph_it_slopeInv = new TGraph(); dGraph_it_slopeInv -> SetTitle(";iteration;1/#slope = dtb/d(#Chi^{2}/NDF)"); }
    if (dGraph_tb_slopeInv==nullptr) { dGraph_tb_slopeInv = new TGraph(); dGraph_tb_slopeInv -> SetTitle(";tb;1/#slope = dtb/d(#Chi^{2}/NDF)"); }
    for (auto graph : {
            dGraph_it_tb,
            dGraph_it_tbStep,
            dGraph_it_chi2,
            dGraph_tb_chi2,
            dGraph_it_slope,
            dGraph_tb_slope,
            dGraph_it_slopeInv,
            dGraph_tb_slopeInv}
        )
    {
        graph -> Set(0);
        graph -> SetMarkerStyle(24);
        graph -> SetLineColor(kBlue);
    }
    double slope2 = abs(slope);
    dGraph_it_tb -> SetPoint(countIteration, countIteration, tbCurr);
    dGraph_it_tbStep -> SetPoint(countIteration, countIteration, tbStep);
    dGraph_it_chi2 -> SetPoint(countIteration, countIteration, chi2NDFCurr);
    dGraph_it_slope -> SetPoint(countIteration, countIteration, slope2);
    dGraph_it_slopeInv -> SetPoint(countIteration, countIteration, double(1./slope2));
    dGraph_tb_chi2 -> SetPoint(countIteration, tbCurr, chi2NDFCurr);
    dGraph_tb_slope -> SetPoint(countIteration, tbCurr, slope2);
    dGraph_tb_slopeInv -> SetPoint(countIteration, tbCurr, double(1./slope2));
#endif

    bool foundNewMinimum = true;
    while (true)
    {
        countIteration++;
        if (foundNewMinimum) {
            tbPrev = tbCurr;
            chi2NDFPrev = chi2NDFCurr;
        }
        //tbStep = fScaleTbStep * slope;
        //if (tbStep>1) tbStep = 1;
        //else if (tbStep<-1) tbStep = -1;
        //tbCurr = tbPrev + tbStep;

        tbStep = par.TbStep();
        tbCurr = par.NextTb(tbPrev);

        if (tbCurr<0 || tbCurr>fTbStartCut) {
#ifdef DEBUG_CHANA_FITPULSE
            lk_debug << "break(" << countIteration << ") tbCur<0 || tbCur<fTbStartCut : " << tbCurr << " " << fTbStartCut << endl;
#endif
            return false;
        }

        ndf = fNDFFit;
        FitAmplitude(buffer, tbCurr, ndf, amplitude, chi2NDFCurr);
        //slope = -(chi2NDFCurr-chi2NDFPrev) / (tbCurr-tbPrev);
        foundNewMinimum = par.Add(chi2NDFCurr,tbCurr);
        slope = par.Slope();
#ifdef DEBUG_CHANA_FITPULSE
        double slope2 = abs(slope);
        dGraph_it_tb -> SetPoint(countIteration, countIteration, tbCurr);
        dGraph_it_tbStep -> SetPoint(countIteration, countIteration, tbStep);
        dGraph_it_chi2 -> SetPoint(countIteration, countIteration, chi2NDFCurr);
        dGraph_it_slope -> SetPoint(countIteration, countIteration, double(slope2));
        dGraph_it_slopeInv -> SetPoint(countIteration, countIteration, double(1./slope2));
        dGraph_tb_chi2 -> SetPoint(countIteration, tbCurr, chi2NDFCurr);
        dGraph_tb_slope -> SetPoint(countIteration, tbCurr, double(slope2));
        dGraph_tb_slopeInv -> SetPoint(countIteration, tbCurr, double(1./slope2));
        lk_debug << "IT-" << countIteration << " tb: " << tbPrev << "->" << tbCurr << "(" << tbStep << ")" << ",  c2/n: " << chi2NDFPrev << "->" << chi2NDFCurr << ",  tb-step: " << tbStep << " ndf=" << ndf << endl;
#endif

        if (abs(tbStep)<fTbStepCut) {
#ifdef DEBUG_CHANA_FITPULSE
            lk_debug << "break(" << countIteration << ") tbStep < " << fTbStepCut << " : " << tbStep << endl;
#endif
            break;
        }

        if (countIteration >= fIterMax) {
#ifdef DEBUG_CHANA_FITPULSE
            lk_debug << "break(" << countIteration << ") : iteration cut exit: " << countIteration << " < " << fIterMax << endl;
#endif
            break;
        }
    }


    tbHit = par.fT1;
    chi2Fitted = par.fC1;

    //if (slope > 0) { // pre-fit is better
    //    tbHit = tbPrev;
    //    chi2Fitted = chi2NDFPrev;
    //}
    //else { // current-fit is better
    //    tbHit = tbCurr;
    //    chi2Fitted = chi2NDFCurr;
    //}

    return true;
}

void LKChannelAnalyzer::FitAmplitude(double *buffer, double tbStartOfPulse,
        int &ndf,
        double &amplitude,
        double &chi2NDF)
{
    if (std::isnan(tbStartOfPulse)) {
#ifdef DEBUG_CHANA_FITAMPLITUDE
        lk_debug << "return tb is nan" << endl;
#endif
        chi2NDF = 1.e10;
        return;
    }
#ifdef DEBUG_CHANA_FITAMPLITUDE
    lk_debug << "pulse y-values: "; for (auto i=0; i<20; ++i) e_cout << i << "/" << fPulse -> Eval(i) << ", "; e_cout << endl;
#endif
    double refy = 0;
    double ref2 = 0;
    int tb0 = int(tbStartOfPulse);
#ifdef DEBUG_CHANA_FITAMPLITUDE
    lk_debug << "tb0 = abs(tbStartOfPulse) : " << tb0 << " = abs(" << tbStartOfPulse << ")" << endl;
#endif
    double tbOffset = tbStartOfPulse - tb0;

    int ndfFit = 0;
    int iTbPulse = 0;
    for (; (ndfFit<ndf && iTbPulse<fNDFPulse); iTbPulse++)
    {
        int tbData = tb0 + iTbPulse;
#ifdef DEBUG_CHANA_FITAMPLITUDE
        lk_debug << "tbData = tb0 + iTbPulse : " << tbData << " = " << tb0 << " + " << iTbPulse << endl;
#endif
        //if (tbData<0 || tbData>=350)
        //    lk_debug << tbData << endl;
        if (tbData>=fTbMax)
            break;
        double valueData = buffer[tbData];
        if (valueData==(4095-fPedestal)) {
            continue;
        }
        if (valueData>=fDynamicRange)
            continue;
        ndfFit++;
        double tbRef = iTbPulse - tbOffset + 0.5;
        double valueRef = fPulse -> Eval(tbRef);
        double errorRef = fPulse -> Error(tbRef);
        double weigth = 1./(errorRef*errorRef);
        refy += weigth * valueRef * valueData;
        ref2 += weigth * valueRef * valueRef;
#ifdef DEBUG_CHANA_FITAMPLITUDE
        lk_debug << "iTbPulse; weight, valueRef, valueData : " << weigth << ", " << valueRef << ", " << valueData << endl;
        lk_debug << "iTbPulse; amplitude = refy / ref : " << iTbPulse << "; " << refy/ref2 << " = " << refy << " / " << ref2 << endl;
#endif
    }
    ndfFit = ndfFit-2;
#ifdef DEBUG_CHANA_FITAMPLITUDE
    lk_debug << "ndf is " << ndf << " ndfFit is " << ndfFit << endl;
#endif

    if (ref2==0) {
        chi2NDF = 1.e10;
        return;
    }
    amplitude = refy / ref2;
#ifdef DEBUG_CHANA_FITAMPLITUDE
    lk_debug << "final amplitude = " << amplitude << " = " << refy << " / " << ref2 << endl;
#endif
    chi2NDF = 0;

    ndfFit = 0;
    iTbPulse = 0;
    for (; (ndfFit<ndf && iTbPulse<fNDFPulse); iTbPulse++)
    {
        int tbData = tb0 + iTbPulse;
        //if (tbData<0 || tbData>=350)
        //    lk_debug << "e " << tbData << endl;
        if (tbData>=fTbMax)
            break;
        double valueData = buffer[tbData];
        if (valueData>=fDynamicRange)
            continue;
        ndfFit++;
        double tbRef = iTbPulse - tbOffset + 0.5;
        double valueRefA = amplitude * fPulse -> Eval(tbRef);
        double errorRefA = amplitude * fPulse -> Error(tbRef);
        double residual = (valueData-valueRefA)*(valueData-valueRefA)/errorRefA/errorRefA;
#ifdef DEBUG_CHANA_FITAMPLITUDE
        lk_debug << "residual = (valueData - valueRefA)^2 / errorRefA^2 at tbRef : " << residual << " = (" << valueData << " - " << valueRefA << ")^2 / " << errorRefA << "^2 at " << tbRef << endl;
#endif
        chi2NDF += residual;
    }
    ndfFit = ndfFit-2;

    //lk_debug << "ndfFit: " << ndfFit << endl;

    chi2NDF = chi2NDF/ndfFit;
    ndf = ndfFit;
}

bool LKChannelAnalyzer::TestPulse(double *buffer, double tbHitPrev, double amplitudePrev, double tbHit, double amplitude)
{
    //int numTbsCorrection = fNumTbsCorrection;
    //if (numTbsCorrection + int(tbHit) >= fTbMax)
    //    numTbsCorrection = fTbMax - int(tbHit);

    if (amplitude < fThreshold)
        return false;

    /*
    double amplitudeCut = 0.5 * fPulse -> Eval(tbHit+fTbSeparationWidth, tbHitPrev, amplitudePrev);
    if (amplitude < amplitudeCut)
    {
        for (int iTbPulse = -1; iTbPulse < numTbsCorrection; iTbPulse++) {
            int tb = int(tbHit) + iTbPulse;
            buffer[tb] -= fPulse -> Eval(tb, tbHit, amplitude);
        }
        return false;
    }
    */

    return true;
}

void LKChannelAnalyzer::SubtractPulse(double *buffer, double tbHit, double amplitude)
{
    /*
    for (int iTbPulse = -1; iTbPulse < numTbsCorrection; iTbPulse++) {
        int tb = int(tbHit) + iTbPulse;
        buffer[tb] -= fPulse -> Eval(tb, tbHit, amplitude);
    }
    */

    int tbCorrectionRange1 = tbHit + fPulseRefTbMin;
    int tbCorrectionRange2 = tbHit + fPulseRefTbMax;
    if (tbCorrectionRange1 < 0)       tbCorrectionRange1 = 0;
    if (tbCorrectionRange2 >= fTbMax) tbCorrectionRange2 = fTbMax-1;
    for (int tb = tbCorrectionRange1; tb < tbCorrectionRange2; tb++) {
        buffer[tb] -= fPulse -> Eval(tb+0.5, tbHit, amplitude);
    }
}

void LKChannelAnalyzer::SetPad(TVirtualPad *pad, TH2D* hist)
{
    fPadFitPanel = pad;
    if (hist!=nullptr)
        fHistFitPanel = hist;
    else if (fHistFitPanel==nullptr) {
        fHistFitPanel = new TH2D("HistCA_FitPanel",";tb;charge;",128,0,512,205,0,4100);
    }
    /*
    auto nx = fHistFitPanel -> GetXaxis() -> GetNbins();
    auto ny = fHistFitPanel -> GetYaxis() -> GetNbins();
    for (auto ix=1; ix<nx; ++ix) {
        for (auto iy=1; iy<ny; ++iy) {
            if (fHistFitPanel->GetXaxis()->GetBinLowEdge(ix)>400 && fHistFitPanel->GetXaxis()->GetBinLowEdge(iy)>3500)
                continue;
        }
    }
    */
    AddInteractivePad(fPadFitPanel);
}

void LKChannelAnalyzer::ExecMouseClickEventOnPad(TVirtualPad *pad, double xOnClick, double yOnClick)
{
    /*
    if (pad!=fPadFitPanel) {
        lk_error << pad << " " << fPadFitPanel << endl;
        return;
    }

    int selectedBin = fHistParam -> FindBin(xOnClick, yOnClick);
    int binx, biny, binz;
    fHistParam -> GetBinXYZ(selectedBin, binx, biny, binz);
    auto x1 = fHistParam -> GetXaxis() -> GetBinLowEdge(binx);
    auto y1 = fHistParam -> GetYaxis() -> GetBinLowEdge(biny);
    if (x1<400&&y1<3500)
        return;

    fPadFitPanel -> cd();
    */
}

TGraphErrors* LKChannelAnalyzer::FillGraphPulseFitting(TGraphErrors* graph, double tb0, double amplitude, double pedestal)
{
    fPulse -> FillPulseGraph(graph, tb0, amplitude, pedestal);
    return graph;
}

TGraphErrors* LKChannelAnalyzer::FillGraphSigAtThreshold(TGraphErrors* graph, double tb0, double amplitude, double pedestal)
{
    graph -> Set(0);
    graph -> SetPoint(0,tb0-5,pedestal);
    graph -> SetPoint(1,tb0  ,pedestal);
    graph -> SetPoint(2,tb0  ,fThreshold+pedestal);
    graph -> SetPoint(3,tb0+5,fThreshold+pedestal);
    graph -> SetPoint(4,tb0+5,pedestal);
    graph -> SetLineColor(kRed);
    graph -> SetMarkerColor(kRed);
    return graph;
}

TGraphErrors* LKChannelAnalyzer::GetGraphSigAtThreshold(double tb0, double amplitude, double pedestal)
{
    if (fGraphArray==nullptr)
        fGraphArray = new TClonesArray("TGraphErrors",100);
    auto graphSigAtThreshold = (TGraphErrors*)  fGraphArray -> ConstructedAt(fNumGraphs++);
    return FillGraphSigAtThreshold(graphSigAtThreshold, tb0, amplitude, pedestal);
}

TGraphErrors* LKChannelAnalyzer::FillGraphSigAtMaximum(TGraphErrors* graph, double tb0, double amplitude, double pedestal)
{
    graph -> Set(0);
    graph -> SetPoint(0,tb0-5,pedestal);
    graph -> SetPoint(1,tb0  ,pedestal);
    graph -> SetPoint(2,tb0  ,amplitude+pedestal);
    graph -> SetPoint(3,tb0+5,amplitude+pedestal);
    graph -> SetPoint(4,tb0+5,pedestal);
    graph -> SetLineColor(kRed);
    graph -> SetMarkerColor(kRed);
    return graph;
}

TGraphErrors* LKChannelAnalyzer::GetGraphSigAtMaximum(double tb0, double amplitude, double pedestal)
{
    if (fGraphArray==nullptr)
        fGraphArray = new TClonesArray("TGraphErrors",100);
    auto graphSigAtMaximum = (TGraphErrors*) fGraphArray -> ConstructedAt(fNumGraphs++);
    return FillGraphSigAtMaximum(graphSigAtMaximum, tb0, amplitude, pedestal);
}

TGraphErrors* LKChannelAnalyzer::GetGraphPulseFitting(double tb0, double amplitude, double pedestal)
{
    if (fGraphArray==nullptr)
        fGraphArray = new TClonesArray("TGraphErrors",100);
    TGraphErrors* graph = (TGraphErrors*) fGraphArray -> ConstructedAt(fNumGraphs++);
    if      (fAnalyzerMode==kPulseFittingMode) fPulse -> FillPulseGraph(graph, tb0, amplitude, pedestal);
    else if (fAnalyzerMode==kSigAtMaximumMode) FillGraphSigAtMaximum(graph, tb0, amplitude, pedestal);
    else if (fAnalyzerMode==kSigAtThresholdMode) FillGraphSigAtThreshold(graph, tb0, amplitude, pedestal);

    return graph;
}

TGraph* LKChannelAnalyzer::GetPedestalGraph(double tb1, double tb2)
{
    if (fGraphArray==nullptr)
        fGraphArray = new TClonesArray("TGraphErrors",100);
    auto graphPedestal = (TGraph*) fGraphArray -> ConstructedAt(fNumGraphs++);
    return FillPedestalGraph(graphPedestal, tb1, tb2);
}

TGraph* LKChannelAnalyzer::FillPedestalGraph(TGraph* graph, double tb1, double tb2)
{
    graph -> Set(0);
    graph -> SetPoint(0,tb1, fPedestal);
    graph -> SetPoint(1,tb2, fPedestal);
    graph -> SetLineStyle(2);
    graph -> SetLineColor(kGray+1);
    return graph;
}
