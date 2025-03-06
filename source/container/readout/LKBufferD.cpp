#include "LKLogger.h"
#include "LKBufferD.h"

ClassImp(LKBufferD);

LKBufferD::LKBufferD()
{
    Clear();
}

void LKBufferD::Clear(Option_t *option)
{
    fEmpty = true;
    memset(fArray, 0, sizeof(double)*512);
}

void LKBufferD::Copy(TObject &object) const
{
    auto objCopy = (LKBufferD &) object;
    objCopy.SetArray(fArray);
}

void LKBufferD::Print(Option_t *option) const
{
    if (!fEmpty) {
        e_info << "- Filled LKBufferD, array : " << std::endl;
        for (int i=0; i<512; ++i)
            e_cout << fArray[i] << " ";
        e_cout << std::endl;
    }
    else
        e_info << "- Empty LKBufferD" << std::endl;
}

void LKBufferD::Draw(Option_t *option)
{
    GetHist() -> Draw(option);
}

TH1D* LKBufferD::GetHist(TString name)
{
    if (fHist==nullptr||name.IsNull()==false) {
        if (name.IsNull()) name = "LKBufferDHist";
        fHist = new TH1D(name,";tb;y",512,0,512);
    }
    FillHist(fHist);
    return fHist;
}

void LKBufferD::FillHist(TH1* hist)
{
    for (int i=0; i<512; ++i)
        hist -> SetBinContent(i+1,fArray[i]);
}

TGraph* LKBufferD::GetGraph()
{
    if (fGraph==nullptr) {
        fGraph = new TGraph();
    }
    FillGraph(fGraph);
    return fGraph;
}

void LKBufferD::FillGraph(TGraph* graph)
{
    graph -> Set(0);
    for (int i=0; i<512; ++i)
        graph -> SetPoint(i,i,fArray[i]);
}

void LKBufferD::SetBuffer(LKBufferD buffer)
{
    fEmpty = buffer.IsEmpty();
    memcpy(fArray, buffer.GetArray(), sizeof(double)*512);
}

double LKBufferD::GetScale(int* array)
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

double LKBufferD::GetScale(double* array)
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

double LKBufferD::CalculateGroupFluctuation(int numGroups, int tb2)
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

void LKBufferD::GetGroupMeanStdDev(int numGroups, int tb2, double* pdstalGroup, double* stddevGroup)
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
