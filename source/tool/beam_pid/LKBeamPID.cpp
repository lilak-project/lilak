#include "LKBeamPID.h"

ClassImp(LKBeamPID)

LKBeamPID::LKBeamPID()
{
    fStage = 0;
    gStyle -> SetNumberContours(100);
    gStyle -> SetOptStat("e");
    ResetBinning();
    fFitArray = new TObjArray();
    fHistDataArray = new TObjArray();
    fHistFitGArray = new TObjArray();
    fHistBackArray = new TObjArray();
    fHistCalcArray = new TObjArray();
    fHistTestArray = new TObjArray();
    fHistErrrArray = new TObjArray();
    fGraphFitError = new TGraph();
    fTop = new LKDrawingGroup();
    fGroupFit = fTop -> CreateGroup(Form("event_fit_%04d",fCurrentRunNumber));
    fGroupPID = fTop -> CreateGroup(Form("event_pid_%04d",fCurrentRunNumber));
    fDraw2D = fGroupPID -> CreateDrawing(Form("draw_2d_%04d",fCurrentRunNumber));
    fDraw2D -> SetCanvasSize(1,1,1);
    fDraw2D -> SetCanvasMargin(.11,.15,.11,0.08);
    fFitCountDiff = new TF1("fitdiff","pol1",0,1);
    fFitCountDiff -> SetParameters(0,0);
    fFitCountDiff -> SetLineColor(kBlue);
    fFitCountDiff -> SetLineStyle(1);
    fFitCountDiff2 = new TF1("fitdiff2","pol1",0,1);
    fFitCountDiff2 -> SetParameters(0,0);
    fFitCountDiff2 -> SetLineWidth(1);
    fFitCountDiff2 -> SetLineStyle(2);

    InitParameters();
    fGroupFit -> Draw();
}

void LKBeamPID::InitParameters()
{
    fBnn0.SetNMM(800,-1025,-875,800,0,200);

    LKParameterContainer par("config.mac");
    par.UpdatePar(fFitRangeInSigma,    "fit_range");
    par.UpdatePar(fSetXName,           "x_name");
    par.UpdatePar(fSetYName,           "y_name");
    par.UpdatePar(fNumContours,        "num_contours");
    par.UpdatePar(fBnn0,               "binning");
    par.UpdatePar(fFinalContourAScale, "cut_s_value");
    par.UpdatePar(fDefaultPath,        "data_path");
    par.UpdatePar(fDefaultFormat,      "file_format");
    par.UpdatePar(fDefaultFitSigmaX,   "fit_sigma_x");
    par.UpdatePar(fDefaultFitSigmaY,   "fit_sigma_y");
    par.UpdatePar(fDefaultFitTheta,    "fit_theta");
    par.UpdatePar(fAmpRatioRange,      "fit_amp_ratio_range");
    par.UpdatePar(fPosRatioRangeInSig, "fit_pos_ratio_range");
    par.UpdatePar(fSigmaRatioRange,    "fit_sigma_ratio_range");
    par.UpdatePar(fThetaRange,         "fit_theta_range");
    fContourScaleList = par.InitPar(fContourScaleList, "example_s_list");
    par.Print();

    if (fSetXName==".") fSetXName = "";
    if (fSetYName==".") fSetYName = "";
    fBnn1 = fBnn0;

    for (auto bin=0; bin<=fNumContours; ++bin) {
        fValueOfS.push_back(0);
        fErrorAtS.push_back(0);
    }
}

void LKBeamPID::Help(TString mode)
{
    //
}

bool LKBeamPID::ListFiles(TString path, TString format)
{
    e_title << "ListFiles" << endl;

    if (path.IsNull()) path = fDefaultPath;
    if (format.IsNull()) format = fDefaultFormat;

    if (!fRunCollected) {
        CollectRootFiles(fListGenFile,path,format);
        fRunCollected = true;
    }

    if (fListGenFile.size()==0) {
        e_info << "End of file list" << endl;
        return false;
    }

    int count = 0;
    e_cout << endl;
    e_info << "List of files" << endl;
    for (auto fileName : fListGenFile) {
        if (count%4==0) e_cout << "--------" << endl;
        e_cout << "   " << left << setw(5) << count++ << " " << fileName << endl;
    }
    e_cout << "--------" << endl;
    e_cout << endl;
    Help("s");

    fStage = 1;
    return true;
}

bool LKBeamPID::SelectFile(int index)
{
    e_title << "SelectFile" << endl;
    while (true)
    {
        if (index<0) {
            e_note << "Enter index number from the file list (q:exit): ";
            TString inputString;
            cin >> inputString;
            inputString = inputString.Strip(TString::kBoth);
            if (inputString=="x"||inputString=="q") { e_cout << "quit" << endl; return false; }
            if (inputString.IsDigit()==false) {
                e_info << "Invalid input." << endl;
                continue;
            }
            index = inputString.Atoi();
        }
        if (index<0||index>=fListGenFile.size()) {
            e_warning << "Index not in range. (size=" << fListGenFile.size() << ")" << endl;
            return false;
        }
        fCurrentFileName = fListGenFile.at(index);
        TString runNumberString = TString(fCurrentFileName(fCurrentFileName.Index("chkf")+8,4));
        if (runNumberString.IsDigit()) fCurrentRunNumber = runNumberString.Atoi();
        else fCurrentRunNumber = 999999999;
        fListGenFile.erase(fListGenFile.begin()+index);
        e_cout << "   " << fCurrentFileName << endl;
        e_cout << "   (this file will be removed from the list)" << endl;
        break;
    }

    if (fDataFile) fDataFile -> Close();
    fDataFile = new TFile(fCurrentFileName,"read");
    fDataTree = (TTree*) fDataFile -> Get("tree");
    if (fSetYName.IsNull()) {
        if      (fCurrentFileName.Index("chkf2run")>=0) { fCurrentType = 2; fYName = "f2ssde"; }
        else if (fCurrentFileName.Index("chkf3run")>=0) { fCurrentType = 3; fYName = "f3ssde"; }
    }
    else
        fYName = fSetYName;

    if (fSetXName.IsNull())
        fXName = "rf0";
    else
        fXName = fSetXName;

    if (!fInitialized)
    {
        fInitialized = true;
    }
    fStage = 2;
    CreateAndFillHistogram(0);

    fGroupFit -> SetName(Form("event_fit_%04d",fCurrentRunNumber));
    fGroupPID -> SetName(Form("event_pid_%04d",fCurrentRunNumber));
    fDraw2D -> SetName(Form("draw_2d_%04d",fCurrentRunNumber));
    fDraw2D -> Draw();
    e_cout << endl;
    Help("td");

    return true;
}

