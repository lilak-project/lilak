#include "LKAttenuationCalculator.h"
#include "LKLogger.h"

ClassImp(LKAttenuationCalculator)

void LKAttenuationCalculator::SetParameters(int factor, int exponent, double hole_diameter, double beam_radius, int beam_type)
{
    fFactor = factor;
    fExponent = exponent;
    fHoleDiameter = hole_diameter;
    fHoleDiameterCalc = hole_diameter;
    fBeamRadius = beam_radius;
    fAttenuation = fFactor * std::pow(10.0, fExponent);

    fBeamType = -1;
    if (beam_type==kUniformBeamProfile) fBeamType = beam_type;
    else if (beam_type==kGauss2DBeamProfile) fBeamType = beam_type;
    else e_error << "beam_type should be 0(gaussian) or 1(uniform)" << endl;

    SetNameTitleAuto();
}

void LKAttenuationCalculator::SetGeometricalParameters(int grid_pattern, double attenuatorSize, double activeWidth, double hole_x_offset, double hole_y_offset)
{
    fGridPattern    = grid_pattern;
    fAttenuatorSize = attenuatorSize;
    fActiveSize     = activeWidth;
    if (hole_x_offset>=0&&hole_x_offset<1) fHoleXOffsetInR = hole_x_offset;
    else e_error << "hole x offset should be 0<=offset<1" << endl;
    if (hole_y_offset>=0&&hole_y_offset<1) fHoleYOffsetInR = hole_y_offset;
    else e_error << "hole y offset should be 0<=offset<1" << endl;

    SetNameTitleAuto();
}

void LKAttenuationCalculator::SetNameTitleAuto()
{
    TString attenuatorName = Form("Attn_%sxE%d",LKMisc::RemoveTrailing0(fFactor,true).Data(),fExponent);
    TString beamName = "_x";
    if (fBeamType==kUniformBeamProfile) beamName = "_Beam_uniform";
    else if (fBeamType==kGauss2DBeamProfile) beamName = "_Beam_gaussian";
    beamName = beamName + "_R" + LKMisc::RemoveTrailing0(fBeamRadius,true);
    TString geomName = Form("_Geom_%d_%s_%sx%s", fGridPattern, LKMisc::RemoveTrailing0(fAttenuatorSize,true).Data(), LKMisc::RemoveTrailing0(fActiveSize,true).Data(), LKMisc::RemoveTrailing0(fActiveSize,true).Data());
    TString offsetName;
    if (fHoleXOffsetInR>0||fHoleYOffsetInR>0) offsetName = Form("_Offset_%s_%s",LKMisc::RemoveTrailing0(100*fHoleXOffsetInR,true).Data(),LKMisc::RemoveTrailing0(100*fHoleYOffsetInR,true).Data());
    TString name = attenuatorName+beamName+geomName+offsetName;
    TString title = Form("fc=%d, ex=%d, hd=%.2f, br=%.2f, by=%d, gt=%d, as=%.2f, aw=%.2f, ah=%.2f, ox=%.2f, oy=%.2f",fFactor,fExponent,fHoleDiameter,fBeamRadius,fBeamType,fGridPattern,fAttenuatorSize,fActiveSize,fActiveSize,fHoleXOffsetInR,fHoleYOffsetInR);
    SetNameTitle(name,title);
}

void LKAttenuationCalculator::SetCalculationParameters(double calculationRange, double efficiencyRange1, double efficiencyRange2, double beamRadiusToSigma, int numEvaluationStepsInX, int numEvaluationStepsInY)
{
    fCalculationRange      = calculationRange;
    fEfficiencyRange1      = efficiencyRange1;
    fEfficiencyRange2      = efficiencyRange2;
    fBeamRadiusToSigma     = beamRadiusToSigma;
    fNumEvaluationStepsInX = numEvaluationStepsInX;
    fNumEvaluationStepsInY = numEvaluationStepsInY;
}

