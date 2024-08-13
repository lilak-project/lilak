#include "LKSiDetector.h"

ClassImp(LKSiDetector);

LKSiDetector::LKSiDetector()
{
    Clear();
}

void LKSiDetector::Clear(Option_t *option)
{
    LKContainer::Clear(option);
    //
}

void LKSiDetector::Copy(TObject &object) const
{
    //LKContainer::Copy(object);
    //auto objCopy = (LKSiDetector &) object;
    //objCopy.SetDetType(fDetType);
}

const char* LKSiDetector::GetName() const
{
    if (fDetTypeName.IsNull()) return Form("t%d",fDetType);
    //else
    //{
        TString detTypeName = fDetTypeName;
        detTypeName.ReplaceAll(" ","");
        return Form("%s_%d(%d)",detTypeName.Data(),fDetID,fDetIndex);
    //}
}

const char* LKSiDetector::GetTitle() const
{
    if (fNumSides==1) {
        TString ttlJ = "";
        if (fNumJunctionUD==2) ttlJ = " (U/D)";
        return Form("%s Idx(%d) ID(%d), single side detector with %d strips%s" + ttlJ,GetTitleType().Data(),fDetIndex,fDetID,fNumJunctionStrips,ttlJ.Data());
    }
    //else if (fNumSides==2)
    //{
        TString ttlJ = "";
        TString ttlO = "";
        if (fNumJunctionUD==2) ttlJ = " (U/D)";
        if (fNumOhmicLR==2) ttlO = " (L/R)";
        return Form("%s Idx(%d) ID(%d), junction %d strips%s, ohmic %d strips%s" + ttlJ,GetTitleType().Data(),fDetIndex,fDetID,fNumJunctionStrips,ttlJ.Data(),fNumOhmicStrips,ttlO.Data());
    //}
}

void LKSiDetector::Print(Option_t *option) const
{
    if (fNumSides==1) {
        TString ttlJLR = "";
        if (fNumJunctionUD==2) ttlJLR = " ,U/D";
        e_info << GetTitleType() << " Idx(" << fDetIndex << ") ID(" << fDetID << "), single side detector with " << fNumJunctionStrips << " strips" << ttlJLR << endl;
    }
    else if (fNumSides==2) {
        TString ttlJLR = "";
        TString ttlOLR = "";
        if (fNumJunctionUD==2) ttlJLR = " ,U/D";
        if (fNumOhmicLR==2) ttlOLR = " ,L/R";
        e_info << GetTitleType() << " Idx(" << fDetIndex << ") ID(" << fDetID << "), junction (" << fNumJunctionStrips << " strips" << ttlJLR << "), ohmic (" << fNumOhmicStrips << " strips" << ttlOLR << ")" << endl;
    }
    if (TString(option).Contains("get_channels")) {
        auto numChannels = fRegisteredChannelArray.GetEntries();
        for (auto iChannel=0; iChannel<numChannels; ++iChannel) {
            auto channel = fRegisteredChannelArray.At(iChannel);
            channel -> Print("get");
        }
    }
}

void LKSiDetector::Draw(Option_t *option)
{
    //GetHist() -> Draw(option);
    //GetHitGraph() -> Draw("samel");
}

TObject* LKSiDetector::Clone(const char *newname) const
{
    //LKSiDetector *obj = (LKSiDetector*) LKContainer::Clone(newname);
    LKSiDetector *obj = new LKSiDetector();
    obj -> SetSiType(fDetTypeName, fDetType, fDetIndex, fDetID, fNumSides, fNumJunctionStrips, fNumOhmicStrips, (fNumJunctionUD==2?1:0), (fNumOhmicLR==2?1:0));
    //obj -> SetSiPosition(fPosition, fLayer, fRow, fPhi, fTheta);
    obj -> SetSiPosition(fPosition, fLayer, fRow, fPhi1, fPhi2, fTheta1, fTheta2);
    return obj;
}