void LKBeamPID::CreateAndFillHistogram(int printb)
{
    if (printb) {
        PrintBinning();
    }
    if (fDataTree) {
        fHistPID = fBnn1.NewH2(Form("histPID_%04d",fCurrentRunNumber),Form(";%s;%s",fXName.Data(),fYName.Data()));
        Long64_t entriesTotal = fDataTree->Draw(Form("%s:%s>>%s",fYName.Data(),fXName.Data(),fHistPID->GetName()),"","goff");
        e_info << "Total entries =" << entriesTotal << " (hist)=" << fHistPID -> GetEntries() << endl;
        fHistPID -> GetXaxis() -> SetTitleOffset(1.2);
        fHistPID -> SetTitle(Form("RUN %04d",fCurrentRunNumber));
        fDraw2D -> Clear();
        fDraw2D -> Add(fHistPID, "colz");
        fDraw2D -> SetStatsFillStyle(3001);
        fDraw2D -> Print(); // XXX
        fStage = 3;
    }
}

void LKBeamPID::AutoBinning()
{
    if (fDataTree) {
        double x1 = fDataTree -> GetMinimum(fXName);
        double x2 = fDataTree -> GetMaximum(fXName);
        double y1 = fDataTree -> GetMinimum(fYName);
        double y2 = fDataTree -> GetMaximum(fYName);
        SetBinning(200, x1, x2, 200, y1, y2);
        PrintBinning();
        CreateAndFillHistogram(0);
        fDraw2D -> Draw();
    }
}

void LKBeamPID::UsePad(TVirtualPad *pad)
{
    fDraw2D -> Clear();
    fDraw2D -> Add(gPad);
    fDraw2D -> Clear("!main:!cvs");
    fDraw2D -> SetOptStat(10);
    fHistPID = (TH2D*) fDraw2D -> GetMainHist();
    fXName = fHistPID -> GetXaxis() -> GetTitle();
    fYName = fHistPID -> GetYaxis() -> GetTitle();
    fCurrentType = 1;
    fStage = 4;
    fDraw2D -> Draw();
    return;
}

void LKBeamPID::ReselectCenters()
{
    if (fStage<2) {
        e_warning << "Must select file before drawing points!" << endl;
        return;
    }
    fDraw2D -> Clear("!main:!cvs");
    fDraw2D -> Draw();
    SelectCenters();
}

void LKBeamPID::Redraw()
{
    if (fStage<2) {
        e_warning << "Must select file before drawing points!" << endl;
        return;
    }
    fDraw2D -> Clear("!main:!cvs");
    fDraw2D -> Draw();
    Help("tr");
}

void LKBeamPID::SelectCenters(vector<vector<double>> points)
{
    e_title << "SelectCenters" << endl;
    if (fStage<2) {
        e_warning << "Must select file before drawing points!" << endl;
        return;
    }

    if (points.size()==0)
    {
        e_cout << endl;
        e_info << "Draw graph with the mouse pointer! Double click to end." << endl;
        auto graph2 = (TGraph*) fDraw2D -> GetCanvas() -> WaitPrimitive("Graph","PolyLine");
        auto graph = (TGraph*) graph2 -> Clone();
        graph -> SetMarkerStyle(20);
        fDraw2D -> Add(graph,"samep");
        auto numPoints = graph -> GetN();
        for (auto iPoint=0; iPoint<numPoints; ++iPoint)
        {
            double x = graph -> GetPointX(iPoint);
            double y = graph -> GetPointY(iPoint);
            e_cout << "   " << iPoint << " " << x << " " << y << endl;
            points.push_back(vector<double>{x,y});
        }
    }

    fGroupFit -> Clear();
    double wx = fHistPID -> GetXaxis() -> GetBinWidth(1);
    double wy = fHistPID -> GetYaxis() -> GetBinWidth(1);
    double binA = wx*wy;

    e_info << "Total " << points.size() << " pid centers are selected" << endl;

    int count = 0;
    fFitArray -> Clear();
    for (auto point : points)
    {
        auto fit = Fit2DGaussian(fHistPID, count, point[0], point[1]);
        auto idx = fFitArray -> GetEntries();
        fFitArray -> Add(fit);
        {
            double xx1, xx2, yy1, yy2;
            fit -> GetParLimits(1,xx1,xx2);
            fit -> GetParLimits(3,yy1,yy2);
            auto graphCenterRange = new TGraph();
            graphCenterRange -> SetName("graphCenterRange");
            graphCenterRange -> SetLineColor(kGreen);
            graphCenterRange -> SetLineStyle(9);
            graphCenterRange -> SetPoint(0,xx1,yy1);
            graphCenterRange -> SetPoint(1,xx2,yy1);
            graphCenterRange -> SetPoint(2,xx2,yy2);
            graphCenterRange -> SetPoint(3,xx1,yy2);
            graphCenterRange -> SetPoint(4,xx1,yy1);
            fit -> GetRange(xx1,yy1,xx2,yy2);
            auto graphFitRange = new TGraph();
            graphFitRange -> SetName("graphFitRange");
            graphFitRange -> SetLineColor(kYellow);
            graphFitRange -> SetLineStyle(9);
            graphFitRange -> SetPoint(0,xx1,yy1);
            graphFitRange -> SetPoint(1,xx2,yy1);
            graphFitRange -> SetPoint(2,xx2,yy2);
            graphFitRange -> SetPoint(3,xx1,yy2);
            graphFitRange -> SetPoint(4,xx1,yy1);
            fit -> SetRange(xx1-(xx2-xx1),yy1-(yy2-yy1),xx2+(xx2-xx1),yy2+(yy2-yy1));
            fDraw2D -> Add(graphCenterRange,"samel");
            fDraw2D -> Add(graphFitRange,"samel");
        }
        {
            auto amplit = fit->GetParameter(0);
            auto valueX = fit->GetParameter(1);
            auto sigmaX = fit->GetParameter(2);
            auto valueY = fit->GetParameter(3);
            auto sigmaY = fit->GetParameter(4);
            auto thetaR = fit->GetParameter(5);
            for (double contourScale : fContourScaleList) {
                auto graphC = GetContourGraph(contourScale*amplit, amplit, valueX, sigmaX, valueY, sigmaY, thetaR);
                graphC -> SetName(Form("graphC_%d_S%s",idx,LKMisc::RemoveTrailing0(100*contourScale,1).Data()));
                graphC -> SetLineColor(kRed);
                graphC -> SetLineStyle(9);
                fDraw2D -> Add(graphC,"samel");
            }
            auto graphC = GetContourGraph(fFinalContourAScale*amplit, amplit, valueX, sigmaX, valueY, sigmaY, thetaR);
            graphC -> SetName(Form("graphC_%d_S%s",idx,LKMisc::RemoveTrailing0(100*fFinalContourAScale,1).Data()));
            graphC -> SetLineColor(kRed);
            fDraw2D -> Add(graphC,"samel");
        }
        count++;
    }
    fGroupFit -> Draw();
    fDraw2D -> Draw();

    Help("rqf");

    fStage = 5;
    return;
}

