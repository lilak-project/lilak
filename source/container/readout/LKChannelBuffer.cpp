#include "LKChannelBuffer.h"

ClassImp(LKChannelBuffer);

LKChannelBuffer::LKChannelBuffer()
{
    Clear();
}

void LKChannelBuffer::Clear(Option_t *option)
{
    LKChannel::Clear(option);

    fChannelID = -1;
    fNoiseScale = -1;
    fTime = -1;
    fEnergy = -1;
    fPedestal = -1;

    memset(fBuffer, 0, sizeof(double)*512);
}

void LKChannelBuffer::Draw(Option_t *option)
{
    GetHist() -> Draw(option);
}

TH1D* LKChannelBuffer::GetHist(TString name)
{
    if (fHist==nullptr) {
        if (name.IsNull()) name = "LKChannelBufferHist";
        fHist = new TH1D(name,";tb;y",512,0,512);
    }
    FillHist(fHist);
    return fHist;
}

void LKChannelBuffer::FillHist(TH1D* hist)
{
    for (Int_t i=0; i<512; ++i)
        hist -> SetBinContent(i+1,fBuffer[i]);
}

void LKChannelBuffer::SetBuffer(double* buffer)
{
    memcpy(fBuffer, buffer, sizeof(double)*512);
}

void LKChannelBuffer::SetBuffer(int* buffer)
{
    for (Int_t i=0; i<512; ++i)
        fBuffer[i] = double(buffer[i]);
}

void LKChannelBuffer::SubtractBuffer(double* buffer)
{
    for (Int_t i=0; i<512; ++i)
        fBuffer[i] = fBuffer[i] - buffer[i];
}

void LKChannelBuffer::SubtractBuffer(double* buffer, double scale)
{
    for (Int_t i=0; i<512; ++i)
        fBuffer[i] = fBuffer[i] - scale*buffer[i];
}

double LKChannelBuffer::GetScale(double* buffer)
{
    double aaa = 0;
    double bbb = 0;
    for (int tb=0; tb<512; tb++) {
        double value = fBuffer[tb];
        double refer = buffer[tb];
        aaa += refer * value;
        bbb += refer * refer;
    }
    double scale = aaa / bbb;
    return scale;
}

double LKChannelBuffer::CalculateGroupFluctuation(int numGroups, int tb2)
{
    double pdstalGroup[20] = {0};
    double stddevGroup[20] = {0};

    GetGroupMeanStdDev(numGroups, tb2, pdstalGroup, stddevGroup);

    double mean = 0;
    double stddev = 0;
    for (auto iGroup=0; iGroup<numGroups; ++iGroup) {
        mean += pdstalGroup[iGroup];
        stddev += pdstalGroup[iGroup]*pdstalGroup[iGroup];
    }
    stddev = stddev / numGroups;
    stddev = sqrt(stddev - mean*mean);

    return stddev;
}

void LKChannelBuffer::GetGroupMeanStdDev(int numGroups, int tb2, double* pdstalGroup, double* stddevGroup)
{
    int numTbsInGroup = floor(tb2/numGroups);
    int numGroupsM1 = numGroups - 1;
    int numTbsInGroupLast = numTbsInGroup + tb2 - numGroups*numTbsInGroup;

    for (auto iGroup=0; iGroup<numGroups; ++iGroup) {
        pdstalGroup[iGroup] = 0;
        stddevGroup[iGroup] = 0;
    }

    int tbGlobal = 0;
    for (auto iGroup=0; iGroup<numGroupsM1; ++iGroup) {
        for (int iTb=0; iTb<numTbsInGroup; iTb++) {
            pdstalGroup[iGroup] += fBuffer[tbGlobal];
            stddevGroup[iGroup] += fBuffer[tbGlobal]*fBuffer[tbGlobal];
            tbGlobal++;
        }
        pdstalGroup[iGroup] = pdstalGroup[iGroup] / numTbsInGroup;
        stddevGroup[iGroup] = stddevGroup[iGroup] / numTbsInGroup;
        stddevGroup[iGroup] = sqrt(stddevGroup[iGroup] - pdstalGroup[iGroup]*pdstalGroup[iGroup]);
    }
    for (int iTb=0; iTb<numTbsInGroupLast; iTb++) {
        pdstalGroup[numGroupsM1] = pdstalGroup[numGroupsM1] + fBuffer[tbGlobal];
        tbGlobal++;
    }
    pdstalGroup[numGroupsM1] = pdstalGroup[numGroupsM1] / numTbsInGroupLast;
}

