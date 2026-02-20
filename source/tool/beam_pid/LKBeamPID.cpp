#include "LKBeamPID.h"

ClassImp(LKBeamPID)

LKBeamPID::LKBeamPID()
{
    fStage = 0;
    gStyle -> SetNumberContours(100);
    ResetBinning();
    fPIDFitArray = new TObjArray();
    fHistDataArray = new TObjArray();
    fHistFitGArray = new TObjArray();
    fHistBackArray = new TObjArray();
    fHistCalcArray = new TObjArray();
    fHistTestArray = new TObjArray();
    fHistErrrArray = new TObjArray();
    fTop = new LKDrawingGroup();
    fGroupFit = fTop -> CreateGroup(Form("event_fit_%04d",fCurrentRunNumber));
    fGroupPID = fTop -> CreateGroup(Form("event_pid_%04d",fCurrentRunNumber));
    fDraw2D = fGroupPID -> CreateDrawing(Form("draw_2d_%04d",fCurrentRunNumber));
    fDraw2D -> SetCanvasSize(1,1,1);
    fDraw2D -> SetOptStat(10);
    fDraw2D -> SetCanvasMargin(.11,.15,.11,0.08);
    fFitCountDiff = new TF1("fitdiff","pol2",0,1);
    fFitCountDiff -> SetParameters(0,0);
    fFitCountDiff -> SetLineColor(kBlue);
    fFitCountDiff -> SetLineStyle(1);
    fFitCountDiff2 = new TF1("fitdiff2","pol2",0,1);
    fFitCountDiff2 -> SetParameters(0,0);
    fFitCountDiff2 -> SetLineWidth(1);
    fFitCountDiff2 -> SetLineStyle(2);
    fFitCountDiff3 = new TF1("fitdiff3","[0]",0,1);
    fFitCountDiff3 -> SetParameter(0,0.1);
    fFitCountDiff3 -> SetLineWidth(1);
    fFitCountDiff3 -> SetLineStyle(3);

    InitParameters();
    //fGroupFit -> Draw();
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
    par.UpdatePar(fSelectedEta,        "eta");
    par.UpdatePar(fDefaultPath,        "data_path");
    par.UpdatePar(fDefaultFormat,      "file_format");
    par.UpdatePar(fDefaultFitSigmaX,   "init_sigma_x");
    par.UpdatePar(fDefaultFitSigmaY,   "init_sigma_y");
    par.UpdatePar(fDefaultFitTheta,    "init_theta");
    par.UpdatePar(fFixSigmaX,          "fix_sigma_x");
    par.UpdatePar(fFixSigmaY,          "fix_sigma_y");
    par.UpdatePar(fFixThetaR,          "fix_theta");
    par.UpdatePar(fAmpRatioRange,      "fit_amp_ratio_range");
    par.UpdatePar(fPosRatioRangeInSig, "fit_pos_ratio_range");
    par.UpdatePar(fSigmaRatioRange,    "fit_sigma_ratio_range");
    par.UpdatePar(fThetaRange,         "fit_theta_range");
    if (par.CheckPar("contour_color")) fContourColor = par.GetParColor("contour_color");
    if (par.CheckPar("text_color_dark")) fPIDIndexTextColor1 = par.GetParColor("text_color_dark");
    if (par.CheckPar("text_color_bright")) fPIDIndexTextColor2 = par.GetParColor("text_color_bright");
    par.UpdatePar(fLegendFillStyle,    "legend_fill_style");
    fCompareEtaList = par.InitPar(fCompareEtaList, "compare_eta_list");
    fDrawEtaList = par.InitPar(fDrawEtaList, "draw_eta_list");
    par.Print();

    fBnn1 = fBnn0;

    for (auto bin=0; bin<=fNumContours; ++bin) {
        fValueOfS.push_back(0);
        fErrorAtS.push_back(0);
    }

    if (fCompareEtaList.size()==0) {
        if (fSelectedEta+0.05<1) fCompareEtaList.push_back(fSelectedEta+0.05);
        if (fSelectedEta-0.05>0) fCompareEtaList.push_back(fSelectedEta-0.05);
    }

    InitBeamPIDValues(10);
}

void LKBeamPID::InitBeamPIDValues(int numPIDs)
{
    int sizePrevious = fBeamPIDList.size();
    for (auto iPID=0; iPID<numPIDs; ++iPID)
    {
        if (iPID>=sizePrevious) {
            vector<double> beamPIDValues;
            beamPIDValues.push_back(0); // 0 total
            beamPIDValues.push_back(0); // 1 count svalue
            beamPIDValues.push_back(0); // 2 count data
            beamPIDValues.push_back(0); // 3 count fit
            beamPIDValues.push_back(0); // 4 contamination count
            beamPIDValues.push_back(0); // 5 contamination error
            beamPIDValues.push_back(0); // 6 corrected count
            beamPIDValues.push_back(0); // 7 purity (contour_count/total)
            beamPIDValues.push_back(0); // 8 purity (bg_subtracted_count/total)
            beamPIDValues.push_back(0); // 9 overall error
            fBeamPIDList.push_back(beamPIDValues);

            vector<double> fittingValues;
            fittingValues.push_back(0); // 0 amplitude
            fittingValues.push_back(0); // 1 valueX
            fittingValues.push_back(0); // 2 valueY
            fittingValues.push_back(0); // 3 sigmaX
            fittingValues.push_back(0); // 4 sigmaY
            fittingValues.push_back(0); // 5 theta
            fFittingList.push_back(fittingValues);

            vector<double> compareValues;
            compareValues.push_back(fCompareEtaList[0]); // 0 compare-1 svalue
            compareValues.push_back(0); // 1 compare-1 count
            compareValues.push_back(0); // 2 compare-1 systematic
            compareValues.push_back(fCompareEtaList[1]); // 3 compare-2 svalue
            compareValues.push_back(0); // 4 compare-2 count
            compareValues.push_back(0); // 5 compare-2 systematic
            fCompareList.push_back(compareValues);
        }
        else {

            fBeamPIDList[iPID][0] = 0; // 0 total
            fBeamPIDList[iPID][1] = 0; // 1 count svalue
            fBeamPIDList[iPID][2] = 0; // 2 count data
            fBeamPIDList[iPID][3] = 0; // 3 count fit
            fBeamPIDList[iPID][4] = 0; // 4 contamination count
            fBeamPIDList[iPID][5] = 0; // 5 contamination error
            fBeamPIDList[iPID][6] = 0; // 6 corrected count
            fBeamPIDList[iPID][7] = 0; // 7 purity (contour_count/total)
            fBeamPIDList[iPID][8] = 0; // 8 purity (bg_subtracted_count/total)
            fBeamPIDList[iPID][9] = 0; // 9 overall error
            fFittingList[iPID][0] = 0; // 0 amplitude
            fFittingList[iPID][1] = 0; // 1 valueX
            fFittingList[iPID][2] = 0; // 2 valueY
            fFittingList[iPID][3] = 0; // 3 sigmaX
            fFittingList[iPID][4] = 0; // 4 sigmaY
            fFittingList[iPID][5] = 0; // 5 theta
            fCompareList[iPID][0] = fCompareEtaList[0]; // 1 compare-1 svalue
            fCompareList[iPID][1] = 0; // 1 compare-1 count
            fCompareList[iPID][2] = 0; // 2 compare-1 systematic
            fCompareList[iPID][3] = fCompareEtaList[1]; // 3 compare-2 svalue
            fCompareList[iPID][4] = 0; // 4 compare-2 count
            fCompareList[iPID][5] = 0; // 5 compare-2 systematic
        }
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
    if (fSetYName==".") {
        if      (fCurrentFileName.Index("chkf2run")>=0) { fCurrentType = 2; fYName = "f2ssde"; }
        else if (fCurrentFileName.Index("chkf3run")>=0) { fCurrentType = 3; fYName = "f3ssde"; }
    }
    else
        fYName = fSetYName;

    if (fSetXName==".")
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
        fDraw2D -> SetStatsFillStyle(fLegendFillStyle);
        fDraw2D -> Print(); // XXX
        fStage = 3;
        fBeamPIDList[0][0] = fHistPID->GetEntries();
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
    fBeamPIDList[0][0] = fHistPID->GetEntries();
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
        InitBeamPIDValues(numPoints);
        for (auto iPID=0; iPID<numPoints; ++iPID)
        {
            double x = graph -> GetPointX(iPID);
            double y = graph -> GetPointY(iPID);
            e_cout << "   " << iPID << " " << x << " " << y << endl;
            points.push_back(vector<double>{x,y});
        }
    }

    auto numPoints = points.size();
    for (auto iPID=0; iPID<numPoints; ++iPID)
        fBeamPIDList[iPID][0] = fHistPID->GetEntries();

    fGroupFit -> Clear();
    double wx = fHistPID -> GetXaxis() -> GetBinWidth(1);
    double wy = fHistPID -> GetYaxis() -> GetBinWidth(1);
    double binArea = wx*wy;

    e_info << "Total " << points.size() << " pid centers are selected" << endl;

    fPIDFitArray -> Clear();
    for (auto iPID=0; iPID<numPoints; ++iPID)
    {
        auto point = points[iPID];
        auto fit = Fit2DGaussian(fHistPID, iPID, point[0], point[1]);
        fPIDFitArray -> Add(fit);
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
            auto valueY = fit->GetParameter(2);
            auto sigmaX = fit->GetParameter(3);
            auto sigmaY = fit->GetParameter(4);
            auto thetaR = fit->GetParameter(5);
            for (double sValue : fDrawEtaList) {
                auto graphC1 = GetContourGraph(sValue*amplit, amplit, valueX, sigmaX, valueY, sigmaY, thetaR);
                graphC1 -> SetName(Form("graphC1_%d_S%s",iPID,LKMisc::RemoveTrailing0(100*sValue,1).Data()));
                graphC1 -> SetLineColor(fContourColor);
                graphC1 -> SetLineStyle(9);
                fDraw2D -> Add(graphC1,"samel");
            }
            auto graphC0 = GetContourGraph(fSelectedEta*amplit, amplit, valueX, sigmaX, valueY, sigmaY, thetaR);
            graphC0 -> SetName(Form("graphC0_%d_S%s",iPID,LKMisc::RemoveTrailing0(100*fSelectedEta,1).Data()));
            graphC0 -> SetLineColor(fContourColor);
            fDraw2D -> Add(graphC0,"samel");
            for (double sValue : fCompareEtaList) {
                auto graphC2 = GetContourGraph(sValue*amplit, amplit, valueX, sigmaX, valueY, sigmaY, thetaR);
                graphC2 -> SetName(Form("graphC2_%d_S%s",iPID,LKMisc::RemoveTrailing0(100*sValue,1).Data()));
                graphC2 -> SetLineColor(kGreen);
                graphC2 -> SetLineStyle(9);
                //fDraw2D -> Add(graphC2,"samel");
            }
            fFittingList[iPID][0] = amplit; // 0 amplitude
            fFittingList[iPID][1] = valueX; // 1 valueX
            fFittingList[iPID][2] = sigmaX; // 2 valueY
            fFittingList[iPID][3] = valueY; // 3 sigmaX
            fFittingList[iPID][4] = sigmaY; // 4 sigmaY
            fFittingList[iPID][5] = thetaR; // 5 theta
        }
    }
    //fGroupFit -> Draw();
    fDraw2D -> Draw();

    Help("rqf");

    fStage = 5;
    return;
}

