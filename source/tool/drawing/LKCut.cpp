#include "TFormula.h"
#include "TF1.h"
#include "LKCut.h"
#include "LKLogger.h"

ClassImp(LKCut)

LKCut::LKCut(const char* name, const char* title)
: TNamed(name, title)
{
    fCutArray = new TObjArray();
}

void LKCut::Draw(Option_t *option)
{
    auto numCuts = fCutArray -> GetEntries();
    if (numCuts==0)
        return;

    for (auto active : {false, true})
    {
        for (auto i=0; i<numCuts; ++i)
        {
            if (fActiveArray[i]==active)
            {
                if (fTypeArray[i]==1) {
                    auto cut = (TCut*) fCutArray -> At(i);
                    TString expression = cut->GetTitle();
                    expression.ReplaceAll("==","=");
                    expression.ReplaceAll(">","=");
                    expression.ReplaceAll("<","=");
                    if (expression.Index("y=")==0) {
                        expression.ReplaceAll("y=","");
                        auto f1 = new TF1(cut->GetName(),expression,0,100);
                        if (active) {
                            f1 -> SetLineColor(kRed);
                            f1 -> SetLineWidth(2);
                        }
                        else {
                            f1 -> SetLineColor(kBlack);
                            f1 -> SetLineWidth(1);
                            f1 -> SetLineStyle(2);
                        }
                        f1 -> Draw("same");
                    }
                    else
                        lk_warning << "Expression " << expression << " cannot be drawn (" << cut->GetTitle() << ")" << endl;
                }
                else if (fTypeArray[i]==2) {
                    auto cutg = (TCutG*) fCutArray -> At(i);
                    if (active) {
                        cutg -> SetLineColor(kRed);
                        cutg -> SetLineWidth(2);
                    }
                    else {
                        cutg -> SetLineColor(kBlack);
                        cutg -> SetLineWidth(1);
                        cutg -> SetLineStyle(2);
                    }
                    cutg -> Draw("same");
                }
            }
        }
    }
}

void LKCut::Draw(double x1, double x2)
{
    auto numCuts = fCutArray -> GetEntries();
    if (numCuts==0)
        return;

    for (auto active : {false, true})
    {
        for (auto i=0; i<numCuts; ++i)
        {
            if (fActiveArray[i]==active)
            {
                if (fTypeArray[i]==1) {
                    auto cut = (TCut*) fCutArray -> At(i);
                    TString expression = cut->GetTitle();
                    expression.ReplaceAll("==","=");
                    expression.ReplaceAll(">","=");
                    expression.ReplaceAll("<","=");
                    if (expression.Index("y=")==0) {
                        expression.ReplaceAll("y=","");
                        auto f1 = new TF1(cut->GetName(),expression,x1,x2);
                        if (active) {
                            f1 -> SetLineColor(kRed);
                            f1 -> SetLineWidth(2);
                        }
                        else {
                            f1 -> SetLineColor(kBlack);
                            f1 -> SetLineWidth(1);
                            f1 -> SetLineStyle(2);
                        }
                        f1 -> Draw("same");
                    }
                    else
                        lk_warning << "Expression " << expression << " cannot be drawn (" << cut->GetTitle() << ")" << endl;
                }
                else if (fTypeArray[i]==2) {
                    auto cutg = (TCutG*) fCutArray -> At(i);
                    if (active) {
                        cutg -> SetLineColor(kRed);
                        cutg -> SetLineWidth(2);
                    }
                    else {
                        cutg -> SetLineColor(kBlack);
                        cutg -> SetLineWidth(1);
                        cutg -> SetLineStyle(2);
                    }
                    cutg -> Draw("same");
                }
            }
        }
    }
}

void LKCut::Print(Option_t *option) const
{
    auto numCuts = fCutArray -> GetEntries();
    e_info << fName << ", " << fTitle << ", continaing " << numCuts << " cuts" << endl;
    for (auto iCut=0; iCut<numCuts; ++iCut)
    {
        if (fTypeArray[iCut]==1) {
            auto cut = (TCut*) fCutArray -> At(iCut);
            e_cout << "      [" << iCut << "] " << cut->GetName() << "; " << cut->GetTitle() << endl;
        }
        else if (fTypeArray[iCut]==2) {
            auto cutg = (TCutG*) fCutArray -> At(iCut);
            int numPoints = cutg -> GetN();
            TString title = Form("cutg with %d points",numPoints);
            if (numPoints <= 4) {
                title += ": ";
                 for (auto iPoint=0; iPoint<numPoints; ++iPoint) {
                     double x, y;
                     cutg -> GetPoint(iPoint, x, y);
                     title += Form(" (%d|%.2f,%.2f),",iPoint,x,y);
                 }
            }
            e_cout << "      [" << iCut << "] " << cutg->GetName() << ", " << cutg->GetTitle() << "; " << title  << endl;
        }
    }
}