void LKBeamPID::FitTotal(bool calibrationRun)
{
    if (calibrationRun) e_title << "CalibrationRun" << endl;
    else e_title << "FitTotal" << endl;

    if (fStage<2) {
        e_warning << "Must select file before fit total pid!" << endl;
        return;
    }
    else if (fStage<5) {
        e_warning << "Must select points before fit total pid!" << endl;
        return;
    }

    fDraw2D -> Clear("!main:!cvs");
    fGroupFit -> Clear();
    auto numFits = fFitArray -> GetEntries();
    TString formulaTotal;
    double xx1=DBL_MAX, xx2=-DBL_MAX, yy1=DBL_MAX, yy2=-DBL_MAX;
    for (auto iFit=0; iFit<numFits; ++iFit)
    {
        TString formulaCurrent = fFormulaRotated2DGaussian.Data();
        formulaCurrent.ReplaceAll("[0]",Form("[%d]",0+iFit*6));
        formulaCurrent.ReplaceAll("[1]",Form("[%d]",1+iFit*6));
        formulaCurrent.ReplaceAll("[2]",Form("[%d]",2+iFit*6));
        formulaCurrent.ReplaceAll("[3]",Form("[%d]",3+iFit*6));
        formulaCurrent.ReplaceAll("[4]",Form("[%d]",4+iFit*6));
        formulaCurrent.ReplaceAll("[5]",Form("[%d]",5+iFit*6));
        if (iFit==0) formulaTotal = formulaCurrent;
        else formulaTotal = formulaTotal + " + " + formulaCurrent;
        auto fit = (TF2*) fFitArray -> At(iFit);
        auto amplit = fit->GetParameter(0);
        auto valueX = fit->GetParameter(1);
        auto sigmaX = fit->GetParameter(2);
        auto valueY = fit->GetParameter(3);
        auto sigmaY = fit->GetParameter(4);
        auto thetaR = fit->GetParameter(5);
        auto x1 = valueX-fFitRangeInSigma*sigmaX; if (xx1>x1) xx1 = x1;
        auto x2 = valueX+fFitRangeInSigma*sigmaX; if (xx2<x2) xx2 = x2;
        auto y1 = valueY-fFitRangeInSigma*sigmaY; if (yy1>y1) yy1 = y1;
        auto y2 = valueY+fFitRangeInSigma*sigmaY; if (yy2<y2) yy2 = y2;
    }
    auto fitContanminent = new TF2(Form("fitContanminent_%04d", fCurrentRunNumber), formulaTotal, xx1, xx2, yy1, yy2);
    auto fitTotal = new TF2(Form("fitTotal_%04d", fCurrentRunNumber), formulaTotal, xx1, xx2, yy1, yy2);
    fitTotal -> SetLineColor(kMagenta);
    fitTotal -> SetContour(3);
    for (auto iFit=0; iFit<numFits; ++iFit)
    {
        auto fit = (TF2*) fFitArray -> At(iFit);
        auto amplit = fit->GetParameter(0);
        auto valueX = fit->GetParameter(1);
        auto valueY = fit->GetParameter(2);
        auto sigmaX = fit->GetParameter(3);
        auto sigmaY = fit->GetParameter(4);
        auto thetaR = fit->GetParameter(5);
        fitTotal -> SetParameter(0+iFit*6, amplit);
        fitTotal -> SetParameter(1+iFit*6, valueX);
        fitTotal -> SetParameter(2+iFit*6, valueY);
        fitTotal -> SetParameter(3+iFit*6, sigmaX);
        fitTotal -> SetParameter(4+iFit*6, sigmaY);
        fitTotal -> SetParameter(5+iFit*6, thetaR);
        fitTotal -> SetParLimits(0+iFit*6, amplit*(1.-fAmpRatioRange), amplit*(1.+fAmpRatioRange));
        fitTotal -> SetParLimits(1+iFit*6, valueX-fPosRatioRangeInSig*sigmaX, valueX+fPosRatioRangeInSig*sigmaX);
        fitTotal -> SetParLimits(2+iFit*6, valueY-fPosRatioRangeInSig*sigmaY, valueY+fPosRatioRangeInSig*sigmaY);
        fitTotal -> SetParLimits(3+iFit*6, sigmaX*(1.-fSigmaRatioRange), sigmaX*(1.+fSigmaRatioRange));
        fitTotal -> SetParLimits(4+iFit*6, sigmaY*(1.-fSigmaRatioRange), sigmaY*(1.+fSigmaRatioRange));
        fitTotal -> SetParLimits(5+iFit*6, thetaR-fThetaRange, thetaR+fThetaRange);
    }
    e_info << "Fitting " << numFits << " PIDs in " << Form("x=(%f,%f), y=(%f,%f) ...",xx1,xx2,yy1,yy2) << endl;
    fHistPID -> Fit(fitTotal,"QBR0");

    auto legend = new TLegend();
    legend -> SetFillStyle(3001);
    legend -> SetMargin(0.1);
    for (auto iFit=0; iFit<numFits; ++iFit)
    {
        auto fit = (TF2*) fFitArray -> At(iFit);
        auto amplit = fitTotal->GetParameter(0+iFit*6);
        auto valueX = fitTotal->GetParameter(1+iFit*6);
        auto sigmaX = fitTotal->GetParameter(2+iFit*6);
        auto valueY = fitTotal->GetParameter(3+iFit*6);
        auto sigmaY = fitTotal->GetParameter(4+iFit*6);
        auto thetaR = fitTotal->GetParameter(5+iFit*6);
        fit -> SetParameter(0,amplit);
        fit -> SetParameter(1,valueX);
        fit -> SetParameter(2,sigmaX);
        fit -> SetParameter(3,valueY);
        fit -> SetParameter(4,sigmaY);
        fit -> SetParameter(5,thetaR);
        fit -> SetLineColor(kMagenta);
        e_cout << "   " << fit -> GetName() << ": " << std::left
            << setw(16) << Form("amp=%f,", amplit)
            << setw(28) << Form("x=(%f, %f),", valueX, sigmaX)
            << setw(28) << Form("y=(%f, %f),", valueY, sigmaY)
            << setw(18) << Form("theta=%f", thetaR*TMath::RadToDeg())
            << endl;
        for (double contourScale : fContourScaleList) {
            auto graphC = GetContourGraph(contourScale*amplit, amplit, valueX, sigmaX, valueY, sigmaY, thetaR);
            graphC -> SetLineColor(kRed);
            graphC -> SetLineStyle(9);
            fDraw2D -> Add(graphC,"samel");
        }
        auto graphC = GetContourGraph(fFinalContourAScale*amplit, amplit, valueX, sigmaX, valueY, sigmaY, thetaR);
        graphC -> SetLineColor(kRed);
        fDraw2D -> Add(graphC,"samel");
        auto text = new TText(valueX,valueY,Form("%d",iFit));
        text -> SetTextAlign(22);
        text -> SetTextSize(0.02);
        if (amplit<fHistPID->GetMaximum()*0.3)
            text -> SetTextColor(kGreen);
        fDraw2D -> Add(text,"same");
        for (auto iPar=0; iPar<fitTotal->GetNpar(); ++iPar)
            fitContanminent->SetParameter(iPar,fitTotal->GetParameter(iPar));
        fitContanminent->SetParameter(0+iFit*6,0);
        auto draw = GetFitTestDrawing(iFit,fHistPID,fit,fitContanminent,(iFit==0));
        fGroupFit -> Add(draw);
        TH1D *histData = (TH1D*) fHistDataArray -> At(iFit);
        TH1D *histBack = (TH1D*) fHistBackArray -> At(iFit);
        TH1D *histCalc = (TH1D*) fHistCalcArray -> At(iFit);
        auto bin = histData -> FindBin(fFinalContourAScale+0.5*(1./fNumContours));
        auto count = histData -> GetBinContent(bin);
        auto contamination = histBack -> GetBinContent(bin);
        auto calculated = histCalc -> GetBinContent(bin);
        auto corrected = count - contamination;
        //fDraw2D -> AddLegendLine(Form("%d) cc=%d",iFit,int(corrected)));
        legend -> AddEntry((TObject*)nullptr,Form("[%d] %d (%d)",iFit,int(count),int(contamination)),"");
    }
    {
        TH2D *histErrr = (TH2D*) fHistErrrArray -> At(0);
        auto draw_errr = fGroupFit -> CreateDrawing();
        draw_errr -> Add(histErrr,"colz");
        auto graph = new TGraphErrors();
        graph -> SetName("count_difference");
        for (auto bin=2; bin<=fNumContours; ++bin) {
            auto hist_e = (TH1D*) histErrr -> ProjectionY(Form("hist_e_proj%d",bin),bin,bin);
            auto mean = hist_e -> GetMean();
            auto stddev = hist_e -> GetStdDev();
            graph -> SetPoint(graph->GetN(),histErrr->GetXaxis()->GetBinCenter(bin),mean);
            graph -> SetPointError(graph->GetN()-1,0,stddev);
            fValueOfS[bin] = double(bin-1)/fNumContours;
            fErrorAtS[bin] = stddev;
        }
        graph -> SetMarkerStyle(24);
        draw_errr -> Add(graph,"samep");
        draw_errr -> SetGridy();
        if (calibrationRun) {
            fFitCountDiff -> SetParameter(0,graph->GetPointX(0));
            graph -> Fit(fFitCountDiff,"QBR0");
            draw_errr -> Add(fFitCountDiff,"samel");
            fCalibrated = true;
        }
        else {
            //fFitCountDiff2 -> SetParameter(0,graph->GetPointX(0));
            //graph -> Fit(fFitCountDiff2);
            //draw_errr -> Add(fFitCountDiff2,"samel");
        }
    }

    //if (calibrationRun==false)
    {
        auto graphFitRange = new TGraph();
        graphFitRange -> SetLineColor(kYellow);
        graphFitRange -> SetLineStyle(2);
        graphFitRange -> SetPoint(0,xx1,yy1);
        graphFitRange -> SetPoint(1,xx2,yy1);
        graphFitRange -> SetPoint(2,xx2,yy2);
        graphFitRange -> SetPoint(3,xx1,yy2);
        graphFitRange -> SetPoint(4,xx1,yy1);
        fDraw2D -> Add(graphFitRange,"samel");

        //fDraw2D -> SetCreateLegend();
        fDraw2D -> SetLegendCorner(1);
        fDraw2D -> Add(legend);
        fGroupFit -> Draw();
        fDraw2D -> Draw();

        Help("rqg");
        fStage = 6;
    }
}

