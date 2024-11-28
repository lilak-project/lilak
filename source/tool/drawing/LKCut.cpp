#include "TFormula.h"
#include "TF1.h"
#include "TFile.h"

#include "LKCut.h"
#include "LKLogger.h"
#include "LKParameter.h"
#include "LKParameterContainer.h"

ClassImp(LKCut)

LKCut::LKCut(const char* name, const char* title)
: TNamed(name, title)
{
    fCutArray = new TObjArray();
}

void LKCut::Draw(Option_t *option)
{
    LKCut::Draw(0,100,0,100);
}

void LKCut::Draw(double x1, double x2, double y1, double y2)
{
    auto numCuts = GetNumCuts();
    if (numCuts==0)
        return;

    for (auto active : {false, true})
    {
        for (auto i=0; i<numCuts; ++i)
        {
            if (fActiveArray[i]==active)
            {
                if (fTypeArray[i]==kTypeTCut)
                {
                    auto cut = (TCut*) fCutArray -> At(i);
                    TString expression = cut->GetTitle();
                    expression.ReplaceAll("==","=");
                    expression.ReplaceAll(">","=");
                    expression.ReplaceAll("<","=");
                    if (expression[0]=='(' && expression[expression.Sizeof()-2]==')')
                        expression = expression(1,expression.Sizeof()-3);
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
                    else if (expression.Index("x=")==0) {
                        TString xValueString(expression(2,expression.Sizeof()-3));
                        if (xValueString.IsFloat()) {
                            auto xValue = xValueString.Atof();
                            auto graph = new TGraph();
                            graph -> SetPoint(0,xValue,y1);
                            graph -> SetPoint(1,xValue,y2);
                            if (active) {
                                graph -> SetLineColor(kRed);
                                graph -> SetLineWidth(2);
                            }
                            else {
                                graph -> SetLineColor(kBlack);
                                graph -> SetLineWidth(1);
                                graph -> SetLineStyle(2);
                            }
                            graph -> Draw("samel");
                        }
                    }
                    //else
                        //lk_warning << "Expression " << expression << " cannot be drawn (" << cut->GetTitle() << ")" << endl;
                }
                else if (fTypeArray[i]==kTypeCutG) {
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
    auto numCuts = GetNumCuts();
    e_info << fName << " " << fTitle << ", containing " << numCuts << " cuts" << endl;
    if (!fExpression.IsNull()) e_info << "Expression: " << fExpression << endl;
    for (auto iCut=0; iCut<numCuts; ++iCut)
    {
        auto tag = fTagArray[iCut];
        TString name;
        TString title;
        TString title2;
        if (fTypeArray[iCut]==kTypeTCut) {
            auto cut = (TCut*) fCutArray -> At(iCut);
            name = cut -> GetName();
            title = cut -> GetTitle();
        }
        else if (fTypeArray[iCut]==kTypeCutG) {
            auto cutg = (TCutG*) fCutArray -> At(iCut);
            int numPoints = cutg -> GetN();
            name = cutg -> GetName();
            title = cutg -> GetTitle();
            title2 = Form("cutg with %d points",numPoints);
            if (numPoints <= 4) {
                title2 += ": ";
                 for (auto iPoint=0; iPoint<numPoints; ++iPoint) {
                     double x, y;
                     cutg -> GetPoint(iPoint, x, y);
                     title2 += Form(" (%d|%.2f,%.2f),",iPoint,x,y);
                 }
            }
        }
        e_cout << "      [" << iCut << "] (" << tag << ") " << name << ", " << title << "; " << title2  << endl;
    }
}

bool LKCut::IsActive()
{
    auto numCuts = GetNumCuts();
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
    auto numCuts = GetNumCuts();
    for (auto i=0; i<numCuts; ++i) {
        Add(cutArray->At(i), activiArray->at(i));
    }
}

void LKCut::Add(TString tag, TCut cut, bool apply)
{
    auto newcut = new TCut(cut.GetName(),cut.GetTitle());
    this -> Add(tag,newcut,apply);
}

void LKCut::Add(TString tag, TCut* cut, bool apply)
{
    if (tag.IsNull()) tag = MakeTag();
    fCutArray -> Add(cut);
    fActiveArray.push_back(apply);
    fTagArray.push_back(tag);
    fTypeArray.push_back(kTypeTCut);
}

void LKCut::Add(TString tag, TCutG* cut, bool apply)
{
    if (tag.IsNull()) tag = MakeTag();
    fCutArray -> Add(cut);
    fActiveArray.push_back(apply);
    fTagArray.push_back(tag);
    fTypeArray.push_back(kTypeCutG);
}

void LKCut::Add(TString tag, TString value, bool apply)
{
    if (value.EndsWith(".root")) {
        auto file = new TFile(value,"read");
        if (file->IsOpen()==false) {
            lk_error << "Cannnot open " << value << endl;
            return;
        }
        auto cutg = (TCutG*) file -> Get("CUTG");
        if (cutg==nullptr) {
            lk_error << "CUTG is null in " << value << endl;
            return;
        }
        cutg -> SetName(Form("cutg%d",GetNumCuts()));
        cutg -> SetTitle(value);
        this -> Add(tag,cutg,apply);
    }
    else {
        TCut cut(Form("tcut%d",GetNumCuts()),value);
        this -> Add(tag,cut,apply);
    }
}

void LKCut::Add(TString tag, TObject* cut, bool apply)
{
    if      (cut->InheritsFrom(TCutG::Class())) { this -> Add(tag, (TCutG*) cut, apply); }
    else if (cut->InheritsFrom(TCut::Class()))  { this -> Add(tag, (TCut*) cut, apply); }
    else if (cut->InheritsFrom(LKCut::Class())) { this -> Add((LKCut*) cut); }
}

int LKCut::IsInsideSingle(int i, Double_t x, Double_t y) const
{
    int inside = 2;
    if (fTypeArray[i]==kTypeTCut) {
        auto cut = (TCut*) fCutArray -> At(i);
        auto formula = TFormula(cut->GetName(),cut->GetTitle());
        inside = int(formula.Eval(x,y));
    }
    else if (fTypeArray[i]==kTypeCutG) {
        auto cut = (TCutG*) fCutArray -> At(i);
        inside = cut -> IsInside(x,y);
    }
    return inside;
}

int LKCut::IsInsideOr(Double_t x, Double_t y) const
{
    auto numCuts = GetNumCuts();
    for (auto i=0; i<numCuts; ++i) {
        if (fActiveArray[i]==false)
            continue;
        int inside = IsInsideSingle(i,x,y);
        if (inside>0)
            return 1;
    }
    return 0;
}

int LKCut::IsInsideAnd(Double_t x, Double_t y) const
{
    auto numCuts = GetNumCuts();
    for (auto i=0; i<numCuts; ++i) {
        if (fActiveArray[i]==false)
            continue;
        int inside = IsInsideSingle(i,x,y);
        if (inside==0)
            return 0;
    }
    return 1;
}

int LKCut::IsInside(Double_t x, Double_t y) const
{
    TString expression = fExpression;
    auto numCuts = GetNumCuts();
    for (auto i=0; i<numCuts; ++i) {
        auto isInside = IsInsideSingle(i,x,y);
        TString isInsideString = "false";
        if (isInside)
            isInsideString = "true";
        auto tag = fTagArray[i];
        expression.ReplaceAll(tag,isInsideString);
    }
    bool isInsideAll = TFormula("cutexp",expression).Eval(0);
    return isInsideAll;
}

TString LKCut::GetCutString(int i, TString varX, TString varY)
{
    if (fTypeArray[i]==kTypeTCut) {
        auto cut = (TCut*) fCutArray -> At(i);
        TString value = cut->GetTitle();
        value.ReplaceAll("x",varX.Data());
        value.ReplaceAll("y",varY.Data());
        return value;
    }
    else if (fTypeArray[i]==kTypeCutG) {
        auto cut = (TCutG*) fCutArray -> At(i);
        TString value = cut -> GetName();
        return value;
    }
    return TString("");
}

TString LKCut::MakeCutString(TString varX, TString varY, TString expression)
{
    if (expression.IsNull())
        expression = fExpression;

    auto numCuts = GetNumCuts();
    for (auto i=0; i<numCuts; ++i) {
        auto tag = fTagArray[i];
        auto value = GetCutString(i,varX,varY);
        value = Form("(%s)",value.Data());
        expression.ReplaceAll(tag,value);
    }
    return expression;
}

TString LKCut::MakeCutStringDelim(TString varX, TString varY, TString delim)
{
    TString cutString;
    auto numCuts = GetNumCuts();
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

void LKCut::AddPar(LKParameterContainer* par)
{
    auto numCuts = par -> GetEntries();
    for (auto iCut=0; iCut<numCuts; ++iCut)
    {
        auto parameter = (LKParameter*) par -> At(iCut);
        TString parName = parameter -> GetMainName();
        TString parValue = parameter -> GetValue();
        parName.ToLower();
        if (parName=="expression") {
            fExpression = parValue;
            continue;
        }

        TString tag;
        TString value;
        bool applyCut = true;
        if (parameter->GetN()==3) {
            tag = parameter -> GetString(0);
            value = parameter -> GetString(1);
            applyCut = parameter -> GetBool(2);
        }
        else if (parameter->GetN()==2) {
            tag = parameter -> GetString(0);
            value = parameter -> GetBool(1);
        }
        else 
            value = parameter -> GetString(0);

        this -> Add(tag,value,applyCut);
    }

    if (fExpression.IsNull())
        lk_warning << "In " << par->GetName() << ", parameter \"expression\" is not set!" << endl;
}