LKDrawingGroup* LKAttenuationCalculator::Run(bool add_sketch, bool add_1d, bool add_2d, bool print_holes)
{
    double attenuator_half = 0.5*fAttenuatorSize;
    double attenuator_radius = 0.5*fActiveSize; // radius of active attenuator area (mm)

    auto drawings = new LKDrawingGroup(fName);

    auto x0     = 0.5*(fAttenuatorSize - fActiveSize);
    auto y0     = 0.5*(fAttenuatorSize - fActiveSize);
    auto area_h = 0.5*fHoleDiameter*0.5*fHoleDiameter*TMath::Pi();
    auto area_a = fActiveSize*fActiveSize;
    auto x_dist = sqrt(2*area_h/sqrt(3)/fAttenuation);
    auto y_dist = sqrt(3)*x_dist/2;
    auto ny     = int(fActiveSize/y_dist)+1;
    auto nx1    = int(fActiveSize/x_dist)+1;
    auto nx     = nx1;

    auto area_c = TMath::Pi()*fActiveSize*fActiveSize/4;
    nx = nx;
    ny = ny;

    vector<TVector3> holePositions;
    auto draw_attenuator_sketch = drawings -> CreateDrawing(Form("draw_%s_sketch",fName.Data()),add_sketch);

    TString hname = Form("hist_%s",fName.Data());
    auto hist_attenuator = new TH2D(hname,";x (mm);y (mm)",100,0,fAttenuatorSize,100,0,fAttenuatorSize);
    hist_attenuator -> SetStats(0);
    draw_attenuator_sketch -> Add(hist_attenuator,"col");

    auto graph_holes = NewGraph("graph_holes", 24, 0.3, kBlack);
    auto graph_active_boundary = NewGraph("graph_active_boundary");
    graph_active_boundary -> SetLineStyle(2);
    for (auto i=0; i<=100; ++i)
        graph_active_boundary -> SetPoint(graph_active_boundary->GetN(), attenuator_radius*cos(i*TMath::Pi()/50)+attenuator_half, attenuator_radius*sin(i*TMath::Pi()/50)+attenuator_half);
    draw_attenuator_sketch -> Add(graph_active_boundary,"samel");

    for (auto yy : {y0,attenuator_half,fAttenuatorSize-y0}) {
        auto line = new TLine(0,yy,fAttenuatorSize,yy);
        line -> SetLineStyle(2);
        draw_attenuator_sketch -> Add(line,"samel");
    }
    for (auto xx : {x0,attenuator_half,fAttenuatorSize-x0}) {
        auto line = new TLine(xx,0,xx,fAttenuatorSize);
        line -> SetLineStyle(2);
        draw_attenuator_sketch -> Add(line,"samel");
    }

    double xLow  = x0;
    double xHigh = x0 + (nx-1)*x_dist;
    double yLow  = y0;
    double yHigh = y0 + (ny-1)*y_dist;
    double dxi = (attenuator_half-0.5*(xLow+xHigh));
    double dyi = (attenuator_half-0.5*(yLow+yHigh));
    //dyi = dyi-0.5*y_dist;
    double xi = x0+dxi;
    double yi = y0+dyi;
    int countY = 0;
    for (auto iy=-1; iy<ny+1; ++iy)
    {
        nx = nx1;
        //if (iy%2==1) nx = nx2;
        double yh = yi + iy*y_dist;
        yh = yh + fHoleYOffsetInR*2*y_dist;
        bool firstX = true;
        TString noteX;
        double xh1, xh2;
        int countX = 0;
        for (auto ix=-1; ix<nx+1; ++ix)
        {
            double xh = xi + ix*x_dist;
            if (iy%2==fGridPattern) xh = xh + 0.5*x_dist;
            xh = xh + fHoleXOffsetInR*x_dist;
            if (xh>x0+fActiveSize) continue;
            if ((attenuator_half-xh)*(attenuator_half-xh)+(attenuator_half-yh)*(attenuator_half-yh)>attenuator_radius*attenuator_radius) continue;
            if (firstX) {
                firstX = false;
                noteX = Form(" %d) x=%.4f, y=%.4f",countY+1,xh,yh);
                xh1 = xh;
                xh2 = yh;
            }
            graph_holes -> SetPoint(graph_holes->GetN(), xh, yh);
            holePositions.push_back(TVector3(xh,yh,0));
            countX++;
        }
        if (countX>0) countY++;
    }
    int num_holes = holePositions.size();

    if (graph_holes->GetN()>800) { graph_holes -> SetMarkerStyle(7); graph_holes -> SetMarkerSize(1); }
    if (graph_holes->GetN()>400) graph_holes -> SetMarkerSize(0.1);
    if (graph_holes->GetN()>200) graph_holes -> SetMarkerSize(0.2);
    if (graph_holes->GetN()>100) graph_holes -> SetMarkerSize(0.3);
    if (graph_holes->GetN()>50 ) graph_holes -> SetMarkerSize(0.4);
    else graph_holes -> SetMarkerSize(0.5);
    draw_attenuator_sketch -> Add(graph_holes,"samep");

    TString title = Form("A=%dx10^{%d}, #phi_{hole}=%.2f, n_{hole}=%d, r_{beam}=%.2f",fFactor,fExponent,fHoleDiameter,num_holes,fBeamRadius);
    hist_attenuator -> SetTitle(title);

    auto draw_efficiency_2d = drawings -> CreateDrawing(Form("draw_%s_2d",fName.Data()),add_2d);
    draw_efficiency_2d -> SetCanvasMargin(0.115,0.14,0.125,0.13);

    auto graph_circle_u = NewGraph("graph_circle_u",20,0.6,kBlue);
    auto graph_circle_r = NewGraph("graph_circle_r"); graph_circle_r -> SetLineColor(kBlue);
    auto graph_circle_g = NewGraph("graph_circle_g",20,0.6,kRed);
    auto graph_circle_3 = NewGraph("graph_circle_3"); graph_circle_3 -> SetLineColor(kRed);
    auto graph_circle_2 = NewGraph("graph_circle_2"); graph_circle_2 -> SetLineColor(kRed);
    auto graph_circle_1 = NewGraph("graph_circle_1"); graph_circle_1 -> SetLineColor(kRed);
    auto graph_sim_center = NewGraph("graph_sim_center");
    auto graph_sim_center_range = NewGraph("graph_sim_center_range");
    double xs1 = attenuator_half - 0.5*x_dist;
    double xs2 = attenuator_half + 0.5*x_dist;
    double ys1 = attenuator_half - 0.5*y_dist;
    double ys2 = attenuator_half + 0.5*y_dist;
    if (fCalculationRange>0.5) {
        xs1 = attenuator_half - fCalculationRange*x_dist;
        xs2 = attenuator_half + fCalculationRange*x_dist;
        ys1 = attenuator_half - fCalculationRange*y_dist;
        ys2 = attenuator_half + fCalculationRange*y_dist;
    }
    graph_sim_center_range -> SetPoint(0,xs1,ys1);
    graph_sim_center_range -> SetPoint(1,xs1,ys2);
    graph_sim_center_range -> SetPoint(2,xs2,ys2);
    graph_sim_center_range -> SetPoint(3,xs2,ys1);
    graph_sim_center_range -> SetPoint(4,xs1,ys1);
    int count = 0;
    TString hname2 = Form("hist_%s_efficiency",fName.Data());
    TString title2 = Form("A=%dx10^{%d}, #phi=%.2f, n_{hole}=%d, r_{beam}=%.2f",fFactor,fExponent,fHoleDiameter,num_holes,fBeamRadius);
    TH1D *hist_efficiency;
    if (fBeamType==kUniformBeamProfile) hist_efficiency = new TH1D(hname2+"_u_bt"+fBeamType+"_1d",title2+Form(";Attenuation / %dx10^{%d}",fFactor,fExponent),100,fEfficiencyRange1,fEfficiencyRange2);
    if (fBeamType==kGauss2DBeamProfile) hist_efficiency = new TH1D(hname2+"_g_bt"+fBeamType+"_1d",title2+Form(";Attenuation / %dx10^{%d}",fFactor,fExponent),100,fEfficiencyRange1,fEfficiencyRange2);
    hist_efficiency -> SetStats(111110);
    if (fBeamType==kGauss2DBeamProfile) hist_efficiency -> SetLineColor(kRed);
    auto histSampleCount2 = new TH2D(hname2+"_bt"+fBeamType+"_2d",title2+";beam-center-x (mm); beam-center-y (mm)",fNumEvaluationStepsInX,xs1,xs2,fNumEvaluationStepsInY,ys1,ys2);
    histSampleCount2 -> SetContour(200);
    histSampleCount2 -> SetStats(0);
    histSampleCount2 -> GetZaxis() -> SetMaxDigits(6);
    histSampleCount2 -> GetZaxis() -> SetDecimals(1);
    for (auto iy=0; iy<fNumEvaluationStepsInY; ++iy)
    {
        double y1 = ys1 + iy*(ys2-ys1)/fNumEvaluationStepsInY;
        for (auto ix=0; ix<fNumEvaluationStepsInX; ++ix)
        {
            double x1 = xs1 + ix*(xs2-xs1)/fNumEvaluationStepsInX;
            double total = 0;
            for (auto point : holePositions) {
                double x2 = point.x();
                double y2 = point.y();
                if ( ((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2)) > fBeamRadius*fBeamRadius)
                    continue;
                double value = 0;
                if (fBeamType==kUniformBeamProfile) value = CircleIntersectionArea(x1, y1, fBeamRadius, x2, y2, 0.5*fHoleDiameterCalc);
                if (fBeamType==kGauss2DBeamProfile) value = IntegrateGaussian2DFaster(x1, y1, fBeamRadiusToSigma*fBeamRadius, fBeamRadiusToSigma*fBeamRadius, x2, y2, 0.5*fHoleDiameterCalc);
                total += value;
            }
            if (fBeamType==kUniformBeamProfile) total = total / (TMath::Pi()*fBeamRadius*fBeamRadius);
            if (fBeamType==kGauss2DBeamProfile) total = total / 1;
            double count_ratio = total/fAttenuation;
            histSampleCount2 -> SetBinContent(ix+1,iy+1,count_ratio);
            hist_efficiency -> Fill(count_ratio);
            //cout << setw(3) << count << setw(10) << x1 << setw(10) << y1 << setw(15) << total << endl;

            if ((ix==0 && iy==0))
            {
                graph_circle_u -> SetPoint(graph_circle_u->GetN(),x1,y1);
                for (auto i=0; i<=100; ++i) {
                    graph_circle_r -> SetPoint(graph_circle_r->GetN(), fBeamRadius*3*fBeamRadiusToSigma*cos(i*TMath::Pi()/50)+x1, fBeamRadius*3*fBeamRadiusToSigma*sin(i*TMath::Pi()/50)+y1);
                }
            }
            else if ((ix==fNumEvaluationStepsInX-1 && iy==fNumEvaluationStepsInY-1))
            {
                graph_circle_g -> SetPoint(graph_circle_g->GetN(),x1,y1);
                for (auto i=0; i<=100; ++i) {
                    graph_circle_3 -> SetPoint(graph_circle_3->GetN(), fBeamRadius*3*fBeamRadiusToSigma*cos(i*TMath::Pi()/50)+x1, fBeamRadius*3*fBeamRadiusToSigma*sin(i*TMath::Pi()/50)+y1);
                    graph_circle_2 -> SetPoint(graph_circle_2->GetN(), fBeamRadius*2*fBeamRadiusToSigma*cos(i*TMath::Pi()/50)+x1, fBeamRadius*2*fBeamRadiusToSigma*sin(i*TMath::Pi()/50)+y1);
                    graph_circle_1 -> SetPoint(graph_circle_1->GetN(), fBeamRadius*1*fBeamRadiusToSigma*cos(i*TMath::Pi()/50)+x1, fBeamRadius*1*fBeamRadiusToSigma*sin(i*TMath::Pi()/50)+y1);
                }
            }
            graph_sim_center -> SetPoint(graph_sim_center->GetN(),x1,y1);
            count++;
        }
    }
    graph_sim_center_range -> SetLineColor(kCyan+1);
    draw_attenuator_sketch -> Add(graph_sim_center_range,"samel");
    graph_sim_center -> SetMarkerStyle(20);
    graph_sim_center -> SetMarkerSize(0.6);
    graph_sim_center -> SetMarkerColor(kRed);
    if (fBeamType==kUniformBeamProfile) draw_attenuator_sketch -> Add(graph_circle_u,"samep");
    if (fBeamType==kUniformBeamProfile) draw_attenuator_sketch -> Add(graph_circle_r,"samel");
    if (fBeamType==kGauss2DBeamProfile) draw_attenuator_sketch -> Add(graph_circle_g,"samep");
    if (fBeamType==kGauss2DBeamProfile) draw_attenuator_sketch -> Add(graph_circle_3,"samel");
    if (fBeamType==kGauss2DBeamProfile) draw_attenuator_sketch -> Add(graph_circle_2,"samel");
    if (fBeamType==kGauss2DBeamProfile) draw_attenuator_sketch -> Add(graph_circle_1,"samel");

    draw_efficiency_2d -> Add(histSampleCount2,"colz");

    auto pv = new TPaveText();
    pv -> SetFillColor(0);
    pv -> SetFillStyle(0);
    pv -> SetBorderSize(0);
    pv -> SetTextAlign(33);
    if (fBeamType==kUniformBeamProfile) pv -> AddText("Uniform distribution");
    if (fBeamType==kGauss2DBeamProfile) pv -> AddText("2d-Gaus distribution");
    draw_efficiency_2d -> Add(pv);
    draw_efficiency_2d -> SetPaveCorner(0);

    //if (fBeamType==kUniformBeamProfile) lg_efficiency_all -> AddEntry(hist_efficiency,Form("Uniform (#mu=%.3f,#sigma=%.4f)",hist_efficiency->GetMean(),hist_efficiency->GetStdDev()),"l");
    //if (fBeamType==kGauss2DBeamProfile) lg_efficiency_all -> AddEntry(hist_efficiency,Form("2d-Gaus (#mu=%.3f,#sigma=%.4f)",hist_efficiency->GetMean(),hist_efficiency->GetStdDev()),"l");

    auto lg_efficiency_1d = new TLegend();
    lg_efficiency_1d -> AddEntry((TObject*)0,Form("A=%dx10^{%d}, #phi=%.2f, r=%.2f",fFactor,fExponent,fHoleDiameter,fBeamRadius),"");
    lg_efficiency_1d -> SetFillStyle(0);
    lg_efficiency_1d -> SetMargin(0.1);
    if (fBeamType==kUniformBeamProfile) lg_efficiency_1d -> AddEntry(hist_efficiency,Form("Uniform (#mu=%.3f,#sigma=%.4f)",hist_efficiency->GetMean(),hist_efficiency->GetStdDev()),"l");
    if (fBeamType==kGauss2DBeamProfile) lg_efficiency_1d -> AddEntry(hist_efficiency,Form("2d-Gaus (#mu=%.3f,#sigma=%.4f)",hist_efficiency->GetMean(),hist_efficiency->GetStdDev()),"l");

    auto draw_efficiency_1d = drawings -> CreateDrawing(Form("draw_%s_1d",fName.Data()),add_1d);
    draw_efficiency_1d -> SetCanvasMargin(0.12,0.12,0.12,0.12);
    draw_efficiency_1d -> SetPaveDx(0.4);
    draw_efficiency_1d -> SetPaveLineDy(0.05);
    draw_efficiency_1d -> SetStatsFillStyle(0);
    draw_efficiency_1d -> SetLegendBelowStats();
    draw_efficiency_1d -> Add(hist_efficiency);
    draw_efficiency_1d -> Add(lg_efficiency_1d);
    draw_efficiency_1d -> SetOptStat(111110);

    double Am = fAttenuation * hist_efficiency -> GetMean();
    double As = fAttenuation * hist_efficiency -> GetStdDev();

    e_cout << endl;
    e_info << fName << endl;
    e_cout << "== Input" << endl;
    e_cout << Form("   Desired attenuation A0 : %dx10^{%d} (%s)",fFactor,fExponent,LKMisc::RemoveTrailing0(fAttenuation,true).Data()) << endl;
    e_cout << Form("   Attenuator frame area  : %s mm x %s mm", LKMisc::RemoveTrailing0(fAttenuatorSize,true).Data(),LKMisc::RemoveTrailing0(fAttenuatorSize,true).Data()) << endl;
    e_cout << Form("   Attenuator active area : %s mm x %s mm", LKMisc::RemoveTrailing0(fActiveSize,true).Data(),   LKMisc::RemoveTrailing0(fActiveSize,true).Data()) << endl;
    e_cout << "   Attenuator hole diameter = " << fHoleDiameter << " mm" << endl;
    e_cout << "   Attenuator hole diameter (after) = " << fHoleDiameterCalc << " mm" << endl;
    if (fBeamType==kUniformBeamProfile) {
        e_cout << "   Beam profile : Uniform" << endl;
        e_cout << "   Beam radius  : " << fBeamRadius << " mm" << endl;
    }
    else if (fBeamType==kGauss2DBeamProfile) {
        e_cout << "   Beam profile : Gaussian" << endl;
        e_cout << "   Beam sigma   : " << fBeamRadiusToSigma*fBeamRadius << " mm" << endl;
        e_cout << "   Beam " << fBeamRadiusToSigma << "*sigma : " << fBeamRadius << " mm" << endl;
    }
    e_cout << "   Hole grid pattern type : " << fGridPattern << endl;
    e_cout << "   Attenuation calculation area  : " << Form("x=(%f,%f),y=(%f,%f)",xs1,xs2,ys1,ys2) << endl;
    e_cout << "   Attenuation calculation steps : " << fNumEvaluationStepsInX << " x " << fNumEvaluationStepsInY << endl;
    e_cout << "   Number of holes : " << num_holes << endl;
    e_cout << "== Output" << endl;
    e_cout << "   x-distance between holes : " << x_dist << " mm" << endl;
    e_cout << "   y-distance between holes : " << y_dist << " mm" << endl;
    e_cout << "   Area of the active circle: " << area_c << " mm2" << endl;
    e_cout << "   Area of the through hole : " << area_h << " mm2" << endl;
    e_cout << "   Area of the sum of holes : " << area_h*num_holes << " mm2" << endl;
    e_cout << "   Attenuation average value Am : " << Am << endl;
    e_cout << "   Attenuation std-dev value As : " << As << endl;
    e_cout << "   Attenuation efficiency Am/A0 : " << Am/fAttenuation << endl;
    e_cout << "   Attenuation error      As/A0 : " << As/fAttenuation << endl;

    if (print_holes) {
        e_cout << "== Hole positions" << endl;
        for (auto position : holePositions)
            e_cout << position.X() << ", " << position.Y() << endl;
    }

    int countDrawings = int(add_sketch) + int(add_1d) + int(add_2d);
    drawings -> SetCanvasSize(countDrawings*500,500);
    drawings -> SetCanvasDivision(countDrawings,1);
    return drawings;
}

