#include "LKSAM.h"
#include "LKLogger.h"
#include "TCanvas.h"
#include "TH1.h"
#include "TH2.h"

using namespace std;

ClassImp(LKSAM)

LKSAM* LKSAM::fInstance = nullptr;
LKSAM* LKSAM::GetSAM()
{
    if (fInstance != nullptr)
        return fInstance;
    return new LKSAM();
}

LKSAM::LKSAM()
{
    fInstance = this;
}

void LKSAM::InitBufferDouble(int length)
{
    // Check if buffer is already initialized and current size is smaller than requested
    if (fBufferDouble == nullptr || fBufferDoubleSize < length) {
        // If buffer exists, delete it
        if (fBufferDouble != nullptr) {
            delete[] fBufferDouble;
        }
        // Create new buffer of the requested length
        fBufferDouble = new double[length];
        fBufferDoubleSize = length;
        //std::cout << "Buffer initialized with size: " << length << std::endl;
    } else {
        //std::cout << "Buffer is already initialized with sufficient size." << std::endl;
    }
}

double LKSAM::EvalPedestal(double *buffer, int length, int method)
{
    if (method==0) return EvalPedestalSamplingMethod(buffer, length);
    e_error << method << endl;
    return 0;
}

double LKSAM::EvalPedestalSamplingMethod(double *buffer, int length, int sampleLength, double cvCut, bool subtractPedestal)
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

    int countBelowCut = 0;
    for (auto iSample=0; iSample<numPedestalSamples; ++iSample) {
        if (pedestalSample[iSample]>0&&stddevSample[iSample]/pedestalSample[iSample]<cvCut)
            countBelowCut++;
    }

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

    tbGlobal = 0;
    for (auto iSample=0; iSample<numPedestalSamplesM1; ++iSample)
    {
        double diffSample = abs(pedestalMeanRef - pedestalSample[iSample]);
        if (diffSample<pedestalErrorRefSample)
        {
            usedSample[iSample] = true;
            countNumPedestalTb += sampleLength;
            for (int iTb=0; iTb<sampleLength; iTb++)
            {
                pedestalFinal += buffer[tbGlobal];
                tbGlobal++;
            }
        }
        else {
            tbGlobal += sampleLength;
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
    }

    if (pedestalFinal!=0||countNumPedestalTb!=0)
        pedestalFinal = pedestalFinal / countNumPedestalTb;
    else
        pedestalFinal = 4096;

    if (subtractPedestal)
        for (auto iTb=0; iTb<length; ++iTb)
            buffer[iTb] = buffer[iTb] - pedestalFinal;

    return pedestalFinal;
}

double LKSAM::FWHM(double *buffer, int length)
{
    int iPeak = 0;
    double amplitude = -DBL_MAX;
    for (auto x=0; x<length; ++x) {
        if (amplitude<buffer[x]) {
            iPeak = x;
            amplitude = buffer[x];
        }
    }

    double pedestal = EvalPedestal(buffer, length);
    double iMin, iMax, half;
    double width = FWHM(buffer, length, iPeak, amplitude, pedestal, iMin, iMax, half);

    return width;
}

double LKSAM::FWHM(TH1D* hist, double xPeak, double amplitude, double pedestal, double &xMin, double &xMax, double &half)
{
    int length = hist -> GetNbinsX();
    InitBufferDouble(length);
    double *histBuffer = hist -> GetArray();
    for (auto i=0; i<length; ++i)
        fBufferDouble[i] = histBuffer[i+1];
    int iPeak = hist -> FindBin(xPeak);
    double binWidth = hist -> GetBinWidth(1);
    double iMin, iMax;
    FWHM(fBufferDouble, length, iPeak, amplitude, pedestal, iMin, iMax, half);

    //lk_debug << iMin << " " << iMax << endl;
    int iMin0 = int(iMin);
    double iMin1 = iMin - iMin0;
    double xMin0 = hist -> GetBinCenter(iMin0+1);
    double xMin1 = binWidth * iMin1;
    xMin = xMin0 + xMin1;

    int iMax0 = int(iMax);
    double iMax1 = iMax - iMax0;
    double xMax0 = hist -> GetBinCenter(iMax0+1);
    double xMax1 = binWidth * iMax1;
    xMax = xMax0 + xMax1;

    return xMax - xMin;
}