void LKSiDetector::SetSiType(TString detTypeName, int detType, int detIndex, int detID, int numSides, int numJunctionStrips, int numOhmicStrips, bool useJunctionLR, bool useOhmicLR)
{
    if (fDetType<0)
    {
        fDetTypeName = detTypeName;
        fDetType = detType;
        fDetIndex = detIndex;
        fDetID = detID;
        fNumSides = numSides;
        fNumJunctionStrips = numJunctionStrips;
        fNumOhmicStrips = numOhmicStrips;
        fNumJunctionUD = useJunctionLR ? 2 : 1;
        fNumOhmicLR = useOhmicLR ? 2 : 1;

        fIdxArray = new int**[fNumSides];
        fCountArray = new int**[fNumSides];
        fEnergyArray = new double**[fNumSides];
        fEnergySumArray = new double**[fNumSides];
        fChannelIndexArray = new int**[fNumSides];
        fStripEnergySumArray = new double*[fNumSides];
        for(int side=0; side<fNumSides; ++side)
        {
            if (side==0) {
                fIdxArray[side] = new int*[fNumJunctionStrips];
                fCountArray[side] = new int*[fNumJunctionStrips];
                fEnergyArray[side] = new double*[fNumJunctionStrips];
                fEnergySumArray[side] = new double*[fNumJunctionStrips];
                fChannelIndexArray[side] = new int*[fNumJunctionStrips];
                fStripEnergySumArray[side] = new double[fNumJunctionStrips];
                for(int strip=0; strip<fNumJunctionStrips; ++strip) {
                    fIdxArray[side][strip] = new int[fNumJunctionUD];
                    fCountArray[side][strip] = new int[fNumJunctionUD];
                    fEnergyArray[side][strip] = new double[fNumJunctionUD];
                    fEnergySumArray[side][strip] = new double[fNumJunctionUD];
                    fChannelIndexArray[side][strip] = new int[fNumJunctionUD];
                    fStripEnergySumArray[side][strip] = 0;
                    for(int lr=0; lr<fNumJunctionUD; ++lr) {
                        fIdxArray[side][strip][lr] = 0;
                        fCountArray[side][strip][lr] = 0;
                        fEnergyArray[side][strip][lr] = 0;
                        fEnergySumArray[side][strip][lr] = 0;
                        fChannelIndexArray[side][strip][lr] = -1;
                    }
                }
            }
            if (side==1) {
                fIdxArray[side] = new int*[fNumOhmicStrips];
                fCountArray[side] = new int*[fNumOhmicStrips];
                fEnergyArray[side] = new double*[fNumOhmicStrips];
                fEnergySumArray[side] = new double*[fNumOhmicStrips];
                fChannelIndexArray[side] = new int*[fNumOhmicStrips];
                fStripEnergySumArray[side] = new double[fNumOhmicStrips];
                for(int strip=0; strip<fNumOhmicStrips; ++strip) {
                    fIdxArray[side][strip] = new int[fNumOhmicLR];
                    fCountArray[side][strip] = new int[fNumOhmicLR];
                    fEnergyArray[side][strip] = new double[fNumOhmicLR];
                    fEnergySumArray[side][strip] = new double[fNumOhmicLR];
                    fChannelIndexArray[side][strip] = new int[fNumOhmicLR];
                    fStripEnergySumArray[side][strip] = 0;
                    for(int lr=0; lr<fNumOhmicLR; ++lr) {
                        fIdxArray[side][strip][lr] = 0;
                        fCountArray[side][strip][lr] = 0;
                        fEnergyArray[side][strip][lr] = 0;
                        fEnergySumArray[side][strip][lr] = 0;
                        fChannelIndexArray[side][strip][lr] = -1;
                    }
                }
            }
        }
    }
    else {
        e_error << "Si Detector is already initialized!" << endl;
    }
}

void LKSiDetector::Fire(int side, int strip, double energy)
{
    fNumFiredStrips++;
    fStripEnergySumArray[side][strip] = energy;
}

void LKSiDetector::ClearFiredFlags()
{
    fNumFiredStrips = 0;
    for(int side=0; side<fNumSides; ++side) {
        if (side==0) { for(int strip=0; strip<fNumJunctionStrips; ++strip) fStripEnergySumArray[side][strip] = 0; }
        if (side==1) { for(int strip=0; strip<fNumOhmicStrips;    ++strip) fStripEnergySumArray[side][strip] = 0; }
    }
}