double LKAttenuationCalculator::CircleIntersectionArea(double x1, double y1, double r1, double x2, double y2, double r2)
{
    double d = std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    if (d >= r1 + r2) {
        return 0.0;
    }
    if (d <= std::abs(r1 - r2)) {
        double smallerRadius = std::min(r1, r2);
        return M_PI * smallerRadius * smallerRadius;
    }
    double r1Squared = r1 * r1;
    double r2Squared = r2 * r2;
    double angle1 = std::acos((d * d + r1Squared - r2Squared) / (2 * d * r1));
    double angle2 = std::acos((d * d + r2Squared - r1Squared) / (2 * d * r2));
    double triangleArea = 0.5 * std::sqrt((-d + r1 + r2) * (d + r1 - r2) * (d - r1 + r2) * (d + r1 + r2));
    double segmentArea1 = r1Squared * angle1;
    double segmentArea2 = r2Squared * angle2;
    return segmentArea1 + segmentArea2 - triangleArea;
}

double LKAttenuationCalculator::Gaussian2D(double x, double y, double xc, double yc, double sigmaX, double sigmaY) {
    double norm = 1.0 / (2 * TMath::Pi() * sigmaX * sigmaY);
    double dx = (x - xc) / sigmaX;
    double dy = (y - yc) / sigmaY;
    return norm * TMath::Exp(-0.5 * (dx * dx + dy * dy));
}