void LKBeamPID::CalibrateParFast()
{
    fGroupFit -> Clear();
    double sigmaX1 = DBL_MAX;
    double sigmaX2 = 0;
    double sigmaY1 = DBL_MAX;
    double sigmaY2 = 0;
    double thetaR1 = DBL_MAX;
    double thetaR2 = 0;
    auto numPIDs = fPIDFitArray -> GetEntries();
    for (auto iPID=0; iPID<numPIDs; ++iPID)
    {
        auto fit = (TF2*) fPIDFitArray -> At(iPID);
        auto amplit = fit->GetParameter(0);
        auto valueX = fit->GetParameter(1);
        auto valueY = fit->GetParameter(2);
        auto sigmaX = fit->GetParameter(3);
        auto sigmaY = fit->GetParameter(4);
        auto thetaR = fit->GetParameter(5);
        if (sigmaX<sigmaX1) sigmaX1 = sigmaX;
        if (sigmaX>sigmaX2) sigmaX2 = sigmaX;
        if (sigmaY<sigmaY1) sigmaY1 = sigmaY;
        if (sigmaY>sigmaY2) sigmaY2 = sigmaY;
        if (thetaR<thetaR1) thetaR1 = thetaR;
        if (thetaR>thetaR2) thetaR2 = thetaR;
    }
    auto hist_sigmaX = new TH1D(Form("sigmaX_%04d_%d",fCurrentRunNumber,fFrameIndex++),"",20,0,1.2*sigmaX2);
    auto hist_sigmaY = new TH1D(Form("sigmaY_%04d_%d",fCurrentRunNumber,fFrameIndex++),"",20,0,1.2*sigmaY2);
    auto hist_thetaR = new TH1D(Form("thetaR_%04d_%d",fCurrentRunNumber,fFrameIndex++),"",20,0,1.2*thetaR2);
    auto hist_sigmaX2 = new TH2D(Form("sigmaX2_%04d_%d",fCurrentRunNumber,fFrameIndex++),"",20,fBnn1.y1(),fBnn1.y2(),20,0,1.2*sigmaX2);
    auto hist_sigmaY2 = new TH2D(Form("sigmaY2_%04d_%d",fCurrentRunNumber,fFrameIndex++),"",20,fBnn1.y1(),fBnn1.y2(),20,0,1.2*sigmaY2);
    auto hist_thetaR2 = new TH2D(Form("thetaR2_%04d_%d",fCurrentRunNumber,fFrameIndex++),"",20,fBnn1.y1(),fBnn1.y2(),20,0,1.2*thetaR2);
    fGroupFit -> AddHist(hist_sigmaX);
    fGroupFit -> AddHist(hist_sigmaY);
    fGroupFit -> AddHist(hist_thetaR);
    fGroupFit -> AddHist(hist_sigmaX2);
    fGroupFit -> AddHist(hist_sigmaY2);
    fGroupFit -> AddHist(hist_thetaR2);
    for (auto iPID=0; iPID<numPIDs; ++iPID)
    {
        auto fit = (TF2*) fPIDFitArray -> At(iPID);
        auto amplit = fit->GetParameter(0);
        auto valueX = fit->GetParameter(1);
        auto valueY = fit->GetParameter(2);
        auto sigmaX = fit->GetParameter(3);
        auto sigmaY = fit->GetParameter(4);
        auto thetaR = fit->GetParameter(5);
        hist_sigmaX -> Fill(sigmaX);
        hist_sigmaY -> Fill(sigmaY);
        hist_thetaR -> Fill(thetaR);
        hist_sigmaX2 -> Fill(valueY,sigmaX);
        hist_sigmaY2 -> Fill(valueY,sigmaY);
        hist_thetaR2 -> Fill(valueY,thetaR);
    }

    {
        fFixSigmaX = hist_sigmaX -> GetMean();
        fFixSigmaY = hist_sigmaY -> GetMean();
        fFixThetaR = hist_thetaR -> GetMean();
        //lk_debug << "<sigmaX> = " << fFixSigmaX << endl;
        //lk_debug << "<sigmaY> = " << fFixSigmaY << endl;
        //lk_debug << "<thetaR> = " << fFixThetaR << endl;

        if (fGraphTTOutTTIn==nullptr) {
            fGraphTTOutTTIn = new TGraph();
            fGraphTTOutTTIn -> SetMarkerStyle(20);
            fGraphTTOutTTIn -> SetMarkerSize(0.5);
        }
        else {
            fGraphTTOutTTIn -> Clear();
            fGraphTTOutTTIn -> Set(0);
        }
        auto drawTT = fGroupFit -> CreateDrawing();
        drawTT -> Add(fGraphTTOutTTIn,"pl");
        drawTT -> SetCreateFrame(Form("tt_%d",fFrameIndex++),";theta_{out};theta_{in}");
        for (double theta=-TMath::Pi(); theta<TMath::Pi(); theta+=0.1) {
            double Rx = fFixSigmaX * sqrt(-2 * log(fSelectedEta));
            double Ry = fFixSigmaY * sqrt(-2 * log(fSelectedEta));
            double pointX = Rx * cos(theta);
            double pointY = Ry * sin(theta);
            TVector3 point(pointX,pointY,0);
            fGraphTTOutTTIn -> SetPoint(fGraphTTOutTTIn->GetN(),point.Phi(),theta);
        }
    }
    //fGroupFit -> Draw();
}