void LKBeamPID::CalibrationRun()
{
    FitTotal(true);
}

void LKBeamPID::MakeSummary()
{
    e_title << "MakeSummary" << endl;
    if (fStage<2) {
        e_warning << "Must select file before creating summary!" << endl;
        return;
    }
    else if (fStage<5) {
        e_warning << "Must select points before creating summary!" << endl;
        return;
    }
    else if (fStage<6) {
        e_warning << "Must run fit total before creating summary!" << endl;
        return;
    }
    gSystem -> Exec("mkdir -p summary/");
    TString summaryName1 = Form("summary/run_%04d.root",fCurrentRunNumber);
    TString summaryName2 = Form("summary/run_%04d.txt",fCurrentRunNumber);
    e_info << "Summary files " << endl;
    for (TString summaryName : {summaryName1,summaryName2}) e_cout << "   " << summaryName << endl;
    for (TString summaryName : {summaryName2})
    {
        ofstream fileSummary(summaryName);
        fileSummary << "####################################################" << endl;
        fileSummary << left;
        fileSummary << setw(25) << "num_s" << fNumContours-1 << endl;
        for (auto bin=2; bin<=fNumContours; ++bin) {
            fileSummary << setw(25) << ("s_error_%d",bin-1) << fValueOfS[bin] << "  " << fErrorAtS[bin] << endl;
        }
        fileSummary << endl;
        auto numFits = fFitArray -> GetEntries();
        for (auto iFit=0; iFit<numFits; ++iFit)
        {
            auto total = fHistPID -> GetEntries();
            TH1D *histData = (TH1D*) fHistDataArray -> At(iFit);
            TH1D *histBack = (TH1D*) fHistBackArray -> At(iFit);
            auto bin = histData -> FindBin(fFinalContourAScale+0.5*(1./fNumContours));
            auto count = histData -> GetBinContent(bin);
            auto contamination = histBack -> GetBinContent(bin);
            auto corrected = count - contamination;
            auto fit = (TF2*) fFitArray -> At(iFit);
            auto amplit = fit->GetParameter(0);
            auto valueX = fit->GetParameter(1);
            auto sigmaX = fit->GetParameter(2);
            auto valueY = fit->GetParameter(3);
            auto sigmaY = fit->GetParameter(4);
            auto thetaR = fit->GetParameter(5);
            fileSummary << "####################################################" << endl;
            fileSummary << left;
            fileSummary << setw(25) << Form("pid%d/total",iFit)              << total << endl;
            fileSummary << setw(25) << Form("pid%d/ca_scale",iFit)           << fFinalContourAScale << endl;
            fileSummary << setw(25) << Form("pid%d/count",iFit)              << count << endl;
            fileSummary << setw(25) << Form("pid%d/contamination",iFit)      << contamination << endl;
            fileSummary << setw(25) << Form("pid%d/contamination_error",iFit)<< contamination*fFitCountDiff->Eval(fFinalContourAScale+0.05) << endl;
            fileSummary << setw(25) << Form("pid%d/corrected",iFit)          << corrected << endl;
            fileSummary << setw(25) << Form("pid%d/purity",iFit)             << corrected/total << endl;
            fileSummary << setw(25) << Form("pid%d/amplitude",iFit)          << amplit << endl;
            fileSummary << setw(25) << Form("pid%d/x(x_rf0)",iFit)           << valueX << endl;
            fileSummary << setw(25) << Form("pid%d/y(%s)",iFit,fYName.Data()) << valueY << endl;
            fileSummary << setw(25) << Form("pid%d/sigma_x",iFit)            << sigmaX << endl;
            fileSummary << setw(25) << Form("pid%d/sigma_y",iFit)            << sigmaY << endl;
            fileSummary << setw(25) << Form("pid%d/theta_deg",iFit)          << thetaR*TMath::RadToDeg() << endl;
            if (fFinalContourGraph!=nullptr) {
                auto numPoints = fFinalContourGraph -> GetN();
                fileSummary << setw(25) << Form("pid%d/contour_x  ",iFit);
                for (auto iPoint=0; iPoint<numPoints; ++iPoint) fileSummary << fFinalContourGraph -> GetPointX(iPoint) << ","; fileSummary << endl;
                fileSummary << setw(25) << Form("pid%d/contour_y  ",iFit);
                for (auto iPoint=0; iPoint<numPoints; ++iPoint) fileSummary << fFinalContourGraph -> GetPointY(iPoint) << ","; fileSummary << endl;
            }
            fileSummary << endl;
        }
    }

    {
        auto fileSummary1 = new TFile(summaryName1,"recreate");

        auto cvsPID = fDraw2D -> GetCanvas();
        auto cvsFit = fGroupFit -> GetCanvas();
        if (cvsPID!=nullptr||cvsFit!=nullptr) gSystem -> Exec("mkdir -p figures/");
        if (cvsPID!=nullptr) {
            cvsPID -> SaveAs(Form("figures/pid_%04d.png",fCurrentRunNumber));
            fileSummary1 -> cd();
            cvsPID -> Write("pid");
        }
        if (cvsFit!=nullptr) {
            cvsFit -> SaveAs(Form("figures/fit_%04d.png",fCurrentRunNumber));
            cvsFit -> Write("fit");
        }

        fTop -> Write("flat");
    }

    Help("ra");
    fStage = 7;
}