double LKAttenuationCalculator::IntegrateGaussian2D(double xc, double yc, double sigmaX, double sigmaY, double x0, double y0, double r0, int nPoints)
{
    double integral = 0.0;
    double dTheta = 2 * TMath::Pi() / nPoints; // Step size in angle
    double dr = r0 / nPoints; // Step size in radius

    for (int i = 0; i < nPoints; ++i) {
        double r = i * dr;
        for (int j = 0; j < nPoints; ++j) {
            double theta = j * dTheta;
            double x = x0 + r * TMath::Cos(theta);
            double y = y0 + r * TMath::Sin(theta);

            // Accumulate the Gaussian value at this point
            integral += Gaussian2D(x, y, xc, yc, sigmaX, sigmaY) * r * dr * dTheta;
        }
    }

    return integral;
}

double LKAttenuationCalculator::IntegrateGaussian2DFast(double xc, double yc, double sigmaX, double sigmaY, double x0, double y0, double r0, int nPoints)
{
    double cx = TMath::Sqrt((x0-xc)*(x0-xc) + (y0-yc)*(y0-yc));
    return IntegrateGaussian2D_SumGamma(sigmaX, cx, r0);
}

double LKAttenuationCalculator::IntegrateGaussian2D_SumGamma(double sigma, double cx, double r0)
{
    cout << "cx = " << cx << endl;
    cx = cx/sigma;
    r0 = r0/sigma;
    double valueSum = 0;
    for (int k=0; k<4; ++k)
    {
        double k_factorial = TMath::Factorial(k);
        double value_add = TMath::Power(cx*cx/2,k)/(k_factorial*k_factorial) * TMath::Gamma(k+1,r0*r0/2);
        cout << k << ") " << TMath::Power(cx*cx/2,k) << " / " << (k_factorial*k_factorial) << " * " << TMath::Gamma(k+1,r0*r0/2) << " = " << value_add << endl;
        valueSum += value_add;
    }
    cout << TMath::Exp(-cx*cx/2) << endl;
    cout << TMath::Exp(-cx*cx/2) * valueSum << endl;
    double integral = 1 - TMath::Exp(-cx*cx/2) * valueSum;
    cout << integral << endl;
    return integral;
}