bool LKCut::IsActive()
{
    auto numCuts = fCutArray -> GetEntries();
    for (auto i=0; i<numCuts; ++i) {
        if (fActiveArray.at(i))
            return true;
    }
    return false;
}

TObject* LKCut::At(Int_t idx) const
{
    auto obj = fCutArray -> At(idx);
    return obj;
}

void LKCut::Add(LKCut *cuts)
{
    auto cutArray = cuts -> GetCutArray();
    auto activiArray = cuts -> GetActiveArray();
    auto numCuts = cutArray -> GetEntries();
    for (auto i=0; i<numCuts; ++i) {
        Add(cutArray->At(i), activiArray->at(i));
    }
}

void LKCut::Add(TCut cut, bool apply)
{
    auto newcut = new TCut(cut.GetName(),cut.GetTitle());
    Add(newcut,apply);
    //lk_debug << &cut << endl;
    //fCutArray -> Add(&cut);
    //fActiveArray.push_back(apply);
    //fTypeArray.push_back(1);
}

void LKCut::Add(TCut* cut, bool apply)
{
    fCutArray -> Add(cut);
    fActiveArray.push_back(apply);
    fTypeArray.push_back(1);
}

void LKCut::Add(TCutG* cut, bool apply)
{
    fCutArray -> Add(cut);
    fActiveArray.push_back(apply);
    fTypeArray.push_back(2);
}

void LKCut::Add(TObject* cut, bool apply)
{
    if (cut->InheritsFrom(TCutG::Class())) { Add((TCutG*) cut, apply); }
    else if (cut->InheritsFrom(TCut::Class())) { Add((TCut*) cut, apply); }
    else if (cut->InheritsFrom(LKCut::Class())) { Add((LKCut*)cut); }
}

int LKCut::IsInside(int i, Double_t x, Double_t y) const
{
    int inside = 2;
    if (fTypeArray[i]==1) {
        auto cut = (TCut*) fCutArray -> At(i);
        auto formula = TFormula(cut->GetName(),cut->GetTitle());
        inside = int(formula.Eval(x,y));
    }
    else if (fTypeArray[i]==2) {
        auto cut = (TCutG*) fCutArray -> At(i);
        inside = cut -> IsInside(x,y);
    }
    return inside;
}

int LKCut::IsInsideOr(Double_t x, Double_t y) const
{
    auto numCuts = fCutArray -> GetEntries();
    for (auto i=0; i<numCuts; ++i) {
        if (fActiveArray[i]==false)
            continue;
        int inside = IsInside(i,x,y);
        if (inside>0)
            return 1;
    }
    return 0;
}

int LKCut::IsInsideAnd(Double_t x, Double_t y) const
{
    auto numCuts = fCutArray -> GetEntries();
    for (auto i=0; i<numCuts; ++i) {
        if (fActiveArray[i]==false)
            continue;
        int inside = IsInside(i,x,y);
        if (inside==0)
            return 0;
    }
    return 1;
}

TString LKCut::GetCutString(int i, TString varX, TString varY)
{
    if (fTypeArray[i]==1) {
        auto cut = (TCut*) fCutArray -> At(i);
        TString value = cut->GetTitle();
        value.ReplaceAll("x",varX.Data());
        value.ReplaceAll("y",varY.Data());
        return value;
    }
    else if (fTypeArray[i]==2) {
        auto cut = (TCutG*) fCutArray -> At(i);
        TString value = cut -> GetName();
        return value;
    }
    return TString("");
}

TString LKCut::MakeCutString(TString varX, TString varY, TString delim)
{
    TString cutString;
    auto numCuts = fCutArray -> GetEntries();
    for (auto i=0; i<numCuts; ++i) {
        if (fActiveArray[i]==false)
            continue;
        auto value = GetCutString(i,varX,varY);
        value = Form("(%s)",value.Data());
        if (cutString.IsNull()) cutString = Form("(%s)",value.Data());
        else cutString = cutString + " " + delim + " " + value;
    }
    return cutString;
}