void LKSiDetector::ClearData()
{
    fChannelArray.Clear();
    for(int side=0; side<fNumSides; ++side)
    {
        if (side==0) {
            for(int strip=0; strip<fNumJunctionStrips; ++strip) {
                for(int lr=0; lr<fNumJunctionUD; ++lr) {
                    fIdxArray[side][strip][lr] = 0;
                    fCountArray[side][strip][lr] = 0;
                    fEnergyArray[side][strip][lr] = 0;
                    fEnergySumArray[side][strip][lr] = 0;
                }
            }
        }
        if (side==1) {
            for(int strip=0; strip<fNumOhmicStrips; ++strip) {
                for(int lr=0; lr<fNumOhmicLR; ++lr) {
                    fIdxArray[side][strip][lr] = 0;
                    fCountArray[side][strip][lr] = 0;
                    fEnergyArray[side][strip][lr] = 0;
                    fEnergySumArray[side][strip][lr] = 0;
                }
            }
        }
    }

    if (fHistJunction!=nullptr) {
        fHistJunction -> Reset();
        fHistOhmic    -> Reset();
    }
}

void LKSiDetector::SetSiPosition(TVector3 position, int layer, int row, double phi1, double phi2, double theta1, double theta2)
{
    SetPosition(position);
    SetLayer(layer);
    SetRow(row);
    SetPhi1(phi1);
    SetPhi2(phi2);
    SetTheta1(theta1);
    SetTheta2(theta2);
}

void LKSiDetector::SetChannel(GETChannel* channel, int side, int strip, int lr)
{
    int idx = fChannelArray.GetEntries();
    fChannelArray.Add(channel);
    double energy = channel -> GetEnergy();
    double integral = channel -> GetIntegral(-1, (side==0));
    fIdxArray[side][strip][lr] = idx;
    fCountArray[side][strip][lr] = 1;
    fEnergyArray[side][strip][lr] = energy;
    fEnergySumArray[side][strip][lr] = integral;
}

void LKSiDetector::AddChannel(GETChannel* channel, int side, int strip, int lr)
{
    double energy = channel -> GetEnergy();
    double integral = channel -> GetIntegral(-1, (side==0));
    fCountArray[side][strip][lr]++;
    fEnergyArray[side][strip][lr] += energy;
    fEnergySumArray[side][strip][lr] += integral;
}

void LKSiDetector::SetChannel(LKSiChannel* channel)
{
    if (channel -> GetDetID() == fDetID)
    {
        int idx = fChannelArray.GetEntries();
        fChannelArray.Add(channel);
        int side = channel -> GetSide();
        int strip = channel -> GetStrip();
        int lr = channel -> GetDirection();
        double energy = channel -> GetEnergy();
        double integral = channel -> GetIntegral(-1, (side==0));
        fIdxArray[side][strip][lr] = idx;
        fCountArray[side][strip][lr] = 1;
        fEnergyArray[side][strip][lr] = energy;
        fEnergySumArray[side][strip][lr] = integral;
    }
}

void LKSiDetector::AddChannel(LKSiChannel* channel)
{
    if (channel -> GetDetID() == fDetID)
    {
        int side = channel -> GetSide();
        int strip = channel -> GetStrip();
        int lr = channel -> GetDirection();
        double energy = channel -> GetEnergy();
        double integral = channel -> GetIntegral(-1, (side==0));
        fCountArray[side][strip][lr]++;
        fEnergyArray[side][strip][lr] += energy;
        fEnergySumArray[side][strip][lr] += integral;
    }
}

void LKSiDetector::AddEnergy(int side, int strip, int lr, double energy)
{
    fCountArray[side][strip][lr]++;
    fEnergyArray[side][strip][lr] += energy;
}

void LKSiDetector::FillHistEnergy()
{
    for(int side=0; side<fNumSides; ++side)
    {
        if (side==0) {
            for(int strip=0; strip<fNumJunctionStrips; ++strip) {
                for(int lr=0; lr<fNumJunctionUD; ++lr) {
                    fHistJunction -> SetBinContent(strip+1,lr+1,fEnergyArray[side][strip][lr]);
                    auto rgChannel = GetRegisteredChannel(side,strip,lr); if (rgChannel!=nullptr) rgChannel -> SetEnergy(fEnergyArray[side][strip][lr]);
                }
            }
        }
        if (side==1) {
            for(int strip=0; strip<fNumOhmicStrips; ++strip) {
                for(int lr=0; lr<fNumOhmicLR; ++lr) {
                    fHistOhmic -> SetBinContent(lr+1,strip+1,fEnergyArray[side][strip][lr]);
                    auto rgChannel = GetRegisteredChannel(side,strip,lr); if (rgChannel!=nullptr) rgChannel -> SetEnergy(fEnergyArray[side][strip][lr]);
                }
            }
        }
    }
}