double LKAttenuationCalculator::IntegrateGaussian2DFaster(double xc, double yc, double sigmaX, double sigmaY, double x0, double y0, double r0)
{
    double dx = (x0 - xc) / sigmaX;
    double dy = (y0 - yc) / sigmaY;
    double gaussianValue = (1.0 / (2 * TMath::Pi() * sigmaX * sigmaY)) *
                           TMath::Exp(-0.5 * (dx * dx + dy * dy));
    double circleArea = TMath::Pi() * r0 * r0;
    return circleArea * gaussianValue;
}

TGraph *LKAttenuationCalculator::NewGraph(TString name, int mst, double msz, int mcl, int lst, int lsz, int lcl)
{
    auto graph = new TGraph();
    graph -> SetName(name);
    if (mst<=0) mst = 20;
    if (msz<=0) msz = 1;
    if (mcl<0) mcl = kBlack;
    graph -> SetMarkerStyle(mst);
    graph -> SetMarkerSize(msz);
    graph -> SetMarkerColor(mcl);
    if (lst<0) lst = 1;
    if (lsz<0) lsz = 1;
    if (lcl<0) lcl = mcl;
    graph -> SetLineStyle(lst);
    graph -> SetLineWidth(lsz);
    graph -> SetLineColor(lcl);
    graph -> SetFillStyle(0);
    return graph;
}

TGraphErrors *LKAttenuationCalculator::NewGraphErrors(TString name, int mst, double msz, int mcl, int lst, int lsz, int lcl)
{
    auto graph = new TGraphErrors();
    graph -> SetName(name);
    if (mst<=0) mst = 20;
    if (msz<=0) msz = 1;
    if (mcl<0) mcl = kBlack;
    graph -> SetMarkerStyle(mst);
    graph -> SetMarkerSize(msz);
    graph -> SetMarkerColor(mcl);
    if (lst<0) lst = 1;
    if (lsz<0) lsz = 1;
    if (lcl<0) lcl = mcl;
    graph -> SetLineStyle(lst);
    graph -> SetLineWidth(lsz);
    graph -> SetLineColor(lcl);
    graph -> SetFillStyle(0);
    return graph;
}
