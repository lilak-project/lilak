#include "LKLogger.h"
#include "LKBufferI.h"
using namespace std;

ClassImp(LKBufferI);

LKBufferI::LKBufferI()
{
    Clear();
}

void LKBufferI::Clear(Option_t *option)
{
    fEmpty = true;
    memset(fArray, 0, sizeof(int)*512);
}

void LKBufferI::Copy(TObject &object) const
{
    auto objCopy = (LKBufferI &) object;
    objCopy.SetArray(fArray);
}

void LKBufferI::Print(Option_t *option) const
{
    if (!fEmpty) {
        e_info << "- Filled LKBufferI, array : " << std::endl;
        for (int i=0; i<512; ++i)
            e_cout << fArray[i] << " ";
        e_cout << std::endl;
    }
    else
        e_info << "- Empty LKBufferI" << std::endl;
}

void LKBufferI::Draw(Option_t *option)
{
    GetHist() -> Draw(option);
}

TH1D* LKBufferI::GetHist(TString name)
{
    if (fHist==nullptr) {
        if (name.IsNull()) name = "LKBufferIHist";
        fHist = new TH1D(name,";tb;y",512,0,512);
    }
    FillHist(fHist);
    return fHist;
}

void LKBufferI::FillHist(TH1* hist)
{
    for (int i=0; i<512; ++i) {
        hist -> SetBinContent(i+1,fArray[i]);
    }
}

TGraph* LKBufferI::GetGraph()
{
    if (fGraph==nullptr) {
        fGraph = new TGraph();
    }
    FillGraph(fGraph);
    return fGraph;
}

void LKBufferI::FillGraph(TGraph* graph)
{
    graph -> Set(0);
    for (int i=0; i<512; ++i)
        graph -> SetPoint(i,i,fArray[i]);
}

void LKBufferI::SetBuffer(LKBufferI buffer)
{
    fEmpty = buffer.IsEmpty();
    memcpy(fArray, buffer.GetArray(), sizeof(int)*512);
}

double LKBufferI::GetScale(int* array)
{
    double aaa = 0;
    double bbb = 0;
    for (int tb=0; tb<512; tb++) {
        double value = fArray[tb];
        double refer = array[tb];
        aaa += refer * value;
        bbb += refer * refer;
    }
    double scale = aaa / bbb;
    return scale;
}

double LKBufferI::GetScale(double* array)
{
    double aaa = 0;
    double bbb = 0;
    for (int tb=0; tb<512; tb++) {
        double value = fArray[tb];
        double refer = array[tb];
        aaa += refer * value;
        bbb += refer * refer;
    }
    double scale = aaa / bbb;
    return scale;
}

double LKBufferI::CalculateGroupFluctuation(int numGroups, int tb2)
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

void LKBufferI::GetGroupMeanStdDev(int numGroups, int tb2, double* pdstalGroup, double* stddevGroup)
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
            pdstalGroup[iGroup] += fArray[tbGlobal];
            stddevGroup[iGroup] += fArray[tbGlobal]*fArray[tbGlobal];
            tbGlobal++;
        }
        pdstalGroup[iGroup] = pdstalGroup[iGroup] / numTbsInGroup;
        stddevGroup[iGroup] = stddevGroup[iGroup] / numTbsInGroup;
        stddevGroup[iGroup] = sqrt(stddevGroup[iGroup] - pdstalGroup[iGroup]*pdstalGroup[iGroup]);
    }
    for (int iTb=0; iTb<numTbsInGroupLast; iTb++) {
        pdstalGroup[numGroupsM1] = pdstalGroup[numGroupsM1] + fArray[tbGlobal];
        tbGlobal++;
    }
    pdstalGroup[numGroupsM1] = pdstalGroup[numGroupsM1] / numTbsInGroupLast;
}

double LKBufferI::Integral(double pedestal)
{
    double sum = 0.;
    for (int tb=0; tb<512; tb++)
        sum += fArray[0] - pedestal;
    return sum;
}
