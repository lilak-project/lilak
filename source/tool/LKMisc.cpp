#include "LKMisc.h"
#include "LKPainter.h"

#include "TColorWheel.h"
#include "TObjString.h"
#include "TObjArray.h"
#include "TMarker.h"
#include "TColor.h"
#include "TH2D.h"
#include "TMath.h"
#include "TText.h"
#include "TLatex.h"

#include <cfloat>

//#define DEBUG_MISC_FWHM
//#define DEBUG_MISC_FINDPED

ClassImp(LKMisc);

LKMisc* LKMisc::fInstance = nullptr;

LKMisc* LKMisc::GetMisc() {
    if (fInstance == nullptr)
        fInstance = new LKMisc();
    return fInstance;
}

LKMisc::LKMisc() {
    fInstance = this;
}

double LKMisc::FWHM(double *buffer, int length)
{
    int xPeak = 0;
    double amplitude = -DBL_MAX;
    for (auto x=0; x<length; ++x) {
        if (amplitude<buffer[x]) {
            xPeak = x;
            amplitude = buffer[x];
        }
    }

#ifdef DEBUG_MISC_FWHM
    lk_debug << "x y at max = " << xPeak << " " << amplitude << endl;
#endif

    double pedestal = LKMisc::EvalPedestal(buffer, length);
#ifdef DEBUG_MISC_FWHM
    lk_debug << "length: " << length << endl;
    lk_debug << "xpeak: " << xPeak << endl;
    lk_debug << "amplitude: " << amplitude << endl;
    lk_debug << "pedestal: " << pedestal << endl;
#endif
    double xMin, xMax;
    double width = LKMisc::FWHM(buffer, length, xPeak, amplitude, pedestal, xMin, xMax);

    return width;
}

double LKMisc::FWHM(double *buffer, int length, int xPeak, double amplitude, double pedestal, double &xMin, double &xMax)
{
    const int numDivisions = 20;
    const auto xResolution = 1./numDivisions;
    const double ratio = 0.5;
    const double threshold = ratio * (amplitude - pedestal) + pedestal;
#ifdef DEBUG_MISC_FWHM
    lk_debug << threshold << endl;
#endif

    double dy0 = DBL_MAX; // mininum value of dy in the lower side
    double dy1 = DBL_MAX; // mininum value of dy in the upper side
    int    x0=0, x1=0, x2=0, x3=0;
    double y0=0, y1=0, y2=0, y3=0;

    if (xPeak>0)
        for (auto x=xPeak-1; x>=0; --x)
        {
            if (buffer[x]>amplitude) {
                xPeak = x;
                amplitude = buffer[x];
                continue;
            }
            break;
        }

    if (xPeak+1<length)
        for (auto x=xPeak+1; x<length; ++x)
        {
            if (buffer[x]>amplitude) {
                xPeak = x;
                amplitude = buffer[x];
                continue;
            }
            break;
        }
#ifdef DEBUG_MISC_FWHM
    lk_debug << "x,a = " << xPeak << ", " << amplitude << endl;
#endif

    for (auto x=xPeak; x>=0; --x) {
#ifdef DEBUG_MISC_FWHM
        lk_debug << " ? " << x << " " << buffer[x] << " " << threshold << " " << (buffer[x]<threshold) << endl;
#endif
        if (buffer[x]<threshold) {
#ifdef DEBUG_MISC_FWHM
            lk_debug << " ! " << buffer[x] << " " << threshold << endl;
#endif
            x0 = x;
            x1 = x+1;
            y0 = buffer[x0];
            y1 = buffer[x1];
            break;
        }
    }

    for (auto x=xPeak; x<length; ++x) {
        if (buffer[x]<threshold) {
            x2 = x-1;
            x3 = x;   
            y2 = buffer[x2];
            y3 = buffer[x3];
            break;
        }
     }

#ifdef DEBUG_MISC_FWHM
     lk_debug << "x0 y0 = " << x0 << " " << buffer[x0] << " " << y0 << endl;
     lk_debug << "x1 y1 = " << x1 << " " << buffer[x1] << " " << y1 << endl;
     lk_debug << "x2 y2 = " << x2 << " " << buffer[x2] << " " << y2 << endl;
     lk_debug << "x3 y3 = " << x3 << " " << buffer[x3] << " " << y3 << endl;
     lk_debug << endl;
#endif

    double sy = (y1-y0)/numDivisions;
    for (auto div=0; div<=numDivisions; ++div)
    {
        auto y = y0 + div*sy;
        auto dy = TMath::Abs(y-threshold);
#ifdef DEBUG_MISC_FWHM
        lk_debug << div << " lower: y dy = " << y << " " << dy << endl;
#endif
        if (dy>dy0) {
            if (abs(dy)>abs(dy0)) xMin = x0 + (div-1)*xResolution;
            else                  xMin = x0 + (div+0)*xResolution;
#ifdef DEBUG_MISC_FWHM
            if (abs(dy)>abs(dy0)) lk_debug << " min " << dy0 << " " << xMin << endl;
            else                  lk_debug << " min " << dy  << " " << xMin << endl;
#endif
            break;
        }
        dy0 = dy;
    }

    sy = (y3-y2)/numDivisions;
    for (auto div=0; div<=numDivisions; ++div)
    {
        auto y = y2 + div*sy;
        auto dy = TMath::Abs(y-threshold);
#ifdef DEBUG_MISC_FWHM
        lk_debug << div << " upper: y dy = " << y << " " << dy << endl;
#endif
        if (dy>dy1) {
            if (abs(dy)>abs(dy1)) xMax = x2 + (div-1)*xResolution;
            else                  xMax = x2 + (div+0)*xResolution;
#ifdef DEBUG_MISC_FWHM
            if (abs(dy)>abs(dy0)) lk_debug << " max " << dy0 << " " << xMax << endl;
            else                  lk_debug << " max " << dy  << " " << xMax << endl;
#endif
            break;
        }
        dy1 = dy;
    }

    double width = xMax - xMin;

    return width;
}