void LKBeamPID::FitTotal(int mode)
{
    bool calibratePar = (mode==1);
    bool calibrateCnt = (mode==2);
    bool calibrateEta = (mode==3);

    if (calibratePar) e_title << "Parameter Calibration Run" << endl;
    else if (calibrateCnt) e_title << "Count CalibrationRun" << endl;
    else if (calibrateEta) e_title << "Eta CalibrationRun" << endl;
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
    auto numPIDs = fPIDFitArray -> GetEntries();
    TString formulaTotal;
    double xx1=DBL_MAX, xx2=-DBL_MAX, yy1=DBL_MAX, yy2=-DBL_MAX;
    for (auto iPID=0; iPID<numPIDs; ++iPID)
    {
        TString formulaCurrent = fFormulaRotated2DGaussian.Data();
        formulaCurrent.ReplaceAll("[0]",Form("[%d]",0+iPID*6));
        formulaCurrent.ReplaceAll("[1]",Form("[%d]",1+iPID*6));
        formulaCurrent.ReplaceAll("[2]",Form("[%d]",2+iPID*6));
        formulaCurrent.ReplaceAll("[3]",Form("[%d]",3+iPID*6));
        formulaCurrent.ReplaceAll("[4]",Form("[%d]",4+iPID*6));
        formulaCurrent.ReplaceAll("[5]",Form("[%d]",5+iPID*6));
        if (iPID==0) formulaTotal = formulaCurrent;
        else formulaTotal = formulaTotal + " + " + formulaCurrent;
        auto fit = (TF2*) fPIDFitArray -> At(iPID);
        auto amplit = fit->GetParameter(0);
        auto valueX = fit->GetParameter(1);
        auto valueY = fit->GetParameter(2);
        auto sigmaX = fit->GetParameter(3);
        auto sigmaY = fit->GetParameter(4);
        auto thetaR = fit->GetParameter(5);
        auto x1 = valueX-fFitRangeInSigma*sigmaX; if (xx1>x1) xx1 = x1;
        auto x2 = valueX+fFitRangeInSigma*sigmaX; if (xx2<x2) xx2 = x2;
        auto y1 = valueY-fFitRangeInSigma*sigmaY; if (yy1>y1) yy1 = y1;
        auto y2 = valueY+fFitRangeInSigma*sigmaY; if (yy2<y2) yy2 = y2;
    }

    auto fitContanminent = new TF2(Form("fitContanminent_%04d", fCurrentRunNumber), formulaTotal, xx1, xx2, yy1, yy2);
    auto fitTotal = new TF2(Form("fitTotal_%04d", fCurrentRunNumber), formulaTotal, xx1, xx2, yy1, yy2);
    fFitTotal = fitTotal;
    fitTotal -> SetLineColor(kMagenta);
    fitTotal -> SetContour(3);
    for (auto iPID=0; iPID<numPIDs; ++iPID)
    {
        auto fit = (TF2*) fPIDFitArray -> At(iPID);
        auto amplit = fit->GetParameter(0);
        auto valueX = fit->GetParameter(1);
        auto valueY = fit->GetParameter(2);
        auto sigmaX = fit->GetParameter(3);
        auto sigmaY = fit->GetParameter(4);
        auto thetaR = fit->GetParameter(5);
        fitTotal -> SetParameter(0+iPID*6, amplit);
        fitTotal -> SetParameter(1+iPID*6, valueX);
        fitTotal -> SetParameter(2+iPID*6, valueY);
        fitTotal -> SetParameter(3+iPID*6, sigmaX);
        fitTotal -> SetParameter(4+iPID*6, sigmaY);
        fitTotal -> SetParameter(5+iPID*6, thetaR);
        fitTotal -> SetParLimits(0+iPID*6, amplit*(1.-fAmpRatioRange), amplit*(1.+fAmpRatioRange));
        fitTotal -> SetParLimits(1+iPID*6, valueX-fPosRatioRangeInSig*sigmaX, valueX+fPosRatioRangeInSig*sigmaX);
        fitTotal -> SetParLimits(2+iPID*6, valueY-fPosRatioRangeInSig*sigmaY, valueY+fPosRatioRangeInSig*sigmaY);
        fitTotal -> SetParLimits(3+iPID*6, sigmaX*(1.-fSigmaRatioRange), sigmaX*(1.+fSigmaRatioRange));
        fitTotal -> SetParLimits(4+iPID*6, sigmaY*(1.-fSigmaRatioRange), sigmaY*(1.+fSigmaRatioRange));
        fitTotal -> SetParLimits(5+iPID*6, thetaR-fThetaRange, thetaR+fThetaRange);
        if (0) {
            for (auto ipar : {0,1,2,3,4,5}) {
                double parval = fit -> GetParameter(ipar);
                double parmin, parmax;
                fit -> GetParLimits(ipar, parmin, parmax);
            }
        }
        if (calibratePar==false) {
            if (fFixSigmaX>=0) { fitTotal -> SetParameter(3+iPID*6, fFixSigmaX); fitTotal -> FixParameter(3+iPID*6, fFixSigmaX); }
            if (fFixSigmaY>=0) { fitTotal -> SetParameter(4+iPID*6, fFixSigmaY); fitTotal -> FixParameter(4+iPID*6, fFixSigmaY); }
            if (fFixThetaR>=0) { fitTotal -> SetParameter(5+iPID*6, fFixThetaR); fitTotal -> FixParameter(5+iPID*6, fFixThetaR); }
            fCalibratedPar = true;
        }

        fFittingList[iPID][0] = amplit; // 0 amplitude
        fFittingList[iPID][1] = valueX; // 1 valueX
        fFittingList[iPID][2] = sigmaX; // 2 valueY
        fFittingList[iPID][3] = valueY; // 3 sigmaX
        fFittingList[iPID][4] = sigmaY; // 4 sigmaY
        fFittingList[iPID][5] = thetaR; // 5 theta
    }
    e_info << "Fitting " << numPIDs << " PIDs in " << Form("x=(%f,%f), y=(%f,%f) ...",xx1,xx2,yy1,yy2) << endl;
    fHistPID -> Fit(fitTotal,"QBR0");

    double sigmaX1 = DBL_MAX;
    double sigmaX2 = 0;
    double sigmaY1 = DBL_MAX;
    double sigmaY2 = 0;
    double thetaR1 = DBL_MAX;
    double thetaR2 = 0;

    auto legend = new TLegend();
    legend -> SetFillStyle(3001);
    legend -> SetMargin(0.1);
    legend -> AddEntry((TObject*)nullptr,Form("#eta = %.2f",fSelectedEta),"");
    for (auto iPID=0; iPID<numPIDs; ++iPID)
    {
        auto fit = (TF2*) fPIDFitArray -> At(iPID);
        auto amplit = fitTotal->GetParameter(0+iPID*6);
        auto valueX = fitTotal->GetParameter(1+iPID*6);
        auto valueY = fitTotal->GetParameter(2+iPID*6);
        auto sigmaX = fitTotal->GetParameter(3+iPID*6);
        auto sigmaY = fitTotal->GetParameter(4+iPID*6);
        auto thetaR = fitTotal->GetParameter(5+iPID*6);
        fit -> SetParameter(0,amplit);
        fit -> SetParameter(1,valueX);
        fit -> SetParameter(2,valueY);
        fit -> SetParameter(3,sigmaX);
        fit -> SetParameter(4,sigmaY);
        fit -> SetParameter(5,thetaR);
        fit -> SetLineColor(kMagenta);
        e_cout << "   " << fit -> GetName() << ": " << std::left
            << setw(16) << Form("amp=%f,", amplit)
            << setw(28) << Form("x=(%f, %f),", valueX, sigmaX)
            << setw(28) << Form("y=(%f, %f),", valueY, sigmaY)
            << setw(18) << Form("theta=%f", thetaR*TMath::RadToDeg())
            << endl;
        if (sigmaX<sigmaX1) sigmaX1 = sigmaX;
        if (sigmaX>sigmaX2) sigmaX2 = sigmaX;
        if (sigmaY<sigmaY1) sigmaY1 = sigmaY;
        if (sigmaY>sigmaY2) sigmaY2 = sigmaY;
        if (thetaR<thetaR1) thetaR1 = thetaR;
        if (thetaR>thetaR2) thetaR2 = thetaR;
        for (double sValue : fDrawEtaList) {
            auto graphC1 = GetContourGraph(sValue*amplit, amplit, valueX, sigmaX, valueY, sigmaY, thetaR);
            graphC1 -> SetLineColor(fContourColor);
            graphC1 -> SetLineStyle(9);
            fDraw2D -> Add(graphC1,"samel");
        }
        auto graphC0 = GetContourGraph(fSelectedEta*amplit, amplit, valueX, sigmaX, valueY, sigmaY, thetaR);
        graphC0 -> SetLineColor(fContourColor);
        fDraw2D -> Add(graphC0,"samel");
        auto text = new TLatex(valueX,valueY,Form("%d",iPID));
        text -> SetTextAlign(22);
        text -> SetTextSize(0.02);
        if (amplit<fHistPID->GetMaximum()*0.3)
            text -> SetTextColor(fPIDIndexTextColor2);
        else
            text -> SetTextColor(fPIDIndexTextColor1);
        fDraw2D -> Add(text,"same");
        for (auto iPar=0; iPar<fitTotal->GetNpar(); ++iPar)
            fitContanminent->SetParameter(iPar,fitTotal->GetParameter(iPar));
        fitContanminent->SetParameter(0+iPID*6,0);
        auto draw = GetFitTestDrawing(iPID,fHistPID,fit,fitContanminent,(iPID==0));
        draw -> SetCreateFrame(Form("frame_pid%d_%d",iPID,fFrameIndex++),Form("pid-%d;eta;count",iPID));
        fGroupFit -> Add(draw);
        auto countHist = fBeamPIDList[iPID][2]; //histData -> GetBinContent(bin);
        auto countBack = fBeamPIDList[iPID][4]; //histBack -> GetBinContent(bin);
        auto corrected = fBeamPIDList[iPID][6]; //count - contamination;
        legend -> AddEntry((TObject*)nullptr,Form("[%d] %d (%d)",iPID,int(countHist),int(countBack)),"");
    }
    
    if (calibrateEta)
    {
        auto sigmaX  = fitTotal->GetParameter(3);
        auto sigmaY  = fitTotal->GetParameter(4);
        auto thetaR  = fitTotal->GetParameter(5);
        //
        double ttMax = -2;
        double i1Max = 0;
        double i2Max = 0;
        auto Rx = sigmaX * sqrt(-2 * log(fSelectedEta));
        auto Ry = sigmaY * sqrt(-2 * log(fSelectedEta));
        for (auto iPID1=0; iPID1<numPIDs; ++iPID1)
        {
            auto valueX1 = fitTotal->GetParameter(1+iPID1*6);
            auto valueY1 = fitTotal->GetParameter(2+iPID1*6);
            TVector3 pos1(valueX1,valueY1,0);
            for (auto iPID2=iPID1+1; iPID2<numPIDs; ++iPID2)
            {
                auto valueX2 = fitTotal->GetParameter(1+iPID2*6);
                auto valueY2 = fitTotal->GetParameter(2+iPID2*6);
                {
                    TVector3 pos2(valueX2,valueY2,0);
                    double theta1 = (pos2-pos1).Phi();
                    double theta2 = theta1 + TMath::Pi();
                    while (theta1>+TMath::Pi()) theta1 = theta1 - 2*TMath::Pi();
                    while (theta1<-TMath::Pi()) theta1 = theta1 + 2*TMath::Pi();
                    while (theta2>+TMath::Pi()) theta2 = theta2 - 2*TMath::Pi();
                    while (theta2<-TMath::Pi()) theta2 = theta2 + 2*TMath::Pi();
                    double efftt1 = fGraphTTOutTTIn -> Eval(theta1-thetaR);
                    double efftt2 = fGraphTTOutTTIn -> Eval(theta2-thetaR);
                    //TVector3 point1(Rx*cos(efftt1-thetaR), Ry*sin(efftt1-thetaR), 0);
                    //TVector3 point2(Rx*cos(efftt2-thetaR), Ry*sin(efftt2-thetaR), 0);
                    TVector3 point1(Rx*cos(efftt1), Ry*sin(efftt1), 0);
                    TVector3 point2(Rx*cos(efftt2), Ry*sin(efftt2), 0);
                    point1.RotateZ(thetaR);
                    point2.RotateZ(thetaR);
                    point1.SetX(valueX1+point1.X()); point1.SetY(valueY1+point1.Y());
                    point2.SetX(valueX2+point2.X()); point2.SetY(valueY2+point2.Y());
                    TVector3 center1(valueX1,valueY1,0);
                    TVector3 center2(valueX2,valueY2,0);
                    TVector3 v = center2 - center1;
                    v.Print();
                    double t1 = v.Dot(point1 - center1) / v.Mag2();
                    double t2 = v.Dot(point2 - center1) / v.Mag2();
                    double dtt = t1 - t2;
                    if (ttMax<dtt) {
                        i1Max = iPID1;
                        i2Max = iPID2;
                        ttMax = dtt;
                    }
                    //auto graphOverlap = new TGraph();
                    //graphOverlap -> SetMarkerStyle(20);
                    //graphOverlap -> SetMarkerSize(0.5);
                    //fDraw2D -> Add(graphOverlap,"samepl");
                    //graphOverlap -> SetPoint(graphOverlap->GetN(),valueX1,valueY1);
                    //graphOverlap -> SetPoint(graphOverlap->GetN(),valueX2,valueY2);
                    //graphOverlap -> SetPoint(graphOverlap->GetN(),point1.X(),point1.Y());
                    //graphOverlap -> SetPoint(graphOverlap->GetN(),point2.X(),point2.Y());
                }
            }
        }

        CalibrateEtaMan(i1Max, i2Max, fitTotal);
        //{
        //    auto iPID1 = i1Max;
        //    auto iPID2 = i2Max;
        //    auto valueX1 = fitTotal->GetParameter(1+iPID1*6);
        //    auto valueY1 = fitTotal->GetParameter(2+iPID1*6);
        //    auto valueX2 = fitTotal->GetParameter(1+iPID2*6);
        //    auto valueY2 = fitTotal->GetParameter(2+iPID2*6);
        //    TVector3 center1(valueX1,valueY1,0);
        //    TVector3 center2(valueX2,valueY2,0);
        //    TVector3 pos1(valueX1,valueY1,0);
        //    TVector3 pos2(valueX2,valueY2,0);
        //    double theta1 = (pos2-pos1).Phi();
        //    double theta2 = theta1 + TMath::Pi();
        //    while (theta1>+TMath::Pi()) theta1 = theta1 - 2*TMath::Pi();
        //    while (theta1<-TMath::Pi()) theta1 = theta1 + 2*TMath::Pi();
        //    while (theta2>+TMath::Pi()) theta2 = theta2 - 2*TMath::Pi();
        //    while (theta2<-TMath::Pi()) theta2 = theta2 + 2*TMath::Pi();
        //    double efftt1 = fGraphTTOutTTIn -> Eval(theta1-thetaR);
        //    double efftt2 = fGraphTTOutTTIn -> Eval(theta2-thetaR);
        //    auto graphDiff = new TGraph();
        //    graphDiff -> SetMarkerStyle(20);
        //    graphDiff -> SetMarkerSize(0.5);
        //    auto drawDiff = fGroupFit -> CreateDrawing();
        //    drawDiff -> Add(graphDiff,"pl");
        //    drawDiff -> SetCreateFrame(Form("diff_%d",fFrameIndex++),";diff;eta");
        //    for (double sValue=0.01; sValue<1; sValue+=0.01)
        //    {
        //        auto Rxi = sigmaX * sqrt(-2 * log(sValue));
        //        auto Ryi = sigmaY * sqrt(-2 * log(sValue));
        //        TVector3 point1(Rxi*cos(efftt1), Ryi*sin(efftt1), 0);
        //        TVector3 point2(Rxi*cos(efftt2), Ryi*sin(efftt2), 0);
        //        point1.RotateZ(thetaR);
        //        point2.RotateZ(thetaR);
        //        point1.SetX(valueX1+point1.X()); point1.SetY(valueY1+point1.Y());
        //        point2.SetX(valueX2+point2.X()); point2.SetY(valueY2+point2.Y());
        //        TVector3 v = center2 - center1;
        //        double t1 = v.Dot(point1 - center1) / v.Mag2();
        //        double t2 = v.Dot(point2 - center1) / v.Mag2();
        //        double dtt = t1 - t2;
        //        graphDiff -> SetPoint(graphDiff->GetN(),dtt,sValue);
        //    }
        //    fSelectedEta = graphDiff -> Eval(0);
        //    e_info << "Calibrated eta value = " << fSelectedEta << endl;
        //    fCalibratedEta = true;
        //}
    }

    if (1) { // sigma, theta calibration
        auto hist_sigmaX = new TH1D(Form("sigmaX_%04d_%d",fCurrentRunNumber,fFrameIndex++),"",20,0,1.2*sigmaX2);
        auto hist_sigmaY = new TH1D(Form("sigmaY_%04d_%d",fCurrentRunNumber,fFrameIndex++),"",20,0,1.2*sigmaY2);
        auto hist_thetaR = new TH1D(Form("thetaR_%04d_%d",fCurrentRunNumber,fFrameIndex++),"",20,0,1.2*thetaR2);
        fGroupFit -> AddHist(hist_sigmaX);
        fGroupFit -> AddHist(hist_sigmaY);
        fGroupFit -> AddHist(hist_thetaR);
        for (auto iPID=0; iPID<numPIDs; ++iPID)
        {
            auto fit = (TF2*) fPIDFitArray -> At(iPID);
            auto sigmaX = fitTotal->GetParameter(3+iPID*6);
            auto sigmaY = fitTotal->GetParameter(4+iPID*6);
            auto thetaR = fitTotal->GetParameter(5+iPID*6);
            hist_sigmaX -> Fill(sigmaX);
            hist_sigmaY -> Fill(sigmaY);
            hist_thetaR -> Fill(thetaR);
        }

        if (calibratePar) {
            fFixSigmaX = hist_sigmaX -> GetMean();
            fFixSigmaY = hist_sigmaY -> GetMean();
            fFixThetaR = hist_thetaR -> GetMean();
            //lk_debug << "<sigmaX> = " << fFixSigmaX << endl;
            //lk_debug << "<sigmaY> = " << fFixSigmaY << endl;
            //lk_debug << "<thetaR> = " << fFixThetaR << endl;
            if (fGraphTTOutTTIn==nullptr) {
                fGraphTTOutTTIn = new TGraph();
                fGraphTTOutTTIn -> SetMarkerStyle(20);
                fGraphTTOutTTIn -> SetMarkerSize(0.5);
            }
            else {
                fGraphTTOutTTIn -> Clear();
                fGraphTTOutTTIn -> Set(0);
            }
            auto drawTT = fGroupFit -> CreateDrawing();
            drawTT -> Add(fGraphTTOutTTIn,"pl");
            drawTT -> SetCreateFrame(Form("tt_%d",fFrameIndex++),";theta_{out};theta_{in}");
            for (double theta=-TMath::Pi(); theta<TMath::Pi(); theta+=0.1) {
                double Rx = fFixSigmaX * sqrt(-2 * log(fSelectedEta));
                double Ry = fFixSigmaY * sqrt(-2 * log(fSelectedEta));
                double pointX = Rx * cos(theta);
                double pointY = Ry * sin(theta);
                TVector3 point(pointX,pointY,0);
                fGraphTTOutTTIn -> SetPoint(fGraphTTOutTTIn->GetN(),point.Phi(),theta);
            }
        }
    }

    if (1) { // count calibration
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
        if (calibrateCnt) {
            fFitCountDiff -> SetParameter(0,graph->GetPointX(0));
            graph -> Fit(fFitCountDiff,"QBR0");
            draw_errr -> Add(fFitCountDiff,"samel");
            graph -> Fit(fFitCountDiff3,"QBR0");
            fBeamPIDList[0][9] = fFitCountDiff3 -> GetParameter(0);
            fCalibratedCnt = true;
        }
        else {
            //fFitCountDiff2 -> SetParameter(0,graph->GetPointX(0));
            //graph -> Fit(fFitCountDiff2);
            //draw_errr -> Add(fFitCountDiff2,"samel");
        }
    }

    { // fit range box
        auto graphFitRange = new TGraph();
        graphFitRange -> SetLineWidth(2);
        graphFitRange -> SetLineColor(kOrange);
        graphFitRange -> SetLineStyle(7);
        graphFitRange -> SetPoint(0,xx1,yy1);
        graphFitRange -> SetPoint(1,xx2,yy1);
        graphFitRange -> SetPoint(2,xx2,yy2);
        graphFitRange -> SetPoint(3,xx1,yy2);
        graphFitRange -> SetPoint(4,xx1,yy1);
        fDraw2D -> Add(graphFitRange,"samel");

        //fDraw2D -> SetCreateLegend();
        fDraw2D -> SetLegendCorner(1);
        fDraw2D -> Add(legend);
        //fGroupFit -> Draw();
        fDraw2D -> Draw();

        Help("rqg");
        fStage = 6;
    }
}

