#include "TCanvas.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TRandom3.h"
#include "TMath.h"
#include "TString.h"
#include "TTree.h"

#include <algorithm>
#include <iostream>

using namespace std;

void make_dummy_data(int runID=25, int numEvents=100000, Long64_t timeWindow=4000000,
                      const char* outputName="data/dummry_run0025.root")
{
    const int numStrips = 16;
    const int y1Start = 0;
    const int x1Start = 16;
    const int y2Start = 32;
    const int x2Start = 48;
    const int comBoard = 2;
    const int numCom = 4;
    const Long64_t xyDtMean = timeWindow/100;
    const Long64_t xyDtSigma = timeWindow/500;

    UShort_t Channel = 0;
    ULong64_t Timestamp = 0;
    UShort_t Board = 0;
    UShort_t Energy = 0;
    UShort_t EnergyShort = 0;
    UInt_t Flags = 0;

    auto file = new TFile(outputName,"recreate");
    auto tree = new TTree("Data_R","CoMPASS RAW events TTree");
    tree -> Branch("Channel",&Channel,"Channel/s");
    tree -> Branch("Timestamp",&Timestamp,"Timestamp/l");
    tree -> Branch("Board",&Board,"Board/s");
    tree -> Branch("Energy",&Energy,"Energy/s");
    tree -> Branch("EnergyShort",&EnergyShort,"EnergyShort/s");
    tree -> Branch("Flags",&Flags,"Flags/i");

    auto hTrueYX1 = new TH2D("true_y_vs_x_detector1","Detector 1 true Y vs X;X strip;Y strip",16,1,17,16,1,17);
    auto hTrueYX2 = new TH2D("true_y_vs_x_detector2","Detector 2 true Y vs X;X strip;Y strip",16,1,17,16,1,17);
    auto hTimeDiff1 = new TH1D("time_difference_detector1","Detector 1 X-Y time difference;#Deltat (#mus);counts",200,0,timeWindow*0.1e-6);
    auto hTimeDiff2 = new TH1D("time_difference_detector2","Detector 2 X-Y time difference;#Deltat (#mus);counts",200,0,timeWindow*0.1e-6);
    auto hEventTimeDiff1 = new TH1D("adjacent_event_time_difference_detector1","Detector 1 adjacent event time difference;#Deltat (#mus);counts",200,0,timeWindow*20.e-6);
    auto hEventTimeDiff2 = new TH1D("adjacent_event_time_difference_detector2","Detector 2 adjacent event time difference;#Deltat (#mus);counts",200,0,timeWindow*20.e-6);
    auto hComChannel = new TH1D("com_channel","Com channel;channel;counts",numCom,0,numCom);
    auto hComEnergy = new TH1D("com_energy","Com energy;energy;counts",400,0,20000);
    auto hComEventTimeDiff = new TH1D("adjacent_event_time_difference_com","Com adjacent event time difference;#Deltat (#mus);counts",200,0,timeWindow*30.e-6);
    hTrueYX1 -> SetDirectory(0);
    hTrueYX2 -> SetDirectory(0);
    hTimeDiff1 -> SetDirectory(0);
    hTimeDiff2 -> SetDirectory(0);
    hEventTimeDiff1 -> SetDirectory(0);
    hEventTimeDiff2 -> SetDirectory(0);
    hComChannel -> SetDirectory(0);
    hComEnergy -> SetDirectory(0);
    hComEventTimeDiff -> SetDirectory(0);

    TRandom3 random(0);
    ULong64_t time = 0;
    ULong64_t previousEventTime1 = 0;
    ULong64_t previousEventTime2 = 0;
    ULong64_t previousComTime = 0;
    Long64_t numHits = 0;
    Long64_t numComHits = 0;

    for (auto iEvent=0; iEvent<numEvents; ++iEvent)
    {
        time += timeWindow*5 + random.Integer(timeWindow*10);

        auto x = (int) TMath::Nint(random.Gaus(7.5,2.0));
        auto y = (int) TMath::Nint(random.Gaus(7.5,2.0));
        x = max(0,min(numStrips-1,x));
        y = max(0,min(numStrips-1,y));
        hTrueYX1 -> Fill(x+1,y+1);

        auto energy = (int) random.Gaus(4000,350);
        energy = max(100,min(16000,energy));

        Board = 0;
        Flags = 0;

        auto yTime = time;
        auto xyDt = (Long64_t) TMath::Abs(random.Gaus(xyDtMean,xyDtSigma));
        xyDt = max((Long64_t)1,min(timeWindow/20,xyDt));
        auto xTime = time + xyDt;
        hTimeDiff1 -> Fill((xTime-yTime)*1.e-6);
        if (previousEventTime1>0)
            hEventTimeDiff1 -> Fill((yTime-previousEventTime1)*1.e-6);
        previousEventTime1 = yTime;

        Timestamp = yTime;
        Channel = y1Start + y;
        Energy = energy + random.Integer(100);
        EnergyShort = Energy*0.8;
        tree -> Fill();
        numHits++;

        Timestamp = xTime;
        Channel = x1Start + x;
        Energy = energy + random.Integer(100);
        EnergyShort = Energy*0.8;
        tree -> Fill();
        numHits++;

        if (random.Rndm()<0.35)
        {
            auto x2 = (int) TMath::Nint(random.Gaus(11.0,1.5));
            auto y2 = (int) TMath::Nint(random.Gaus(4.0,2.5));
            x2 = max(0,min(numStrips-1,x2));
            y2 = max(0,min(numStrips-1,y2));
            hTrueYX2 -> Fill(x2+1,y2+1);

            auto time2 = time + timeWindow*5 + random.Integer(timeWindow*10);
            auto energy2 = (int) random.Gaus(7500,900);
            energy2 = max(100,min(16000,energy2));

            Board = 1;
            auto yTime2 = time2;
            auto xyDt2 = (Long64_t) TMath::Abs(random.Gaus(xyDtMean*1.5,xyDtSigma*1.2));
            xyDt2 = max((Long64_t)1,min(timeWindow/20,xyDt2));
            auto xTime2 = time2 + xyDt2;
            hTimeDiff2 -> Fill((xTime2-yTime2)*1.e-6);
            if (previousEventTime2>0)
                hEventTimeDiff2 -> Fill((yTime2-previousEventTime2)*1.e-6);
            previousEventTime2 = yTime2;

            Timestamp = yTime2;
            Channel = y2Start + y2;
            Energy = energy2 + random.Integer(100);
            EnergyShort = Energy*0.8;
            tree -> Fill();
            numHits++;

            Timestamp = xTime2;
            Channel = x2Start + x2;
            Energy = energy2 + random.Integer(100);
            EnergyShort = Energy*0.8;
            tree -> Fill();
            numHits++;

            time = time2;
        }

        if (random.Rndm()<0.45)
        {
            time += timeWindow*5 + random.Integer(timeWindow*10);

            auto com = random.Integer(numCom);
            auto comEnergyMean = 2500 + 1800*com;
            auto comEnergy = (int) random.Gaus(comEnergyMean,350 + 80*com);
            comEnergy = max(100,min(16000,comEnergy));

            Board = comBoard;
            Timestamp = time;
            Channel = com;
            Energy = comEnergy;
            EnergyShort = Energy*0.75;
            Flags = 0;
            tree -> Fill();
            numHits++;
            numComHits++;

            hComChannel -> Fill(com);
            hComEnergy -> Fill(Energy);
            if (previousComTime>0)
                hComEventTimeDiff -> Fill((Timestamp-previousComTime)*1.e-6);
            previousComTime = Timestamp;
        }
    }

    tree -> Write();
    hTrueYX1 -> Write();
    hTrueYX2 -> Write();
    hTimeDiff1 -> Write();
    hTimeDiff2 -> Write();
    hEventTimeDiff1 -> Write();
    hEventTimeDiff2 -> Write();
    hComChannel -> Write();
    hComEnergy -> Write();
    hComEventTimeDiff -> Write();
    file -> Close();

    TString pngName = outputName;
    pngName.ReplaceAll(".root","_truth_summary.png");
    auto cvs = new TCanvas("cvs_truth_summary","",1500,1200);
    cvs -> Divide(3,3);
    cvs -> cd(1); hTrueYX1 -> Draw("colz");
    cvs -> cd(2); hTrueYX2 -> Draw("colz");
    cvs -> cd(3); hTimeDiff1 -> Draw();
    cvs -> cd(4); hTimeDiff2 -> Draw();
    cvs -> cd(5); hEventTimeDiff1 -> Draw();
    cvs -> cd(6); hEventTimeDiff2 -> Draw();
    cvs -> cd(7); hComChannel -> Draw();
    cvs -> cd(8); hComEnergy -> Draw();
    cvs -> cd(9); hComEventTimeDiff -> Draw();
    cvs -> SaveAs(pngName);

    cout << "Created " << outputName << endl;
    cout << "Truth summary figure: " << pngName << endl;
    cout << "Events: " << numEvents << endl;
    cout << "Hits: " << numHits << endl;
    cout << "Com hits: " << numComHits << endl;
    cout << "Detector 1 Y channels: " << y1Start << " - " << y1Start+numStrips-1 << endl;
    cout << "Detector 1 X channels: " << x1Start << " - " << x1Start+numStrips-1 << endl;
    cout << "Detector 2 Y channels: " << y2Start << " - " << y2Start+numStrips-1 << endl;
    cout << "Detector 2 X channels: " << x2Start << " - " << x2Start+numStrips-1 << endl;
    cout << "Com channels: Board " << comBoard << ", Channel 0 - " << numCom-1 << endl;
}
