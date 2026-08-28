#include "TCanvas.h"
#include "TError.h"
#include "TH1D.h"
#include "TH2D.h"
#include "THttpServer.h"
#include "TRandom3.h"
#include "TROOT.h"
#include "TSystem.h"

namespace {
THttpServer* gLiveServer = nullptr;
TCanvas* gLiveCanvas = nullptr;
TH1D* gEnergy = nullptr;
TH2D* gEnergyVsTime = nullptr;
}

void close_live_jsroot()
{
    if (gLiveServer != nullptr) {
        delete gLiveServer;
        gLiveServer = nullptr;
    }

    delete gLiveCanvas;
    gLiveCanvas = nullptr;

    delete gEnergy;
    gEnergy = nullptr;

    delete gEnergyVsTime;
    gEnergyVsTime = nullptr;
}

void live_jsroot(
        int port = 9091,
        Long64_t numEvents = 0,
        int updateEvery = 100)
{
    close_live_jsroot();

    if (updateEvery < 1)
        updateEvery = 1;

    // Create canvases in memory without opening a native ROOT window.
    gROOT->SetBatch(kTRUE);

    gEnergy = new TH1D(
            "energy",
            "Live energy;Energy;Counts",
            400, 0, 4000);
    gEnergy->SetDirectory(nullptr);

    gEnergyVsTime = new TH2D(
            "energy_vs_time",
            "Live energy vs. time;Event time;Energy",
            200, 0, 2000,
            200, 0, 4000);
    gEnergyVsTime->SetDirectory(nullptr);

    gLiveCanvas = new TCanvas(
            "live_canvas", "LILAK live monitor", 1200, 600);
    gLiveCanvas->Divide(2, 1);
    gLiveCanvas->cd(1);
    gEnergy->Draw();
    gLiveCanvas->cd(2);
    gEnergyVsTime->Draw("colz");

    gLiveServer = new THttpServer(
            Form("http:%d?loopback&top=LILAK", port));

    if (!gLiveServer->IsAnyEngine()) {
        Error("live_jsroot", "cannot start HTTP server on port %d", port);
        close_live_jsroot();
        return;
    }

    // THttpServer keeps pointers to these live in-memory objects. Every Fill()
    // below therefore becomes visible on the next JSROOT monitoring request.
    gLiveServer->Register("Live", gEnergy);
    gLiveServer->Register("Live", gEnergyVsTime);
    gLiveServer->Register("Live", gLiveCanvas);
    gLiveServer->SetItemField("/", "_monitoring", "1000");

    Info("live_jsroot", "open http://localhost:%d", port);
    Info("live_jsroot", "open Objects/Live and double-click an object");
    Info("live_jsroot", "numEvents=0 runs until interrupted with Ctrl-C");

    TRandom3 random(0);
    Long64_t eventNumber = 0;

    while (numEvents <= 0 || eventNumber < numEvents) {
        // Replace this block with values from the real analysis event.
        const double eventTime = eventNumber % 2000;
        const double energy = random.Gaus(1500, 180)
                            + (random.Rndm() < 0.15 ? random.Gaus(900, 80) : 0);

        gEnergy->Fill(energy);
        gEnergyVsTime->Fill(eventTime, energy);
        ++eventNumber;

        if (eventNumber % updateEvery == 0) {
            gLiveCanvas->Modified();
            gLiveCanvas->Update();

            // THttpServer handles queued browser requests on the ROOT thread.
            // ProcessEvents() also returns true after an interactive interrupt.
            if (gSystem->ProcessEvents())
                break;

            // Slow the demo enough that the live update is easy to observe.
            gSystem->Sleep(10);
        }
    }

    Info("live_jsroot", "processed %lld events", eventNumber);
    Info("live_jsroot", "objects remain available; call close_live_jsroot() to stop");
}
