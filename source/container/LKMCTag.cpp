#include "LKMCTag.h"

ClassImp(LKMCTag)

LKMCTag::LKMCTag()
{
}

LKMCTag::~LKMCTag()
{
}

void LKMCTag::Clear(Option_t *option)
{
    LKContainer::Clear(option);
    fIndex.clear(); 
    fMCID.clear(); 
    fWeight.clear();
}

void LKMCTag::Copy(TObject &obj) const
{
    ((LKMCTag&)obj).Clear();
    LKContainer::Copy(obj);
    for (auto i=0; i<fIndex.size(); i++) 
        ((LKMCTag&)obj).AddMCWeightTag(fMCID[i], fWeight[i], fIndex[i]);
    
}

void LKMCTag::AddMCTag(Int_t mcId, Int_t index)
{
    AddMCWeightTag(mcId, 1., index);
}

void LKMCTag::AddMCWeightTag(Int_t mcId, Double_t weight, Int_t index)
{
    bool isExist = false;
    for (auto i=0; i<fIndex.size(); i++)
    {
        if (fIndex[i] == index && fMCID[i] == mcId) {
            fWeight[i] = fWeight[i] + fabs(weight);
            isExist = true;
            break;
        }
    }

    if (!isExist) {
        fIndex.push_back(index);
        fMCID.push_back(mcId);
        fWeight.push_back(fabs(weight));
    }
}

Int_t LKMCTag::GetMCNum(int index)
{
    int num = 0;
    for (auto i=0; i<fIndex.size(); i++)
        if (fIndex[i] == index) num++;
    
    return num;
}

Int_t LKMCTag::GetMCID(int mcIdx, int index)
{
    if (mcIdx >= GetMCNum(index)) return -999;

    int tmpIdx = 0;
    for (auto i=0; i<fIndex.size(); i++)
    {
        if (fIndex[i] == index) {
            if (tmpIdx == mcIdx) return fMCID[i];
            tmpIdx++;
        }
    }

    return -999;
}

Double_t LKMCTag::GetMCPurity(int mcIdx, int index)
{
    if (mcIdx >= GetMCNum(index)) return -999.;

    double mcIdxWeight = 0.;
    double weightSum = 0.;
    int tmpIdx = 0;
    for (auto i=0; i<fIndex.size(); i++)
    {
        if (fIndex[i] == index) {
            if (tmpIdx == mcIdx) 
                mcIdxWeight = fWeight[i];
            
            weightSum += fWeight[i];
            tmpIdx++;
        }
    }

    if (weightSum == 0.) return -999.;
    mcIdxWeight /= weightSum;

    return mcIdxWeight;
}