void LKSiDetector::FillHistEnergySum()
{
    for(int side=0; side<fNumSides; ++side)
    {
        if (side==0) {
            for(int strip=0; strip<fNumJunctionStrips; ++strip) {
                for(int lr=0; lr<fNumJunctionUD; ++lr) {
                    fHistJunction -> SetBinContent(strip+1,lr+1,fEnergySumArray[side][strip][lr]);
                    auto rgChannel = GetRegisteredChannel(side,strip,lr); if (rgChannel!=nullptr) rgChannel -> SetEnergy(fEnergySumArray[side][strip][lr]);
                }
            }
        }
        if (side==1) {
            for(int strip=0; strip<fNumOhmicStrips; ++strip) {
                for(int lr=0; lr<fNumOhmicLR; ++lr) {
                    fHistOhmic -> SetBinContent(lr+1,strip+1,fEnergySumArray[side][strip][lr]);
                    auto rgChannel = GetRegisteredChannel(side,strip,lr); if (rgChannel!=nullptr) rgChannel -> SetEnergy(fEnergySumArray[side][strip][lr]);
                }
            }
        }
    }
}

void LKSiDetector::FillHistCount()
{
    for(int side=0; side<fNumSides; ++side)
    {
        if (side==0) {
            for(int strip=0; strip<fNumJunctionStrips; ++strip) {
                for(int lr=0; lr<fNumJunctionUD; ++lr) {
                    fHistJunction -> SetBinContent(strip+1,lr+1,fCountArray[side][strip][lr]);
                    auto rgChannel = GetRegisteredChannel(side,strip,lr); if (rgChannel!=nullptr) rgChannel -> SetEnergy(fCountArray[side][strip][lr]);
                }
            }
        }
        if (side==1) {
            for(int strip=0; strip<fNumOhmicStrips; ++strip) {
                for(int lr=0; lr<fNumOhmicLR; ++lr) {
                    fHistOhmic -> SetBinContent(lr+1,strip+1,fCountArray[side][strip][lr]);
                    auto rgChannel = GetRegisteredChannel(side,strip,lr); if (rgChannel!=nullptr) rgChannel -> SetEnergy(fCountArray[side][strip][lr]);
                }
            }
        }
    }
}

void LKSiDetector::FillHistValue(double value)
{
    for(int side=0; side<fNumSides; ++side)
    {
        if (side==0) {
            for(int strip=0; strip<fNumJunctionStrips; ++strip) {
                for(int lr=0; lr<fNumJunctionUD; ++lr) {
                    fHistJunction -> SetBinContent(strip+1,lr+1,value);
                    auto rgChannel = GetRegisteredChannel(side,strip,lr); if (rgChannel!=nullptr) rgChannel -> SetEnergy(value);
                }
            }
        }
        if (side==1) {
            for(int strip=0; strip<fNumOhmicStrips; ++strip) {
                for(int lr=0; lr<fNumOhmicLR; ++lr) {
                    fHistOhmic -> SetBinContent(lr+1,strip+1,value);
                    auto rgChannel = GetRegisteredChannel(side,strip,lr); if (rgChannel!=nullptr) rgChannel -> SetEnergy(value);
                }
            }
        }
    }
}

TString LKSiDetector::GetNameType() const
{
    TString nameType = fDetTypeName;
    if (nameType.IsNull()) nameType = Form("t%d",fDetType);
    else nameType = nameType + "_" + fDetID;
    nameType.ReplaceAll(" ","");
    return nameType;
}

TString LKSiDetector::GetTitleType() const
{
    TString titleType = fDetTypeName;
    if (titleType.IsNull()) titleType = Form("Type(%d)",fDetType);
    else titleType = titleType + "(" + fDetID + ")";
    return titleType;
}