LKDrawing* LKBeamPID::GetFitTestDrawing(int idx, TH2D *hist, TF2* fit, TF2* fitContanminent, bool resetError)
{
    gStyle->SetPaintTextFormat(".3f");
    auto amplit = fit->GetParameter(0);
    auto valueX = fit->GetParameter(1);
    auto sigmaX = fit->GetParameter(2);
    auto valueY = fit->GetParameter(3);
    auto sigmaY = fit->GetParameter(4);
    auto thetaR = fit->GetParameter(5);
    TString nameData = fit -> GetName(); nameData.ReplaceAll("fit_","histIntegralData_");
    TString nameFitG = fit -> GetName(); nameFitG.ReplaceAll("fit_","histIntegralFitG_");
    TString nameTest = fit -> GetName(); nameTest.ReplaceAll("fit_","histIntegralTest_");
    TString nameBack = fit -> GetName(); nameBack.ReplaceAll("fit_","histIntegralBack_");
    TString nameCalc = fit -> GetName(); nameBack.ReplaceAll("fit_","histIntegralCalc_");
    TString nameErrr = fit -> GetName(); nameErrr.ReplaceAll("fit_","histIntegralErrr_");
    TString title = Form("[RUN %04d] (%d) Count in contour;S = Contour amplitude scale [Amp];Count",fCurrentRunNumber,idx);
    TH1D *histData = (TH1D*) fHistDataArray -> At(idx);
    TH1D *histFitG = (TH1D*) fHistFitGArray -> At(idx);
    TH1D *histBack = (TH1D*) fHistBackArray -> At(idx);
    TH1D *histCalc = (TH1D*) fHistCalcArray -> At(idx);
    TH1D *histTest = (TH1D*) fHistTestArray -> At(idx);
    //TH1D *histErrr = (TH1D*) fHistErrrArray -> At(idx);
    TH2D *histErrr = (TH2D*) fHistErrrArray -> At(0);
    if (histData==nullptr)
    {
        gROOT -> cd();
        histData = new TH1D(nameData,title,fNumContours,0,1);
        histFitG = new TH1D(nameFitG,title,fNumContours,0,1);
        histBack = new TH1D(nameBack,title,fNumContours,0,1);
        histCalc = new TH1D(nameCalc,title,fNumContours,0,1);
        histTest = new TH1D(nameTest,title,fNumContours,0,1);
        fHistDataArray -> Add(histData);
        fHistFitGArray -> Add(histFitG);
        fHistBackArray -> Add(histBack);
        fHistCalcArray -> Add(histCalc);
        fHistTestArray -> Add(histTest);
        histData -> SetFillColor(19);
        histData -> SetLineWidth(2);
        histData -> SetLineColor(kBlack);
        histData -> GetXaxis() -> SetTitleOffset(1.2);
        histFitG -> SetLineColor(kRed);
        histFitG -> SetLineWidth(2);
        histFitG -> SetLineStyle(2);
        histBack -> SetLineColor(kBlue);
        histBack -> SetLineWidth(2);
        histBack -> SetLineStyle(1);
        histCalc -> SetLineColor(kGreen);
        histCalc -> SetLineWidth(1);
        histCalc -> SetLineStyle(1);
    }
    else {
        histData -> Reset("ICES");
        histFitG -> Reset("ICES");
        histBack -> Reset("ICES");
        histCalc -> Reset("ICES");
        histTest -> Reset("ICES");
        histData -> SetTitle(title);
        histFitG -> SetTitle(title);
        histBack -> SetTitle(title);
        histCalc -> SetTitle(title);
        histTest -> SetTitle(title);
        histData -> SetName(nameData);
        histFitG -> SetName(nameFitG);
        histBack -> SetName(nameCalc);
        histCalc -> SetName(nameBack);
        histTest -> SetName(nameTest);
    }

    if (histErrr==nullptr) {
        histErrr = new TH2D(nameErrr,title,fNumContours,0,1,60,-0.3,0.3);
        fHistErrrArray -> Add(histErrr);
    }
    if (resetError) {
        histErrr -> Reset("ICES");
        TString title2 = Form("[RUN %04d] (%d);S = Contour amplitude [A];Error",fCurrentRunNumber,idx);
        histErrr -> SetTitle(title2);
        histErrr -> SetName(nameErrr);
    }

    auto draw = new LKDrawing();
    draw -> SetCanvasMargin(0.15,0.05,0.1,0.1);
    draw -> SetOptStat(0);
    draw -> SetAutoMax();
    draw -> SetCreateLegend();
    draw -> Add(histData,"hist","data");
    if (fitContanminent!=nullptr)
        draw -> Add(histBack,"same hist","contaminent");
    draw -> Add(histFitG,"same","fit");
    draw -> Add(histCalc,"same hist","fit+contam.");
    //draw -> Add(histTest,"same","test");
    //draw -> Add(histErrr,"drawx",".");
    draw -> AddLegendLine(Form("A=%.2f",amplit));
    draw -> AddLegendLine(Form("x=%.2f",valueX));
    draw -> AddLegendLine(Form("#sigma_{x}=%.2f",sigmaX));
    draw -> AddLegendLine(Form("y=%.2f",valueY));
    draw -> AddLegendLine(Form("#sigma_{y}=%.2f",sigmaY));
    draw -> AddLegendLine(Form("#theta=%.1f deg.",thetaR*TMath::RadToDeg()));
    double wx = hist -> GetXaxis() -> GetBinWidth(1);
    double wy = hist -> GetYaxis() -> GetBinWidth(1);
    double binA = wx*wy;
    double dc = (1./fNumContours);
    double countFullG = 1;
    for (double contourScale=0; contourScale<1; contourScale+=dc) {
        auto graphC = GetContourGraph(contourScale*amplit, amplit, valueX, sigmaX, valueY, sigmaY, thetaR);
        graphC -> SetName(Form("contourGraph_%.2f",contourScale));
        if (contourScale==fFinalContourAScale) fFinalContourGraph = graphC;
        double countFitG = Integral2DGaussian(fit, contourScale);
        countFitG = countFitG / binA;
        double countCalc = countFitG;
        double x_contour = contourScale+0.5*dc;
        histFitG -> SetBinContent(histFitG->GetXaxis()->FindBin(x_contour),countFitG/countFullG);
        double countTest = IntegralInsideGraph(hist, graphC, fit);
        countTest = countTest;
        histTest -> SetBinContent(histTest->GetXaxis()->FindBin(x_contour),countTest/countFullG);
        double countBack = 0;
        if (fitContanminent!=nullptr) {
            countBack = IntegralInsideGraph(hist, graphC, fitContanminent);
            countCalc = countCalc + countBack;
            histBack -> SetBinContent(histBack->GetXaxis()->FindBin(x_contour),countBack/countFullG);
        }
        histCalc -> SetBinContent(histCalc->GetXaxis()->FindBin(x_contour),countCalc/countFullG);
        if (contourScale!=0) {
            double countHist = IntegralInsideGraph(hist, graphC);
            histData -> SetBinContent(histData->GetXaxis()->FindBin(x_contour),countHist/countFullG);
            double signalRatio = countFitG/(countFitG+countBack);
            double diff = (countHist*signalRatio-countFitG)/(countHist*signalRatio);
            histErrr -> Fill(x_contour,diff);
        }
    }
    return draw;
}