double LKMisc::EvalPedestal(double *buffer, int length, int method)
{
    if (method==0) return EvalPedestalSamplingMethod(buffer, length);
    e_error << method << endl;
    return 0;
}

double LKMisc::EvalPedestalSamplingMethod(double *buffer, int length, int sampleLength, double cvCut, bool subtractPedestal)
{
    int numPedestalSamples = floor(length/sampleLength);
    int numPedestalSamplesM1 = numPedestalSamples - 1;
    int numTbSampleLast = sampleLength + length - numPedestalSamples*sampleLength;
    

    bool* usedSample = new bool[numPedestalSamples];
    int*  stddevSample = new int[numPedestalSamples];
    int*  pedestalSample = new int[numPedestalSamples];

    for (auto i=0; i<numPedestalSamples; ++i) {
        usedSample[i] = false;
        stddevSample[i] = 0.;
        pedestalSample[i] = 0.;
    }

    int tbGlobal = 0;
    for (auto iSample=0; iSample<numPedestalSamplesM1; ++iSample) {
        for (int iTb=0; iTb<sampleLength; iTb++) {
            pedestalSample[iSample] += buffer[tbGlobal];
            stddevSample[iSample] += buffer[tbGlobal]*buffer[tbGlobal];
            tbGlobal++;
        }
        pedestalSample[iSample] = pedestalSample[iSample] / sampleLength;
        stddevSample[iSample] = stddevSample[iSample] / sampleLength;
        stddevSample[iSample] = sqrt(stddevSample[iSample] - pedestalSample[iSample]*pedestalSample[iSample]);
    }
    for (int iTb=0; iTb<numTbSampleLast; iTb++) {
        pedestalSample[numPedestalSamplesM1] = pedestalSample[numPedestalSamplesM1] + buffer[tbGlobal];
        tbGlobal++;
    }
    pedestalSample[numPedestalSamplesM1] = pedestalSample[numPedestalSamplesM1] / numTbSampleLast;
#ifdef DEBUG_MISC_FINDPED
    for (auto iSample=0; iSample<numPedestalSamples; ++iSample)
        lk_debug << "i-" << iSample << " " << pedestalSample[iSample] << ">0?, " << stddevSample[iSample] << " " << stddevSample[iSample]/pedestalSample[iSample] << "<?" << cvCut << endl;
#endif

    int countBelowCut = 0;
    for (auto iSample=0; iSample<numPedestalSamples; ++iSample) {
        if (pedestalSample[iSample]>0&&stddevSample[iSample]/pedestalSample[iSample]<cvCut)
            countBelowCut++;
    }
#ifdef DEBUG_MISC_FINDPED
    lk_debug << "countBelowCut = " << countBelowCut << endl;
#endif

    double pedestalDiffMin = DBL_MAX;
    double pedestalMeanRef = 0;
    int idx1 = 0;
    int idx2 = 0;
    if (countBelowCut>=2) {
        for (auto iSample=0; iSample<numPedestalSamples; ++iSample) {
            if (pedestalSample[iSample]<=0) continue;
            if (stddevSample[iSample]/pedestalSample[iSample]>=cvCut) continue;
            for (auto jSample=0; jSample<numPedestalSamples; ++jSample) {
                if (iSample>=jSample) continue;
                if (pedestalSample[jSample]<=0) continue;
                if (stddevSample[jSample]/pedestalSample[jSample]>=cvCut) continue;
                double diff = abs(pedestalSample[iSample] - pedestalSample[jSample]);
#ifdef DEBUG_MISC_FINDPED
                lk_debug << iSample << "|" << jSample << ") " << stddevSample[jSample] << " " << pedestalSample[jSample] << endl;
                lk_debug << iSample << "|" << jSample << ") " << diff << " <? " << pedestalDiffMin << endl;
#endif
                if (diff<pedestalDiffMin) {
                    pedestalDiffMin = diff;
                    pedestalMeanRef = 0.5 * (pedestalSample[iSample] + pedestalSample[jSample]);
                    idx1 = iSample;
                    idx2 = jSample;
                }
            }
        }
    }
    else {
        for (auto iSample=0; iSample<numPedestalSamples; ++iSample) {
            for (auto jSample=0; jSample<numPedestalSamples; ++jSample) {
                if (iSample>=jSample) continue;
                double diff = abs(pedestalSample[iSample] - pedestalSample[jSample]);
                if (diff<pedestalDiffMin) {
                    pedestalDiffMin = diff;
                    pedestalMeanRef = 0.5 * (pedestalSample[iSample] + pedestalSample[jSample]);
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

#ifdef DEBUG_MISC_FINDPED
    lk_debug << "diff min : " << pedestalDiffMin << endl;
    lk_debug << "ref-diff : " << pedestalMeanRef << endl;
    lk_debug << "ref-error-part: " << pedestalErrorRefSample << endl;
#endif

    tbGlobal = 0;
    for (auto iSample=0; iSample<numPedestalSamplesM1; ++iSample)
    {
        double diffSample = abs(pedestalMeanRef - pedestalSample[iSample]);
#ifdef DEBUG_MISC_FINDPED
        lk_debug << "i-" << iSample << " " << diffSample << "(" << pedestalMeanRef << " - " << pedestalSample[iSample]<< ")" << " <? " << pedestalErrorRefSample << endl;
#endif
        if (diffSample<pedestalErrorRefSample)
        {
            usedSample[iSample] = true;
            countNumPedestalTb += sampleLength;
            for (int iTb=0; iTb<sampleLength; iTb++)
            {
                pedestalFinal += buffer[tbGlobal];
                tbGlobal++;
            }
#ifdef DEBUG_MISC_FINDPED
            lk_debug << "1) diffSample < pedestalErrorRefSample : i-" << iSample << " diff=" << diffSample << " pdFinal=" << pedestalFinal << " countNumPedestalTb=" << countNumPedestalTb << endl;
#endif
        }
        else {
            tbGlobal += sampleLength;
#ifdef DEBUG_MISC_FINDPED
            lk_debug << "2) diffSample > pedestalErrorRefSample : i-" << iSample << " diff=" << diffSample << " pdFinal=" << pedestalFinal << " countNumPedestalTb=" << countNumPedestalTb << endl;
#endif
        }
    }
    double diffSample = abs(pedestalMeanRef - pedestalSample[numPedestalSamplesM1]);
    if (diffSample<pedestalErrorRefSample)
    {
        usedSample[numPedestalSamplesM1] = true;
        countNumPedestalTb += numTbSampleLast;
        for (int iTb=0; iTb<numTbSampleLast; iTb++) {
            pedestalFinal += buffer[tbGlobal];
            tbGlobal++;
        }
#ifdef DEBUG_MISC_FINDPED
        lk_debug << numPedestalSamplesM1 << " diff=" << diffSample << " " << pedestalFinal/countNumPedestalTb << " " << countNumPedestalTb << endl;
#endif
    }

#ifdef DEBUG_MISC_FINDPED
    lk_debug << pedestalFinal << " " <<  countNumPedestalTb << endl;;
#endif
    if (pedestalFinal!=0||countNumPedestalTb!=0)
        pedestalFinal = pedestalFinal / countNumPedestalTb;
    else
        pedestalFinal = 4096;

#ifdef DEBUG_MISC_FINDPED
    lk_debug << pedestalFinal << endl;
#endif

    if (subtractPedestal)
        for (auto iTb=0; iTb<length; ++iTb)
            buffer[iTb] = buffer[iTb] - pedestalFinal;

    return pedestalFinal;
}

void LKMisc::DrawColors()
{
    auto cvs1 = e_painter() -> CanvasResize("CvsLKMiscColors",500,200,0.4);
    cvs1 -> DrawColorTable();

    TColorWheel *colorWheel = new TColorWheel();
    auto cvs2 = e_painter() -> CanvasResize("CvsLKMiscColors2",800,825,0.6);
    colorWheel -> SetCanvas(cvs2);
    colorWheel -> Draw();
}

void LKMisc::DrawMarkers()
{
    auto cvs = e_painter() -> CanvasResize("CvsLKMiscMarkers",600,400,0.6);
    cvs -> SetMargin(0.02,0.02,0.02,0.02);
    auto hist = new TH2D("HistLKMiscUserMarkers","",100,0.2,10.8,100,0.1,5.6);
    hist -> SetStats(0);
    hist -> SetStats(0);
    hist -> GetXaxis() -> SetLabelOffset(100);
    hist -> GetYaxis() -> SetLabelOffset(100);
    hist -> GetXaxis() -> SetNdivisions(0);
    hist -> GetYaxis() -> SetNdivisions(0);
    hist -> Draw();
    int i = 0;
    for (auto y=5; y>=1; --y) {
        for (auto x=1; x<=10; ++x) {
            if (i==0) { i++; continue; }
            auto m = new TMarker(x,y,i);
            m -> SetMarkerSize(3.5);
            auto t = new TText(x,y-0.42,Form("%d",i));
            t -> SetTextSize(0.035);
            t -> SetTextAlign(22);
            if ((i>=20&&i<=29)||i==33||i==34) t -> SetTextColor(kBlue);
            if ((i>=24&&i<=28)||i==30||i==32) t -> SetTextColor(kRed);
            if (i>=9&&i<=19) {
                t -> SetTextColor(kGray);
                m -> SetMarkerColor(kGray);
            }
            m -> Draw();
            t -> Draw();
            i++;
        }
    }
}

void LKMisc::DrawColors(vector<TString> colors)
{
    int nx = 10;
    int nc = colors.size();
    int ny = 5;
    if (nc>nx*ny) { ny = 6; nx = 12; }
    if (nc>nx*ny) { ny = 7; nx = 14; }
    if (nc>nx*ny) { ny = 8; nx = 16; }
    if (nc>nx*ny) { ny = 9; nx = 18; }
    if (nc>nx*ny) { ny =10; nx = 20; }

    auto cvs = e_painter() -> CanvasResize("CvsLKMiscUserColors",60*nx,80*ny, 0.1*ny);
    cvs -> SetMargin(0.02,0.02,0.02,0.02);
    auto hist = new TH2D("HistLKMiscUserColors","",100,0.2,nx+.8,100,0.1,1.1*ny);
    hist -> SetStats(0);
    hist -> GetXaxis() -> SetLabelOffset(100);
    hist -> GetYaxis() -> SetLabelOffset(100);
    hist -> GetXaxis() -> SetNdivisions(0);
    hist -> GetYaxis() -> SetNdivisions(0);
    hist -> Draw();
    int countColors = 0;
    for (auto y=ny; y>=1; --y)
    {
        for (auto x=1; x<=nx; ++x)
        {
            if (countColors>=nc) break;
            int color = TColor::GetColor(colors[countColors]);
            auto m = new TMarker(x,y,21);
            m -> SetMarkerSize(3.5);
            m -> SetMarkerColor(color);
            auto t = new TText(x,y-0.42,Form("%s",colors[countColors].Data()));
            //auto t = new TText(x,y-0.42,Form("%d (%d)",countColors,color));
            //auto t = new TText(x,y-0.42,Form("%d",countColors));
            t -> SetTextSize(0.025);
            t -> SetTextFont(42);
            t -> SetTextColor(color);
            t -> SetTextAlign(22);
            m -> Draw();
            t -> Draw();
            countColors++;
        }
    }
}

void LKMisc::DrawColors(vector<int> colors)
{
    int nx = 10;
    int nc = colors.size();
    int ny = 5;
    auto cvs = e_painter() -> CanvasResize("CvsLKMiscUserColors",600,80*ny,0.5);
    cvs -> SetMargin(0.02,0.02,0.02,0.02);
    auto hist = new TH2D("HistLKMiscUserColors","",100,0.2,10.8,100,0.1,1.1*ny);
    hist -> SetStats(0);
    hist -> GetXaxis() -> SetLabelOffset(100);
    hist -> GetYaxis() -> SetLabelOffset(100);
    hist -> GetXaxis() -> SetNdivisions(0);
    hist -> GetYaxis() -> SetNdivisions(0);
    hist -> Draw();
    int countColors = 0;
    for (auto y=ny; y>=1; --y)
    {
        for (auto x=1; x<=nx; ++x)
        {
            if (countColors>=nc) break;
            int color = colors[countColors];
            auto m = new TMarker(x,y,21);
            m -> SetMarkerSize(3.5);
            m -> SetMarkerColor(color);
            //auto t = new TText(x,y-0.42,Form("%d (%d)",countColors,color));
            auto t = new TText(x,y-0.42,Form("%d",countColors));
            t -> SetTextSize(0.035);
            t -> SetTextFont(42);
            t -> SetTextAlign(22);
            m -> Draw();
            t -> Draw();
            countColors++;
        }
    }
}

void LKMisc::DrawFonts()
{
    TString font_names[16][4] = {{"","","",""},
        {" ", "I", "Free Serif Italic"         ,"Times-Italic"},
        {"B", " ", "Free Serif Bold"           ,"Times-Bold"},
        {"B", "I", "Free Serif Bold Italic"    ,"Times-BoldItalic"},
        {" ", " ", "Tex Gyre Regular"          ,"Helvetica"},
        {" ", "I", "Tex Gyre Italic"           ,"Helvetica-Oblique"},
        {"B", " ", "Tex Gyre Bold"             ,"Helvetica-Bold"},
        {"B", "I", "Tex Gyre Bold Italic"      ,"Helvetica-BoldOblique"},
        {" ", " ", "Free Mono"                 ,"Courier"},
        {" ", " ", "Free Mono Oblique"         ,"Courier-Oblique"},
        {"B", "I", "Free Mono Bold"            ,"Courier-Bold"},
        {"B", "I", "Free Mono Bold Oblique"    ,"Courier-BoldOblique"},
        {" ", " ", "Symbol"                    ,"Symbol"},
        {" ", " ", "Free Serif"                ,"Times-Roman"},
        {" ", " ", "Wingdings"                 ,"ZapfDingbats"},
        {" ", " ", "Greek letters"             ,"Greek letters"}};
    int nt = 15;
    TString equation_example = "d#sigma/d#Omega = ((Z_{1}Z_{2}e^{2})/(16#pi#epsilon_{0}E_{kin}))^{2}#scale[1.4]{/}sin^{4}(#theta/2)";
    auto cvs = LKPainter::GetPainter() -> CanvasResize("CvsLKMiscFonts",1300,600,0.8);
    cvs -> SetMargin(0.02,0.02,0.02,0.02);
    auto hist = new TH2D("HistLKMiscUserColors","",100,0,12,100,0,nt+1);
    hist -> SetStats(0);
    hist -> GetXaxis() -> SetLabelOffset(100);
    hist -> GetYaxis() -> SetLabelOffset(100);
    hist -> GetXaxis() -> SetNdivisions(0);
    hist -> GetYaxis() -> SetNdivisions(0);
    hist -> Draw();
    for (auto i=1; i<=nt; ++i)
    {
        int font_number = i*10+2;
        double x = 1.2;
        double y = nt+1-i;
        {
            auto tt3 = new TText(0.40,y,Form("%d",font_number)); tt3 -> SetTextFont(42);
            auto tt1 = new TText(0.80,y,font_names[i][0]); tt1 -> SetTextFont(62);
            auto tt2 = new TText(1.00,y,font_names[i][1]); tt2 -> SetTextFont(12);
            for (auto tt : {tt1,tt2,tt3}) {
                tt -> SetTextSize(0.03);
                tt -> SetTextColor(kGray+2);
                tt -> SetTextAlign(22);
                tt -> Draw();
            }
        }
        TString text_example, note;
        if (i==4) note = "*TAxis default";
        if (i==6) note = "*TText default";
        TString equation_examples = equation_example;
        text_example = text_example + note + " [" + font_names[i][2] + "] Equation look like: " + equation_examples;
        auto tte = new TLatex(x,y,text_example);
        tte -> SetTextSize(0.03);
        tte -> SetTextFont(font_number);
        tte -> SetTextAlign(12);
        tte -> SetTextColor(kGray+1);
        if (i==2||i==13) tte -> SetTextColor(kBlue);
        else if (i==4||i==6) tte -> SetTextColor(kRed+1);
        tte -> Draw();
    }
}

int LKMisc::Index1(TString option, TString pat)
{
    int idx = option.Index(pat);

    auto iComment = pat.Index("#",1);
    TString comment;
    if (iComment>0) {
        comment = pat(iComment+1,pat.Sizeof());
        pat = pat(0,iComment);
        pat = pat.Strip(TString::kBoth);
        comment = comment.Strip(TString::kBoth);
    }

    bool help = false;
    if (option.Index("?")>0) {
        if (comment) cout << "- " << pat << "  # " << comment << endl;
        else cout << "- " << pat << endl;
        help = true;
    }

    return idx;
}

TString LKMisc::FindOption(TString &option, TString name, bool removeAfter, int addValue)
{
    TString opcopy = Form(":%s:",option.Data());
    auto iComment = name.Index("#",1);
    TString comment;
    if (iComment>0) {
        comment = name(iComment+1,name.Sizeof());
        name = name(0,iComment);
        name = name.Strip(TString::kBoth);
        comment = comment.Strip(TString::kBoth);
    }

    bool help = false;
    if (name.Index("?||help")<0&&name.Index("?")<0&&name.Index("help")<0&&CheckOption(option,"?||help",true,0)) {
        if (comment) cout << "- " << name << "  # " << comment << endl;
        else cout << "- " << name << endl;
        help = true;
    }

    if (name.Index("||")>0)
    {
        TString foundOption;
        auto nameList = name.Tokenize("||");
        auto numNames = nameList -> GetEntries();
        for (auto iName=0; iName<numNames; ++iName)
        {
            TString tokName = ((TObjString *) nameList->At(iName)) -> GetString();
            tokName = tokName.Strip(TString::kBoth);
            TString found = FindOption(option,tokName,removeAfter,addValue);
            if (foundOption.IsNull()) foundOption = found;
            else foundOption = foundOption + "||" + found;
        }
        if (help) option = option + ":?";
        return foundOption;
    }
    else
    {
        name = name.Strip(TString::kBoth);
        int idxName = opcopy.Index(Form(":%s",name.Data()));
        if (idxName<0) {
            if (help) option = option + ":?";
            return "";
        }
        idxName = idxName + 1;
        int idxColon = opcopy.Index(":",idxName);
        int idxEqual = opcopy.Index("=",idxName);
        int idxNext = idxName + name.Sizeof()-1;
        if (idxNext!=idxEqual && idxNext!=idxColon) {
            if (help) option = option + ":?";
            return "";
        }
        //return Form("%s*",name); // found option which do not exact

        if (idxEqual<0||idxEqual>idxColon) {
            if (removeAfter)
                opcopy.ReplaceAll(Form(":%s:",name.Data()),":");
            option = opcopy(1,opcopy.Sizeof()-3);
            if (help) option = option + ":?";
            return name; // found option but value do not exist
        }
        TString value = opcopy(idxEqual+1,idxColon-idxEqual-1);
        if (removeAfter) {
            opcopy.ReplaceAll(Form(":%s=%s:",name.Data(),value.Data()),":");
            option = opcopy(1,opcopy.Sizeof()-3);
        }
        if (addValue!=0) {
            if (removeAfter)
                e_error << "Cannot add value while removing option!" << endl;
            if (value.IsDec()==false)
                e_error << "Cannot add value to non decimal number!" << endl;
            int valueInt = value.Atoi();
            valueInt = valueInt + addValue;
            opcopy.ReplaceAll(Form(":%s=%s:",name.Data(),value.Data()),Form(":%s=%d:",name.Data(),valueInt));
            option = opcopy(1,opcopy.Sizeof()-3);
        }
        if (help) option = option + ":?";
        return value;
    }
}

int LKMisc::FindOptionInt(TString option, TString name, int emptyValue)
{
    TString value = LKMisc::FindOption(option, name, false, false);
    if (value.IsNull())
        return emptyValue;
    else
        return value.Atoi();
}

double LKMisc::FindOptionDouble(TString option, TString name, double emptyValue)
{
    TString value = LKMisc::FindOption(option, name, false, false);
    if (value.IsNull())
        return emptyValue;
    else
        return value.Atof();
}

TString LKMisc::FindOptionString(TString option, TString name, TString emptyValue)
{
    TString value = LKMisc::FindOption(option, name, false, false);
    if (value.IsNull())
        return emptyValue;
    else
        return value;
}

bool LKMisc::CheckOption(TString &option, TString name, bool removeAfter, int addValue)
{
    auto value = LKMisc::FindOption(option, name, removeAfter, addValue);
    if (value.IsNull())
        return false;
    return true;
}

bool LKMisc::RemoveOption(TString &option, TString name)
{
    auto value = LKMisc::FindOption(option, name, true);
    return (value.IsNull()==false);
}

void LKMisc::AddOption(TString &original, TString adding)
{
    if (LKMisc::CheckOption(original,adding)==false)
        original = original + ":" + adding;
}

void LKMisc::AddOption(TString &original, TString adding, TString value)
{
    LKMisc::RemoveOption(original,adding);
    TString option = Form("%s=%s",adding.Data(),value.Data());
    original = original + ":" + option;
}

void LKMisc::AddOption(TString &original, TString adding, double value)
{
    LKMisc::RemoveOption(original,adding);
    TString option = Form("%s=%f",adding.Data(),value);
    while (option[option.Sizeof()-2]=='0')
        option = option(0,option.Sizeof()-2);
    original = original + ":" + option;
}

void LKMisc::AddOption(TString &original, TString adding, int value)
{
    LKMisc::RemoveOption(original,adding);
    TString option = Form("%s=%d",adding.Data(),value);
    original = original + ":" + option;
}

bool LKMisc::ValueIsInArray(TString value, vector<TString> array)
{
    for (auto sample : array)
        if (value==sample)
            return true;
    return false;
}

bool LKMisc::ValueIsInArray(double value, vector<double> array)
{
    for (auto sample : array)
        if (value==sample)
            return true;
    return false;
}

bool LKMisc::ValueIsInArray(int value, vector<int> array)
{
    for (auto sample : array)
        if (value==sample)
            return true;
    return false;
}

TString LKMisc::RemoveTrailing0(double value, bool removeLastDot)
{
    TString valstr = Form("%f",value);
    return RemoveTrailing0(valstr, removeLastDot);
}

TString LKMisc::RemoveTrailing0(TString value, bool removeLastDot)
{
    auto posDot = value.Index(".");
    auto posLast = value.Sizeof()-2;
    auto lastStr = TString(value(posLast));
    if (posDot>=0&&posDot<posLast) {
        while (lastStr=="0") {
            value.Remove(posLast);
            posLast = value.Sizeof()-2;
            lastStr = TString(value(posLast));
        }
    }
    if (removeLastDot) {
        lastStr = TString(value(posLast));
        if (lastStr==".")
            value.Remove(posLast);
    }
    return value;
}