/*
double LKChannelBuffer::CalculatePedestal(int numGroups, int tb2, double stdDevCut)
{
    int numTbsInGroup = floor(tb2/numGroups);
    int numGroupsM1 = numGroups - 1;
    int numTbsInGroupLast = numTbsInGroup + tb2 - numGroups*numTbsInGroup;

    double pdstalGroup[20] = {0};
    double stddevGroup[20] = {0};

    GetGroupMeanStdDev(numGroups, tb2, pdstalGroup, stddevGroup);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////

    int countGroupsInCut = 0;
    for (auto iGroup=0; iGroup<numGroups; ++iGroup)
        //if (pdstalGroup[iGroup]>0&&stddevGroup[iGroup]/pdstalGroup[iGroup]<0.1)
        if (pdstalGroup[iGroup]>0&&stddevGroup[iGroup]<stdDevCut)
            countGroupsInCut++;

    double pedestalDiffMin = DBL_MAX; // the minium pedestal difference between any two groups
    double pedestalMeanRef = 0; // mean value between pedestal lof two groups, where two goups are chosen to have minimum pedestal difference (pedestalDiffMin)
    if (countGroupsInCut>=2) {
        for (auto iGroup=0; iGroup<numGroups; ++iGroup) {
            if (pdstalGroup[iGroup]<=0||stddevGroup[iGroup]>stdDevCut) continue;
            for (auto jGroup=0; jGroup<numGroups; ++jGroup) {
                if (iGroup>=jGroup) continue;
                if (pdstalGroup[jGroup]<=0||stddevGroup[jGroup]>stdDevCut) continue;
                double diff = abs(pdstalGroup[iGroup] - pdstalGroup[jGroup]);
                if (diff<pedestalDiffMin) {
                    pedestalDiffMin = diff;
                    pedestalMeanRef = 0.5 * (pdstalGroup[iGroup] + pdstalGroup[jGroup]);
                }
            }
        }
    }
    else {
        for (auto iGroup=0; iGroup<numGroups; ++iGroup) {
            for (auto jGroup=0; jGroup<numGroups; ++jGroup) {
                if (iGroup>=jGroup) continue;
                double diff = abs(pdstalGroup[iGroup] - pdstalGroup[jGroup]);
                if (diff<pedestalDiffMin) {
                    pedestalDiffMin = diff;
                    pedestalMeanRef = 0.5 * (pdstalGroup[iGroup] + pdstalGroup[jGroup]);
                }
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////

    double pedestalErrorRefGroup = stdDevCut;
    pedestalErrorRefGroup = sqrt(pedestalErrorRefGroup*pedestalErrorRefGroup + pedestalDiffMin*pedestalDiffMin);
    if (pedestalErrorRefGroup>fPedestalErrorRefGroupCut)
        pedestalErrorRefGroup = fPedestalErrorRefGroupCut;

    double pedestalFinal = 0;
    int countNumPedestalTb = 0;

    tbGlobal = 0;
    for (auto iGroup=0; iGroup<numGroupsM1; ++iGroup)
    {
        double diffGroup = abs(pedestalMeanRef - pdstalGroup[iGroup]);
        if (diffGroup<pedestalErrorRefGroup)
        {
            countNumPedestalTb += numTbsInGroup;
            for (int iTb=0; iTb<numTbsInGroup; iTb++)
            {
                pedestalFinal += buffer[tbGlobal];
                tbGlobal++;
            }
        }
        else {
            tbGlobal += numTbsInGroup;
        }
    }
    double diffGroup = abs(pedestalMeanRef - pdstalGroup[numGroupsM1]);
    if (diffGroup<pedestalErrorRefGroup)
    {
        countNumPedestalTb += numTbsInGroupLast;
        for (int iTb=0; iTb<numTbsInGroupLast; iTb++) {
            pedestalFinal += buffer[tbGlobal];
            tbGlobal++;
        }
    }

    pedestalFinal = pedestalFinal / countNumPedestalTb;

    for (auto iTb=0; iTb<tb2; ++iTb)
        buffer[iTb] = buffer[iTb] - pedestalFinal;

    //fDynamicRange = fDynamicRange - pedestalFinal;

    auto pedestal = pedestalFinal;

    return pedestalFinal;
}
*/