TF2* LKBeamPID::Fit2DGaussian(TH2D *hist, int idx, double valueX, double valueY, double sigmaX, double sigmaY, double theta)
{
    if (sigmaX==0) sigmaX = fDefaultFitSigmaX;
    if (sigmaY==0) sigmaY = fDefaultFitSigmaY;
    if (theta==0) theta = fDefaultFitTheta;
    TF2 *fit = new TF2(Form("fit_%04d_%d", fCurrentRunNumber, idx), fFormulaRotated2DGaussian, valueX-fFitRangeInSigma*sigmaX,valueX+fFitRangeInSigma*sigmaX, valueY-fFitRangeInSigma*sigmaY,valueY+fFitRangeInSigma*sigmaY);
    double amplit = hist -> GetBinContent(hist->GetXaxis()->FindBin(valueX),hist->GetYaxis()->FindBin(valueY));
    fit -> SetParameter(0, amplit);
    fit -> SetParameter(1, valueX);
    fit -> SetParameter(3, valueY);
    fit -> SetParameter(2, sigmaX);
    fit -> SetParameter(4, sigmaY);
    fit -> SetParameter(5, theta*TMath::DegToRad());
    fit -> SetParLimits(0, amplit*0.5, amplit*2);
    fit -> SetParLimits(1, valueX-0.5*sigmaX, valueX+0.5*sigmaX);
    fit -> SetParLimits(3, valueY-0.5*sigmaY, valueY+0.5*sigmaY);
    fit -> SetParLimits(2, sigmaX*0.5, sigmaX*1.2);
    fit -> SetParLimits(4, sigmaY*0.5, sigmaY*1.2);
    fit -> SetParLimits(5, 0.*TMath::Pi(), 0.25*TMath::Pi());
    fit -> SetLineColor(kRed);
    hist -> Fit(fit,"QBR0");
    fit -> SetContour(3);
    e_cout << "   " << fit -> GetName() << ": " << std::left
        << setw(16) << Form("amp=%f,", fit->GetParameter(0))
        << setw(28) << Form("x=(%f, %f),", fit->GetParameter(1), fit->GetParameter(2))
        << setw(28) << Form("y=(%f, %f),", fit->GetParameter(3), fit->GetParameter(4))
        << setw(18) << Form("theta=%f,", fit->GetParameter(5)*TMath::RadToDeg())
        << endl;
    return fit;
}