double LKSAM::FWHM(double *buffer, int length, int iPeak, double amplitude, double pedestal, double &iMin, double &iMax, double &half)
{
    const int numDivisions = 20;
    const auto xResolution = 1./numDivisions;
    const double ratio = 0.5;
    const double threshold = ratio * (amplitude - pedestal) + pedestal;
    half = threshold;

    double dy0 = DBL_MAX; // mininum value of dy in the lower side
    double dy1 = DBL_MAX; // mininum value of dy in the upper side
    int    x0=0, x1=0, x2=0, x3=0;
    double y0=0, y1=0, y2=0, y3=0;

    if (iPeak>0)
        for (auto x=iPeak-1; x>=0; --x) {
            if (buffer[x]>amplitude) {
                iPeak = x;
                amplitude = buffer[x];
                continue;
            }
            break;
        }

    if (iPeak+1<length)
        for (auto x=iPeak+1; x<length; ++x)
        {
            if (buffer[x]>amplitude) {
                iPeak = x;
                amplitude = buffer[x];
                continue;
            }
            break;
        }

    for (auto x=iPeak; x>=0; --x) {
        if (buffer[x]<threshold) {
            x0 = x;
            x1 = x+1;
            y0 = buffer[x0];
            y1 = buffer[x1];
            break;
        }
    }

    for (auto x=iPeak; x<length; ++x) {
        if (buffer[x]<threshold) {
            x2 = x-1;
            x3 = x;   
            y2 = buffer[x2];
            y3 = buffer[x3];
            break;
        }
    }

    //lk_debug << iPeak << endl;
    //lk_debug << x0 << " " << x1 << " " << x2 << " " << x3 << endl;

    iMin = x0;
    iMax = x2;

    double sy = (y1-y0)/numDivisions;
    for (auto div=0; div<=1.5*numDivisions; ++div)
    {
        auto y = y0 + div*sy;
        auto dy = TMath::Abs(y-threshold);
        if (dy>dy0) {
            if (abs(dy)>abs(dy0)) iMin = x0 + (div-1)*xResolution;
            else                  iMin = x0 + (div+0)*xResolution;
            //lk_debug << div << " " << dy << " " << iMin << endl;
            break;
        }
        dy0 = dy;
    }

    sy = (y3-y2)/numDivisions;
    for (auto div=0; div<=1.5*numDivisions; ++div)
    {
        auto y = y2 + div*sy;
        auto dy = TMath::Abs(y-threshold);
        //lk_debug << div << ") iMax=" << x2 + (div+0)*xResolution << " dy=" << dy << " dy1=" << dy1 << endl;
        if (dy>dy1) {
            if (abs(dy)>abs(dy1)) iMax = x2 + (div-1)*xResolution;
            else                  iMax = x2 + (div+0)*xResolution;
            //lk_debug << div << " " << dy << " " << iMax << endl;
            break;
        }
        dy1 = dy;
    }

    double width = iMax - iMin;

    return width;
}

double LKSAM::GetSmoothLevel(TH1* hist, double thresholdRatio)
{
    auto max = hist -> GetMaximum();
    auto threshold = max*thresholdRatio;
    auto nbins = hist -> GetXaxis() -> GetNbins();
    if (nbins<5) return 1.;

    int countV = 0;
    int countX = 0;
    auto value1 = hist -> GetBinContent(1);
    auto value2 = hist -> GetBinContent(2);
    for (auto bin=3; bin<=nbins; ++bin)
    {
        auto value3 = hist -> GetBinContent(bin);
        int count0 = 0;
        if (value1==0) count0++;
        if (value2==0) count0++;
        if (value3==0) count0++;
        if (count0>=2) {
            countX++;
        }
        else {
            double diff1 = (value2-value1);
            double diff2 = (value3-value2);
            if ((diff1>0)!=(diff2>0)) {
                if (abs(diff1)>threshold||abs(diff2)>threshold) {
                    countV++;
                }
            }
        }
        value1 = value2;
        value2 = value3;
    }
    double level = double((nbins-2-countX)-countV)/double(nbins-2-countX);
    return level;
}