TH2* LKSiDetector::CreateHistJunction(TString name, TString title, double x1, double x2, double y1, double y2, TString option)
{
    if (fHistJunction==nullptr)
    {
        if (name.IsNull())
            name = Form("hist_%s_i%d_junction",GetNameType().Data(),fDetID);
        if (title.IsNull())
            title = Form("%s ID(%d) Junction (%d,%d)",GetTitleType().Data(),fDetID,fNumJunctionStrips,fNumJunctionUD);
        if (x1==x2) {
            x1 = 0;
            x2 = fNumJunctionStrips;
            y1 = 0;
            y2 = fNumJunctionUD;
        }
        fHistJunction = new TH2D(name,title,fNumJunctionStrips,x1,x2,fNumJunctionUD, y1,y2);
        fHistJunction -> SetStats(0);
        int i=1;
        for (auto x=0; x<fNumJunctionStrips; ++x)
            for (auto y=0; y<fNumJunctionUD; ++y)
                fHistJunction -> SetBinContent(x+1,y+1,i++);

        bool drawAxis = true;
        if (option.Index("!axis")>=0) { drawAxis = false; option.ReplaceAll("!axis", ""); }
        if (option.Index( "axis")>=0) { drawAxis = true;  option.ReplaceAll("axis", ""); }
        if (drawAxis==false) {
            fHistJunction -> GetXaxis() -> SetTickSize(0);
            fHistJunction -> GetYaxis() -> SetTickSize(0);
            fHistJunction -> GetXaxis() -> SetBinLabel(1,"");
            fHistJunction -> GetYaxis() -> SetBinLabel(1,"");
        }
    }
    return fHistJunction;
}

TH2* LKSiDetector::CreateHistOhmic(TString name, TString title, double x1, double x2, double y1, double y2, TString option)
{
    if (fHistOhmic==nullptr)
    {
        if (name.IsNull())
            name = Form("hist_%s_i%d_junction",GetNameType().Data(),fDetID);
        if (title.IsNull())
            title = Form("%s ID(%d) Ohmic (%d,%d)",GetTitleType().Data(),fDetID,fNumOhmicStrips,fNumOhmicLR);
        if (x1==x2) {
            x1 = 0;
            x2 = fNumOhmicLR;
            y1 = 0;
            y2 = fNumOhmicStrips;
        }
        fHistOhmic = new TH2D(name,title,fNumOhmicLR,x1,x2,fNumOhmicStrips,y1,y2);
        fHistOhmic -> SetStats(0);
        int i=1;
        for (auto x=0; x<fNumOhmicLR; ++x)
            for (auto y=0; y<fNumOhmicStrips; ++y)
                fHistOhmic -> SetBinContent(x+1,y+1,i++);

        bool drawAxis = true;
        if (option.Index("!axis")>=0) { drawAxis = false; option.ReplaceAll("!axis", ""); }
        if (option.Index( "axis")>=0) { drawAxis = true;  option.ReplaceAll("axis", ""); }
        if (drawAxis==false) {
            fHistOhmic -> GetXaxis() -> SetTickSize(0);
            fHistOhmic -> GetYaxis() -> SetTickSize(0);
            fHistOhmic -> GetXaxis() -> SetBinLabel(1,"");
            fHistOhmic -> GetYaxis() -> SetBinLabel(1,"");
        }
    }
    return fHistOhmic;
}

void LKSiDetector::RegisterChannel(LKSiChannel* channel)
{
    auto idx = fRegisteredChannelArray.GetEntries();
    fRegisteredChannelArray.Add(channel);
    int side = channel -> GetSide();
    int strip = channel -> GetStrip();
    int lr = channel -> GetDirection();
    if (side>=0&&strip>=0&&lr>=0)
        fChannelIndexArray[side][strip][lr] = idx;
}

LKSiChannel* LKSiDetector::GetRegisteredChannel(int side, int strip, int lr)
{
    LKSiChannel* channel = nullptr;
    int idx = fChannelIndexArray[side][strip][lr];
    if (idx>=0)
        channel = (LKSiChannel*) fRegisteredChannelArray.At(idx);
    return channel;
}