TGraph *LKBeamPID::GetContourGraph(double contourAmp, double amplit, double valueX, double sigmaX, double valueY, double sigmaY, double thetaR)
{
    double Rx = sigmaX * sqrt(-2 * log(contourAmp / amplit));
    double Ry = sigmaY * sqrt(-2 * log(contourAmp / amplit));

    auto graph = new TGraph();
    graph -> SetMarkerStyle(20);

    for (double theta=0; theta<=2*TMath::Pi(); theta+=0.1) {
        double pointX = valueX + Rx * cos(theta);
        double pointY = valueY + Ry * sin(theta);
        TVector3 point(pointX-valueX,pointY-valueY,0);
        point.RotateZ(thetaR);
        graph -> SetPoint(graph->GetN(),point.X()+valueX,point.Y()+valueY);
    }
    graph -> SetPoint(graph->GetN(),graph->GetPointX(0),graph->GetPointY(0));
    return graph;
}

double LKBeamPID::IntegralInsideGraph(TH2D* hist, TGraph* graph, bool justCount)
{
    double integral = 0;
    int nx = hist -> GetXaxis() -> GetNbins();
    int ny = hist -> GetYaxis() -> GetNbins();
    double wx = hist -> GetXaxis() -> GetBinWidth(1);
    double wy = hist -> GetYaxis() -> GetBinWidth(1);
    double binA = wx*wy;
    if (justCount) binA = 1;
    for (auto xbin=1; xbin<=nx; ++xbin) {
        double xvalue = hist -> GetXaxis() -> GetBinCenter(xbin);
        for (auto ybin=1; ybin<=ny; ++ybin) {
            double yvalue = hist -> GetYaxis() -> GetBinCenter(ybin);
            if (graph -> IsInside(xvalue,yvalue)) {
                double value = hist -> GetBinContent(xbin,ybin);
                integral += value*binA;
            }
        }
    }
    return integral;
}

double LKBeamPID::IntegralInsideGraph(TH2D* hist, TGraph* graph, TF2 *f2, bool justCount)
{
    double integral = 0;
    int nx = hist -> GetXaxis() -> GetNbins();
    int ny = hist -> GetYaxis() -> GetNbins();
    double wx = hist -> GetXaxis() -> GetBinWidth(1);
    double wy = hist -> GetYaxis() -> GetBinWidth(1);
    double binA = wx*wy;
    if (justCount) binA = 1;
    for (auto xbin=1; xbin<=nx; ++xbin) {
        double xvalue = hist -> GetXaxis() -> GetBinCenter(xbin);
        for (auto ybin=1; ybin<=ny; ++ybin) {
            double yvalue = hist -> GetYaxis() -> GetBinCenter(ybin);
            if (graph -> IsInside(xvalue,yvalue)) {
                double value = f2 -> Eval(xvalue,yvalue);
                integral += value*binA;
            }
        }
    }
    return integral;
}

double LKBeamPID::Integral2DGaussian(double amplitude, double sigma_x, double sigma_y, double contourS)
{
    double value = amplitude*2*TMath::Pi()*sigma_x*sigma_y*(1-contourS);
    value = value * (1+fFitCountDiff->Eval(contourS+0.05));
    return value;
}

double LKBeamPID::Integral2DGaussian(TF2 *f2, double contourS)
{
    double value = Integral2DGaussian(f2->GetParameter(0), f2->GetParameter(2), f2->GetParameter(4), contourS);
    return value;
}