void LKBeamPID::DrawDetail() {
    fGroupFit -> Draw();
}

void LKBeamPID::CalibrateEtaMan(int iPID1, int iPID2, TF2* fitTotal)
{
    if (fitTotal==nullptr) fitTotal = fFitTotal;
    auto sigmaX = fitTotal->GetParameter(3);
    auto sigmaY = fitTotal->GetParameter(4);
    auto thetaR = fitTotal->GetParameter(5);
    auto valueX1 = fitTotal->GetParameter(1+iPID1*6);
    auto valueY1 = fitTotal->GetParameter(2+iPID1*6);
    auto valueX2 = fitTotal->GetParameter(1+iPID2*6);
    auto valueY2 = fitTotal->GetParameter(2+iPID2*6);
    TVector3 center1(valueX1,valueY1,0);
    TVector3 center2(valueX2,valueY2,0);
    TVector3 pos1(valueX1,valueY1,0);
    TVector3 pos2(valueX2,valueY2,0);
    double theta1 = (pos2-pos1).Phi();
    double theta2 = theta1 + TMath::Pi();
    while (theta1>+TMath::Pi()) theta1 = theta1 - 2*TMath::Pi();
    while (theta1<-TMath::Pi()) theta1 = theta1 + 2*TMath::Pi();
    while (theta2>+TMath::Pi()) theta2 = theta2 - 2*TMath::Pi();
    while (theta2<-TMath::Pi()) theta2 = theta2 + 2*TMath::Pi();
    double efftt1 = fGraphTTOutTTIn -> Eval(theta1-thetaR);
    double efftt2 = fGraphTTOutTTIn -> Eval(theta2-thetaR);
    auto graphDiff = new TGraph();
    graphDiff -> SetMarkerStyle(20);
    graphDiff -> SetMarkerSize(0.5);
    auto drawDiff = fGroupFit -> CreateDrawing();
    drawDiff -> Add(graphDiff,"pl");
    drawDiff -> SetCreateFrame(Form("diff_%d",fFrameIndex++),";diff;eta");
    bool found = false;
    for (double sValue=0.01; sValue<1; sValue+=0.01)
    {
        auto Rxi = sigmaX * sqrt(-2 * log(sValue));
        auto Ryi = sigmaY * sqrt(-2 * log(sValue));
        TVector3 point1(Rxi*cos(efftt1), Ryi*sin(efftt1), 0);
        TVector3 point2(Rxi*cos(efftt2), Ryi*sin(efftt2), 0);
        point1.RotateZ(thetaR);
        point2.RotateZ(thetaR);
        point1.SetX(valueX1+point1.X()); point1.SetY(valueY1+point1.Y());
        point2.SetX(valueX2+point2.X()); point2.SetY(valueY2+point2.Y());
        TVector3 v = center2 - center1;
        double t1 = v.Dot(point1 - center1) / v.Mag2();
        double t2 = v.Dot(point2 - center1) / v.Mag2();
        double dtt = t1 - t2;
        if (dtt>0) found = true;
        graphDiff -> SetPoint(graphDiff->GetN(),dtt,sValue);
    }
    if (found==false) {
        fSelectedEta = 0.01;
        lk_debug << "Eta is too small! Setting eta value to " << fSelectedEta << endl;
        lk_debug << "Manually set eta if you want to use lower value" << endl;
    }
    else {
        fSelectedEta = graphDiff -> Eval(0);
        e_info << "Calibrated eta value = " << fSelectedEta << endl;
    }
    fCalibratedEta = true;
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
            fileSummary << setw(25) << Form("s_error_%d",bin-1) << fValueOfS[bin] << "  " << fErrorAtS[bin] << endl;
        }
        fileSummary << endl;

        fileSummary << setw(25) << "xname " << fXName << endl;
        fileSummary << setw(25) << "yname " << fYName << endl;
        fileSummary << endl;

        TString compareEtaString; for (auto s : fCompareEtaList) compareEtaString = compareEtaString + LKMisc::RemoveTrailing0(s) + ", "; compareEtaString.Remove(compareEtaString.Sizeof()-2);
        auto numPIDs = fPIDFitArray -> GetEntries();
        for (auto iPID=0; iPID<numPIDs; ++iPID)
        {
            double aa = fFittingList[iPID][0];
            double x0 = fFittingList[iPID][1];
            double y0 = fFittingList[iPID][2];
            double sx = fFittingList[iPID][3];
            double sy = fFittingList[iPID][4];
            double tt = fFittingList[iPID][5];
            double lx = sx * sqrt(-2 * log(fSelectedEta));
            double ly = sy * sqrt(-2 * log(fSelectedEta));

            fileSummary << "####################################################" << endl;
            fileSummary << setw(30) << Form("pid_%d/svalue              ",iPID) << fBeamPIDList[iPID][1] << endl;
            fileSummary << setw(30) << Form("pid_%d/count/total         ",iPID) << fBeamPIDList[iPID][0] << endl;
            fileSummary << setw(30) << Form("pid_%d/count/data          ",iPID) << fBeamPIDList[iPID][2] << endl;
            fileSummary << setw(30) << Form("pid_%d/count/fit           ",iPID) << fBeamPIDList[iPID][3] << endl;
            fileSummary << setw(30) << Form("pid_%d/count/contamination ",iPID) << fBeamPIDList[iPID][4] << endl;
            fileSummary << setw(30) << Form("pid_%d/count/corrected     ",iPID) << fBeamPIDList[iPID][6] << endl;
            fileSummary << setw(30) << Form("pid_%d/count/contour_purity",iPID) << fBeamPIDList[iPID][7] << endl;
            fileSummary << setw(30) << Form("pid_%d/count/bg_subt_purity",iPID) << fBeamPIDList[iPID][8] << endl;
            fileSummary << setw(30) << Form("pid_%d/count/overall_error ",iPID) << fBeamPIDList[0][9] << endl;
            fileSummary << endl;

            fileSummary << setw(30) << Form("pid_%d/compare_1/svalue    ",iPID) << fCompareList[iPID][0] << endl;
            fileSummary << setw(30) << Form("pid_%d/compare_1/count     ",iPID) << fCompareList[iPID][1] << endl;
            fileSummary << setw(30) << Form("pid_%d/compare_1/systematic",iPID) << fCompareList[iPID][2] << endl;
            fileSummary << setw(30) << Form("pid_%d/compare_1/syst/perc ",iPID) << fCompareList[iPID][2]/fBeamPIDList[iPID][2]*100 << endl;
            fileSummary << setw(30) << Form("pid_%d/compare_2/svalue    ",iPID) << fCompareList[iPID][3] << endl;
            fileSummary << setw(30) << Form("pid_%d/compare_2/count     ",iPID) << fCompareList[iPID][4] << endl;
            fileSummary << setw(30) << Form("pid_%d/compare_2/systematic",iPID) << fCompareList[iPID][5] << endl;
            fileSummary << setw(30) << Form("pid_%d/compare_2/syst/perc ",iPID) << fCompareList[iPID][5]/fBeamPIDList[iPID][2]*100 << endl;
            fileSummary << endl;

            fileSummary << setw(30) << Form("pid_%d/fit/amplitude       ",iPID) << aa << endl;
            fileSummary << setw(30) << Form("pid_%d/fit/valueX          ",iPID) << x0 << endl;
            fileSummary << setw(30) << Form("pid_%d/fit/valueY          ",iPID) << y0 << endl;
            fileSummary << setw(30) << Form("pid_%d/fit/sigmaX          ",iPID) << sx << endl;
            fileSummary << setw(30) << Form("pid_%d/fit/sigmaY          ",iPID) << sy << endl;
            fileSummary << setw(30) << Form("pid_%d/fit/theta           ",iPID) << tt << endl;
            fileSummary << endl;

            TString gateEquation = Form("((x-(%f))*cos(%f)+(y-(%f))*sin(%f))*((x-(%f))*cos(%f)+(y-(%f))*sin(%f))/(%f)/(%f) + ((y-(%f))*cos(%f)-(x-(%f))*sin(%f))*((y-(%f))*cos(%f)-(x-(%f))*sin(%f))/(%f)/(%f) < 1",
                    x0,tt,y0,tt,
                    x0,tt,y0,tt,
                    lx,lx,
                    y0,tt,x0,tt,
                    y0,tt,x0,tt,
                    ly,ly);
            fileSummary << setw(30) << Form("pid_%d/fit/gate            ",iPID) << gateEquation << endl;
            const char* xx = "%s";
            const char* yy = "%s";
            TString gateEquation2 = Form("Form(\"((%s-(%f))*cos(%f)+(%s-(%f))*sin(%f))*((%s-(%f))*cos(%f)+(%s-(%f))*sin(%f))/(%f)/(%f) + ((%s-(%f))*cos(%f)-(%s-(%f))*sin(%f))*((%s-(%f))*cos(%f)-(%s-(%f))*sin(%f))/(%f)/(%f) < 1\",xName,yName,xName,yName,yName,xName,yName,xName)",
                    xx,x0,tt,yy,y0,tt,
                    xx,x0,tt,yy,y0,tt,
                    lx,lx,
                    yy,y0,tt,xx,x0,tt,
                    yy,y0,tt,xx,x0,tt,
                    ly,ly);
            fileSummary << setw(30) << Form("pid_%d/fit/gate2           ",iPID) << gateEquation2 << endl;
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

LKDrawing* LKBeamPID::GetFitTestDrawing(int iPID, TH2D *hist, TF2* fit, TF2* fitContanminent, bool resetError)
{
    gStyle->SetPaintTextFormat(".3f");
    auto amplit = fit->GetParameter(0);
    auto valueX = fit->GetParameter(1);
    auto valueY = fit->GetParameter(2);
    auto sigmaX = fit->GetParameter(3);
    auto sigmaY = fit->GetParameter(4);
    auto thetaR = fit->GetParameter(5);
    fFittingList[iPID][0] = amplit; // 0 amplitude
    fFittingList[iPID][1] = valueX; // 1 valueX
    fFittingList[iPID][2] = valueY; // 2 valueY
    fFittingList[iPID][3] = sigmaX; // 3 sigmaX
    fFittingList[iPID][4] = sigmaY; // 4 sigmaY
    fFittingList[iPID][5] = thetaR; // 5 theta

    TString nameData = fit -> GetName(); nameData.ReplaceAll("fit_","graphIntegralData_");
    TString nameFitG = fit -> GetName(); nameFitG.ReplaceAll("fit_","graphIntegralFitG_");
    TString nameTest = fit -> GetName(); nameTest.ReplaceAll("fit_","graphIntegralTest_");
    TString nameBack = fit -> GetName(); nameBack.ReplaceAll("fit_","graphIntegralBack_");
    TString nameCalc = fit -> GetName(); nameBack.ReplaceAll("fit_","graphIntegralCalc_");
    TString nameErrr = fit -> GetName(); nameErrr.ReplaceAll("fit_","graphIntegralErrr_");
    TString title = Form("[RUN %04d] (%d) Count in contour;S = Contour amplitude scale [Amp];Count",fCurrentRunNumber,iPID);

    auto histErrr = (TH2D*) fHistErrrArray -> At(0);
    if (histErrr==nullptr) {
        histErrr = new TH2D(nameErrr,title,fNumContours,0,1,60,-0.3,0.3);
        fHistErrrArray -> Add(histErrr);
    }
    if (resetError) {
        histErrr -> Reset("ICES");
        TString title2 = Form("[RUN %04d] (%d);S = Contour amplitude [A];Error",fCurrentRunNumber,iPID);
        histErrr -> SetTitle(title2);
        histErrr -> SetName(nameErrr);
    }

    auto graphData = (TGraphErrors*) fHistDataArray -> At(iPID);
    auto graphFitG = (TGraphErrors*) fHistFitGArray -> At(iPID);
    auto graphBack = (TGraphErrors*) fHistBackArray -> At(iPID);
    auto graphCalc = (TGraphErrors*) fHistCalcArray -> At(iPID);
    auto graphTest = (TGraphErrors*) fHistTestArray -> At(iPID);
    if (graphData==nullptr)
    {
        gROOT -> cd();
        graphData = new TGraphErrors(); graphData -> SetName(nameData);
        graphFitG = new TGraphErrors(); graphFitG -> SetName(nameFitG);
        graphBack = new TGraphErrors(); graphBack -> SetName(nameBack);
        graphCalc = new TGraphErrors(); graphCalc -> SetName(nameCalc);
        graphTest = new TGraphErrors(); graphTest -> SetName(nameTest);
        fHistDataArray -> Add(graphData);
        fHistFitGArray -> Add(graphFitG);
        fHistBackArray -> Add(graphBack);
        fHistCalcArray -> Add(graphCalc);
        fHistTestArray -> Add(graphTest);
        //
        graphData -> SetFillColor(19);
        graphData -> SetMarkerStyle(20);
        graphData -> SetMarkerColor(kBlack);
        graphData -> SetLineWidth(2);
        graphData -> SetLineColor(kBlack);
        //graphData -> GetXaxis() -> SetTitleOffset(1.2);
        //
        graphFitG -> SetMarkerColor(kRed);
        graphFitG -> SetMarkerStyle(21);
        graphFitG -> SetLineColor(kRed);
        graphFitG -> SetLineWidth(2);
        graphFitG -> SetLineStyle(2);
        //
        graphBack -> SetMarkerColor(kGreen);
        graphBack -> SetMarkerStyle(24);
        graphBack -> SetLineColor(kGreen);
        graphBack -> SetLineWidth(2);
        graphBack -> SetLineStyle(1);
        //
        graphCalc -> SetMarkerColor(kBlue);
        graphCalc -> SetMarkerStyle(25);
        graphCalc -> SetLineColor(kBlue);
        graphCalc -> SetLineWidth(1);
        graphCalc -> SetLineStyle(1);
    }
    else {
        graphData -> Clear();
        graphFitG -> Clear();
        graphBack -> Clear();
        graphCalc -> Clear();
        graphTest -> Clear();
        graphData -> Set(0);
        graphFitG -> Set(0);
        graphBack -> Set(0);
        graphCalc -> Set(0);
        graphTest -> Set(0);
        graphData -> SetName(nameData);
        graphFitG -> SetName(nameFitG);
        graphBack -> SetName(nameCalc);
        graphCalc -> SetName(nameBack);
        graphTest -> SetName(nameTest);
    }

    auto graphAtSelectedEta = new TGraphAsymmErrors();
    graphAtSelectedEta -> SetMarkerStyle(24);
    graphAtSelectedEta -> SetMarkerSize(1.5);
    graphAtSelectedEta -> SetMarkerColor(40);
    graphAtSelectedEta -> SetLineColor(40);
    auto ttCompare1 = new TLatex();
    auto ttCompare2 = new TLatex();
    ttCompare1 -> SetTextSize(0.025);
    ttCompare2 -> SetTextSize(0.025);
    ttCompare1 -> SetTextAlign(12);
    ttCompare2 -> SetTextAlign(12);
    auto draw = new LKDrawing();
    draw -> SetCanvasMargin(0.15,0.05,0.1,0.1);
    draw -> SetOptStat(0);
    draw -> SetAutoMax();
    draw -> SetCreateLegend();
    draw -> Add(graphData,"samepl","data");
    if (fitContanminent!=nullptr)
        draw -> Add(graphBack,"samepl","contaminent");
    draw -> Add(graphFitG,"samepl","fit");
    draw -> Add(graphCalc,"samepl","fit+contam.");
    draw -> Add(graphAtSelectedEta,"same pl","selected");
    draw -> Add(ttCompare1,"same",".");
    draw -> Add(ttCompare2,"same",".");
    draw -> AddLegendLine(Form("A=%.2f",amplit));
    draw -> AddLegendLine(Form("x=%.2f",valueX));
    draw -> AddLegendLine(Form("#sigma_{x}=%.2f",sigmaX));
    draw -> AddLegendLine(Form("y=%.2f",valueY));
    draw -> AddLegendLine(Form("#sigma_{y}=%.2f",sigmaY));
    draw -> AddLegendLine(Form("#theta=%.1f deg.",thetaR*TMath::RadToDeg()));
    double wx = hist -> GetXaxis() -> GetBinWidth(1);
    double wy = hist -> GetYaxis() -> GetBinWidth(1);
    double binArea = wx*wy;
    double dc = (1./fNumContours);
    double parameters[6] = {amplit, valueX, valueY, sigmaX, sigmaY, thetaR};
    double countData[6] = {0};

    for (double sValue=0; sValue<1; sValue+=dc)
    {
        EvaluateCounts(parameters, countData, iPID, 0, sValue, binArea, hist, fit, fitContanminent);
        double countFitG = countData[0];
        double countCalc = countData[1];
        double countTest = countData[2];
        double countBack = countData[3];
        double countHist = countData[4];
        auto idx = graphData -> GetN();
        graphFitG -> SetPoint(idx,sValue,countFitG);
        graphTest -> SetPoint(idx,sValue,countTest);
        if (fitContanminent!=nullptr)
            graphBack -> SetPoint(idx,sValue,countBack);
        graphCalc -> SetPoint(idx,sValue,countCalc);
        if (sValue!=0) {
            graphData -> SetPoint(idx,sValue,countHist);
            double signalRatio = countFitG/(countFitG+countBack);
            double diff = (countHist*signalRatio-countFitG)/(countHist*signalRatio);
            histErrr -> Fill(sValue+0.5*dc,diff);
        }
    }

    { // selected
        double sValue = fSelectedEta; // XXX
        EvaluateCounts(parameters, countData, iPID, 1, sValue, binArea, hist, fit, fitContanminent);
        double countFitG = countData[0];
        double countBack = countData[3];
        double countHist = countData[4];
        //fBeamPIDList[iPID][0] = 0; // 0 total
        fBeamPIDList[iPID][1] = fSelectedEta; // 1 count svalue
        fBeamPIDList[iPID][2] = countHist; // 2 count data
        fBeamPIDList[iPID][3] = countFitG; // 3 count fit
        fBeamPIDList[iPID][4] = countBack; // 4 contamination count
        fBeamPIDList[iPID][5] = 0; // 5 contamination error
        fBeamPIDList[iPID][6] = countHist-countBack; // 6 corrected count
        fBeamPIDList[iPID][7] = (countHist)/fBeamPIDList[iPID][0]; // 7 purity (contour_count/total)
        fBeamPIDList[iPID][8] = (countHist-countBack)/fBeamPIDList[iPID][0]; // 8 purity (bg_subtracted_count/total)
        ttCompare1 -> SetX(fCompareList[iPID][0]);
        ttCompare2 -> SetX(fCompareList[iPID][3]);
        ttCompare1 -> SetY(fCompareList[iPID][1]);
        ttCompare2 -> SetY(fCompareList[iPID][4]);
        ttCompare1 -> SetTitle(Form("  %.1f %s",100*fCompareList[iPID][2]/fBeamPIDList[iPID][2],"%"));
        ttCompare2 -> SetTitle(Form("  %.1f %s",100*fCompareList[iPID][5]/fBeamPIDList[iPID][2],"%"));
    }
    int iCompare = 0;
    for (double sValue : fCompareEtaList) { // compare
        EvaluateCounts(parameters, countData, iPID, 1, sValue, binArea, hist, fit, fitContanminent);
        double countHist = countData[4];
        if (iCompare==0) fCompareList[iPID][1] = countHist; // 1 compare-1 count
        if (iCompare==0) fCompareList[iPID][2] = (countHist - fBeamPIDList[iPID][2]); // 2 compare-1 systematic
        if (iCompare==1) fCompareList[iPID][4] = countHist; // 4 compare-2 count
        if (iCompare==1) fCompareList[iPID][5] = (countHist - fBeamPIDList[iPID][2]); // 5 compare-2 systematic
        ++iCompare;
    }
    graphAtSelectedEta -> SetPoint(0,fSelectedEta,fBeamPIDList[iPID][2]);
    graphAtSelectedEta -> SetPoint(1,fCompareList[iPID][0],fCompareList[iPID][1]);
    graphAtSelectedEta -> SetPoint(2,fCompareList[iPID][3],fCompareList[iPID][4]);
    return draw;
}

void LKBeamPID::EvaluateCounts(double parameters[6], double countData[6], int iPID, bool isSelectedEta, double sValue, double binArea, TH2D* hist, TF2* fit, TF2* fitContanminent)
{
    double amplit = parameters[0];
    double valueX = parameters[1];
    double valueY = parameters[2];
    double sigmaX = parameters[3];
    double sigmaY = parameters[4];
    double thetaR = parameters[5];

    auto graphC = GetContourGraph(sValue*amplit, amplit, valueX, sigmaX, valueY, sigmaY, thetaR);
    graphC -> SetName(Form("contourGraph_%d_%.2f",iPID,sValue));
    if (isSelectedEta) fFinalContourGraph = graphC;
    double countFitG = Integral2DGaussian(fit, sValue) / binArea;
    double countCalc = countFitG;
    double x_contour = sValue;//+0.5*dc;
    double countTest = IntegralInsideGraph(hist, graphC, fit);
    double countBack = 0;
    if (fitContanminent!=nullptr) {
        countBack = IntegralInsideGraph(hist, graphC, fitContanminent);
        countCalc = countCalc + countBack;
    }
    double countHist = 0;
    if (sValue!=0) {
        countHist = IntegralInsideGraph(hist, graphC);
    }

    countData[0] = countFitG;
    countData[1] = countCalc;
    countData[2] = countTest;
    countData[3] = countBack;
    countData[4] = countHist;
}

TF2* LKBeamPID::Fit2DGaussian(TH2D *hist, int iPID, double valueX, double valueY, double sigmaX, double sigmaY, double theta)
{
    if (sigmaX==0) sigmaX = fDefaultFitSigmaX;
    if (sigmaY==0) sigmaY = fDefaultFitSigmaY;
    if (theta==0) theta = fDefaultFitTheta;
    TF2 *fit = new TF2(Form("fit_%04d_%d", fCurrentRunNumber, iPID), fFormulaRotated2DGaussian, valueX-fFitRangeInSigma*sigmaX,valueX+fFitRangeInSigma*sigmaX, valueY-fFitRangeInSigma*sigmaY,valueY+fFitRangeInSigma*sigmaY);
    double amplit = hist -> GetBinContent(hist->GetXaxis()->FindBin(valueX),hist->GetYaxis()->FindBin(valueY));
    fit -> SetParameter(0, amplit);
    fit -> SetParameter(1, valueX);
    fit -> SetParameter(2, valueY);
    fit -> SetParameter(3, sigmaX);
    fit -> SetParameter(4, sigmaY);
    fit -> SetParameter(5, theta*TMath::DegToRad());
    fit -> SetParLimits(0, amplit*0.5, amplit*2);
    fit -> SetParLimits(1, valueX-0.5*sigmaX, valueX+0.5*sigmaX);
    fit -> SetParLimits(2, valueY-0.5*sigmaY, valueY+0.5*sigmaY);
    fit -> SetParLimits(3, sigmaX*0.5, sigmaX*1.2);
    fit -> SetParLimits(4, sigmaY*0.5, sigmaY*1.2);
    fit -> SetParLimits(5, 0.*TMath::Pi(), 0.25*TMath::Pi());
    if (0) {
        for (auto ipar : {0,1,2,3,4,5}) {
            double parval = fit -> GetParameter(ipar);
            double parmin, parmax;
            fit -> GetParLimits(ipar, parmin, parmax);
            lk_debug << parval << " " << parmin << " -> " << parmax << endl;
        }
    }
    if (fFixSigmaX>=0) { fit -> SetParameter(3, fFixSigmaX); fit -> FixParameter(3, fFixSigmaX); }
    if (fFixSigmaY>=0) { fit -> SetParameter(4, fFixSigmaY); fit -> FixParameter(4, fFixSigmaY); }
    if (fFixThetaR>=0) { fit -> SetParameter(5, fFixThetaR); fit -> FixParameter(5, fFixThetaR); }
    fit -> SetLineColor(kRed);
    hist -> Fit(fit,"QBR0");
    fit -> SetContour(3);
    e_cout << "   " << fit -> GetName() << ": " << std::left
        << setw(16) << Form("amp=%f,", fit->GetParameter(0))
        << setw(28) << Form("x=(%f, %f),", fit->GetParameter(1), fit->GetParameter(3))
        << setw(28) << Form("y=(%f, %f),", fit->GetParameter(2), fit->GetParameter(4))
        << setw(18) << Form("theta=%f,", fit->GetParameter(5)*TMath::RadToDeg())
        << endl;
    return fit;
}

TGraph *LKBeamPID::GetContourGraph(double sValue, double amplit, double valueX, double sigmaX, double valueY, double sigmaY, double thetaR)
{
    double Rx = sigmaX * sqrt(-2 * log(sValue / amplit));
    double Ry = sigmaY * sqrt(-2 * log(sValue / amplit));

    auto graph = new TGraph();
    graph -> SetMarkerStyle(20);

    for (double theta=-TMath::Pi(); theta<TMath::Pi(); theta+=0.1) {
        double pointX = Rx * cos(theta);
        double pointY = Ry * sin(theta);
        TVector3 point(pointX,pointY,0);
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
    double binArea = wx*wy;
    if (justCount) binArea = 1;
    for (auto xbin=1; xbin<=nx; ++xbin) {
        double xvalue = hist -> GetXaxis() -> GetBinCenter(xbin);
        for (auto ybin=1; ybin<=ny; ++ybin) {
            double yvalue = hist -> GetYaxis() -> GetBinCenter(ybin);
            if (graph -> IsInside(xvalue,yvalue)) {
                double value = hist -> GetBinContent(xbin,ybin);
                integral += value*binArea;
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
    double binArea = wx*wy;
    if (justCount) binArea = 1;
    for (auto xbin=1; xbin<=nx; ++xbin) {
        double xvalue = hist -> GetXaxis() -> GetBinCenter(xbin);
        for (auto ybin=1; ybin<=ny; ++ybin) {
            double yvalue = hist -> GetYaxis() -> GetBinCenter(ybin);
            if (graph -> IsInside(xvalue,yvalue)) {
                double value = f2 -> Eval(xvalue,yvalue);
                integral += value*binArea;
            }
        }
    }
    return integral;
}

double LKBeamPID::Integral2DGaussian(double amplitude, double sigmaX, double sigmaY, double contourS)
{
    double value = amplitude*2*TMath::Pi()*sigmaX*sigmaY*(1-contourS);
    value = value * (1+fFitCountDiff->Eval(contourS));
    return value;
}

double LKBeamPID::Integral2DGaussian(TF2 *f2, double contourS)
{
    double value = Integral2DGaussian(f2->GetParameter(0), f2->GetParameter(3), f2->GetParameter(4), contourS);
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

void LKBeamPID::SetEta(double scale)
{
    if (scale<0) {
        e_note << "Enter fit range from 0.1 to 0.9 (1 default): ";
        TString inputString;
        cin >> inputString;
        inputString = inputString.Strip(TString::kBoth);
        scale = inputString.Atof();
    }
    fSelectedEta = scale;
    e_cout << "   " << fSelectedEta << endl;
}

void LKBeamPID::SaveConfiguration()
{
    TString drawEtaString; for (auto s : fDrawEtaList) drawEtaString = drawEtaString + LKMisc::RemoveTrailing0(s) + ", "; drawEtaString.Remove(drawEtaString.Sizeof()-2);
    TString compareEtaString; for (auto s : fCompareEtaList) compareEtaString = compareEtaString + LKMisc::RemoveTrailing0(s) + ", "; compareEtaString.Remove(compareEtaString.Sizeof()-2);
    SaveBinning();
    TString bnnString = Form("%d,%f,%f, %d,%f,%f",fBnn1.nx(), fBnn1.x1(), fBnn1.x2(), fBnn1.ny(), fBnn1.y1(), fBnn1.y2());
    TString xName = fSetXName.IsNull()?".":fSetXName;
    TString yName = fSetYName.IsNull()?".":fSetYName;
    LKParameterContainer par;
    par.AddPar("","","LKBeamPID configuration file");
    par.AddPar("","","eta values");
    par.AddPar("eta"                   ,LKMisc::RemoveTrailing0(fSelectedEta),        "eta (ratio compared to the gaussian amplitude) for drawing pid cut contour");
    par.AddPar("draw_eta_list"         ,drawEtaString,       "list of eta for contours in the pid pid in addition to the parameter \"eta\".");
    par.AddPar("compare_eta_list"      ,compareEtaString,    "these eta values will calculate the statistics information in the summary file in addition to the parameter \"eta\". maximum of two");
    par.AddPar("","","parameters for drawing histograms from the tree");
    par.AddPar("data_path"             ,fDefaultPath,        "path to look for the files");
    par.AddPar("file_format"           ,fDefaultFormat,      "function will search files which end with this value");
    par.AddPar("x_name"                ,xName,               "x value name in tree. use \".\" to use default value");
    par.AddPar("y_name"                ,yName,               "y value name in tree. use \".\" to use default value");
    par.AddPar("binning"               ,bnnString,           "default x(3), y(3) binning for pid plot");
    par.AddPar("","","parameter settings");
    par.AddPar("init_sigma_x"          ,LKMisc::RemoveTrailing0(fDefaultFitSigmaX),   "default initial sigma_x value");
    par.AddPar("init_sigma_y"          ,LKMisc::RemoveTrailing0(fDefaultFitSigmaY),   "default initial sigma_y value");
    par.AddPar("init_theta"            ,LKMisc::RemoveTrailing0(fDefaultFitTheta),    "default initial theta value");
    par.AddPar("fix_sigma_x"           ,LKMisc::RemoveTrailing0(fFixSigmaX),          "fix sigma_x value, the shape parameter fit will not change the value if set >=0");
    par.AddPar("fix_sigma_y"           ,LKMisc::RemoveTrailing0(fFixSigmaY),          "fix sigma_y vanot change the value if set >=0");
    par.AddPar("fix_theta"             ,LKMisc::RemoveTrailing0(fFixThetaR),          "fix theta vanot change the value if set >=0");
    par.AddPar("","","fitting configuration");
    par.AddPar("fit_amp_ratio_range"   ,LKMisc::RemoveTrailing0(fAmpRatioRange),      "amplitude (+-) range in ratio. Used to limit the parameter range when fitting");
    par.AddPar("fit_pos_ratio_range"   ,LKMisc::RemoveTrailing0(fPosRatioRangeInSig), "position (+-) range in ratio. Used to limit the parameter range when fitting");
    par.AddPar("fit_sigma_ratio_range" ,LKMisc::RemoveTrailing0(fSigmaRatioRange),    "sigma (+-) range in ratio. Used to limit the parameter range when fitting");
    par.AddPar("fit_theta_range"       ,LKMisc::RemoveTrailing0(fThetaRange),         "theta (+-) range. Used to limit the parameter range when fitting");
    par.AddPar("fit_range"             ,LKMisc::RemoveTrailing0(fFitRangeInSigma),    "the PID fit range will be chosen using this value by [fit_range]*[init_sigma]");
    par.AddPar("","","drawing configuration");
    par.AddPar("contour_color"         ,fContourColor,       "set the color of the contour in the drawing. default is kRed");
    par.AddPar("text_color_dark"       ,fPIDIndexTextColor1, "text color of the index number of the pid for [pid_amplit]>=0.3*[hist_max]");
    par.AddPar("text_color_bright"     ,fPIDIndexTextColor2, "text color of the index number of the pid for [pid_amplit]< 0.3*[hist_max]");
    par.AddPar("legend_fill_style"     ,fLegendFillStyle,    "fill style for the legend containing pid index and count information");
    par.AddPar("","","else");
    par.AddPar("num_contours"          ,fNumContours,        "number of contours for integral test");
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
void LKBeamPID::SetXBinSize(double w, int fill) { fBnn1.SetWX(w); if (fill) CreateAndFillHistogram(1); fDraw2D -> Draw(); }
void LKBeamPID::SetYBinSize(double w, int fill) { fBnn1.SetWY(w); if (fill) CreateAndFillHistogram(1); fDraw2D -> Draw(); }
void LKBeamPID::SetBinNX(double n, int fill) { fBnn1.SetNX(n); if (fill) CreateAndFillHistogram(1); fDraw2D -> Draw(); }
void LKBeamPID::SetBinNY(double n, int fill) { fBnn1.SetNY(n); if (fill) CreateAndFillHistogram(1); fDraw2D -> Draw(); }
void LKBeamPID::SaveBinning() {
    if (!fHistPID) return;
    fBnn1.SetBinning(fHistPID,true);
    e_info << "Saving binning: " << fBnn1.Print(false) << endl;
    CreateAndFillHistogram(1);
}