void LKBeamPID::CollectRootFiles(std::vector<TString> &listGenFile, TString dataPath, TString format)
{
    if (dataPath.IsNull()) dataPath = fDefaultPath;
    if (format.IsNull()) format = fDefaultFormat;

    if (dataPath.Index("~/")==0) {
        dataPath.Remove(0,1);
        dataPath = TString(gSystem->Getenv("HOME")) + dataPath;
    }


    e_info << "Looking for data files(" << format << ") in " << dataPath << endl;
    void *dirp = gSystem->OpenDirectory(dataPath);
    if (!dirp) {
        e_warning << dataPath << " do not exist!" << endl;
        return;
    }

    const char *entry;
    while ((entry = gSystem->GetDirEntry(dirp))) {
        TString fileName = entry;
        if (fileName.Index(".")==0 || fileName == "..") continue;
        TString fullPath = TString(dataPath) + "/" + fileName;
        fullPath.ReplaceAll("//","/");
        if (gSystem->AccessPathName(fullPath, kFileExists)) continue;
        if (gSystem->OpenDirectory(fullPath)) {
            CollectRootFiles(listGenFile,fullPath, format);
        } else {
            if (fileName.EndsWith(format))
                listGenFile.push_back(fullPath);
        }
    }
    gSystem->FreeDirectory(dirp);
}

void LKBeamPID::SetRunNumber(int run)
{
    fCurrentRunNumber = run;
}

void LKBeamPID::SetGausFitRange(double sigDist)
{
    if (sigDist<0) {
        e_note << "Enter fit range from 0 to 5 (1 default): ";
        TString inputString;
        cin >> inputString;
        inputString = inputString.Strip(TString::kBoth);
        sigDist = inputString.Atof();
    }
    fFitRangeInSigma = sigDist;
    e_cout << "   " << fFitRangeInSigma << endl;
}

void LKBeamPID::SetSValue(double scale)
{
    if (scale<0) {
        e_note << "Enter fit range from 0.1 to 0.9 (1 default): ";
        TString inputString;
        cin >> inputString;
        inputString = inputString.Strip(TString::kBoth);
        scale = inputString.Atof();
    }
    fFinalContourAScale = scale;
    e_cout << "   " << fFinalContourAScale << endl;
}

void LKBeamPID::SaveConfiguration()
{
    TString sListString; for (auto s : fContourScaleList) sListString = sListString + LKMisc::RemoveTrailing0(s) + ", "; sListString.Remove(sListString.Sizeof()-2);
    TString bnnString = Form("%d,%f,%f, %d,%f,%f",fBnn1.nx(), fBnn1.x1(), fBnn1.x2(), fBnn1.ny(), fBnn1.y1(), fBnn1.y2());
    TString xName = fXName.IsNull()?".":fXName;
    TString yName = fYName.IsNull()?".":fYName;
    LKParameterContainer par;
    par.AddPar("fit_range"             ,fFitRangeInSigma,    "fit range in unit of sigma");
    par.AddPar("x_name"                ,xName,               "x value name in tree");
    par.AddPar("y_name"                ,yName,               "y value name in tree");
    par.AddPar("num_contours"          ,fNumContours,        "number of contours for integral test");
    par.AddPar("binning"               ,bnnString,           "default x(3),y(3) binning for pid plot");
    par.AddPar("cut_s_value"           ,fFinalContourAScale, "s-value (ratio compared to the gaussian amplitude) for drawing pid cut contour");
    par.AddPar("example_s_list"        ,sListString,         "list of s-value for contours in the pid pid. cut_s_value is automatically added to the list.");
    par.AddPar("data_path"             ,fDefaultPath,        "path to look for the files");
    par.AddPar("file_format"           ,fDefaultFormat,      "function will search files which end with file_format");
    par.AddPar("fit_sigma_x"           ,fDefaultFitSigmaX,   "default initial sigma_x value");
    par.AddPar("fit_sigma_y"           ,fDefaultFitSigmaY,   "default initial sigma_y value");
    par.AddPar("fit_theta"             ,fDefaultFitTheta,    "default initial theta value");
    par.AddPar("fit_amp_ratio_range"   ,fAmpRatioRange,      "amplitude (+-) range in ratio");
    par.AddPar("fit_pos_ratio_range"   ,fPosRatioRangeInSig, "position (+-) range in ratio");
    par.AddPar("fit_sigma_ratio_range" ,fSigmaRatioRange,    "sigma (+-) range in ratio");
    par.AddPar("fit_theta_range"       ,fThetaRange,         "theta (+-) range");
    par.SaveAs("config.mac");
}

void LKBeamPID::PrintBinning() { e_cout << "== Current binning: " << fBnn1.Print(false) << endl; }
void LKBeamPID::ResetBinning() { fBnn1 = fBnn0; CreateAndFillHistogram(1); }
void LKBeamPID::SetBinningX(int nx, double x1, double x2, int fill) { fBnn1.SetXNMM(nx,x1,x2); if (fill) CreateAndFillHistogram(1); }
void LKBeamPID::SetBinningY(int ny, double y1, double y2, int fill) { fBnn1.SetYNMM(ny,y1,y2); if (fill) CreateAndFillHistogram(1); }
void LKBeamPID::SetRangeX(double x1, double x2, int fill) { fBnn1.SetXMM(x1,x2); if (fill) CreateAndFillHistogram(1); }
void LKBeamPID::SetRangeY(double y1, double y2, int fill) { fBnn1.SetYMM(y1,y2); if (fill) CreateAndFillHistogram(1); }
void LKBeamPID::SetBinning(int nx, double x1, double x2, int ny, double y1, double y2) { SetBinningX(nx,x1,x2,0); SetBinningY(ny,y1,y2,0); CreateAndFillHistogram(1); }
void LKBeamPID::SetBinning(double x1, double x2, double y1, double y2) { SetRangeX(x1,x2,0); SetRangeY(y1,y2,0); CreateAndFillHistogram(1); }
void LKBeamPID::SetXBinSize(double w, int fill) { fBnn1.SetWX(w); if (fill) CreateAndFillHistogram(1); }
void LKBeamPID::SetYBinSize(double w, int fill) { fBnn1.SetWY(w); if (fill) CreateAndFillHistogram(1); }
void LKBeamPID::SetBinNX(double n, int fill) { fBnn1.SetNX(n); if (fill) CreateAndFillHistogram(1); }
void LKBeamPID::SetBinNY(double n, int fill) { fBnn1.SetNY(n); if (fill) CreateAndFillHistogram(1); }
void LKBeamPID::SaveBinning() {
    if (!fHistPID) return;
    fBnn1.SetBinning(fHistPID,true);
    CreateAndFillHistogram(1);
}
